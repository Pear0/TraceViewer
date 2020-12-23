//
// Created by Will Gulian on 11/26/20.
//

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fcntl.h>
#include <unistd.h>

#include "DwarfInfo.h"
#include "libdwarf.h"
#include "dwarf.h"
#include "libelf/libelf.h"
#include "libelf/gelf.h"

namespace {

struct DieStackEntry {
  Dwarf_Half tag = 0;
  Dwarf_Off die_offset = 0;
  const char *name = nullptr;

  std::vector<Dwarf_Half> attribute_codes{};

  ConcreteFunctionInfo *function = nullptr;
  InlinedFunctionInfo *inlined_function = nullptr;
};

void print_ranges(const std::vector<std::pair<uint64_t, uint64_t>> &ranges) {
  std::cout << "[";

  bool comma = false;
  for (auto &pair : ranges) {
    if (comma) {
      std::cout << ", ";
    }
    comma = true;
    std::cout << "0x" << std::hex << pair.first << std::dec;
    std::cout << "-";
    std::cout << "0x" << std::hex << pair.second << std::dec;
  }

  std::cout << "]";
}

bool compare_ranges(const std::vector<std::pair<uint64_t, uint64_t>> &a,
                    const std::vector<std::pair<uint64_t, uint64_t>> &b) {
  if (a.empty() && b.empty())
    return false;
  if (a.empty())
    return false;
  if (b.empty())
    return true;

  auto a_begin = a.begin()->first + 1;
  auto a_end = a.rbegin()->second;

  auto b_begin = b.begin()->first + 1;
  auto b_end = b.rbegin()->second;

  // any overlap, incomparable
  if (a_begin < b_end && b_begin < a_end)
    return a_begin < b_begin;

  if (a_end < b_begin)
    return true;
  if (b_end < a_begin)
    return false;

  return false;
}

bool ranges_contains_address(const std::vector<std::pair<uint64_t, uint64_t>> &ranges, uint64_t addr) {
  for (auto[low, high] : ranges) {
    if (low <= addr && addr < high)
      return true;
  }
  return false;
}

struct CompilationUnitHeader {
  Dwarf_Unsigned header_length;
  Dwarf_Half version_stamp;
  Dwarf_Unsigned abbrev_offset;
  Dwarf_Half address_size;
  Dwarf_Half offset_size;
  Dwarf_Half extension_size;
  Dwarf_Sig8 signature;
  Dwarf_Unsigned type_offset;
  Dwarf_Unsigned next_cu_header;
  Dwarf_Half header_cu_type;
};

struct CompilationUnitInfo {
  Dwarf_Off range_lists_base = 0;

  // this is only valid if has_ranges_attr is true.
  Dwarf_Addr default_base_address = 0;
  bool has_ranges_attr = false;
};

}





/* Adding return codes to DW_DLV, relevant to our purposes here. */
#define NOT_THIS_CU 10
#define IN_THIS_CU 11
#define FOUND_SUBPROG 12

#define TRUE 1
#define FALSE 0

#define NT_GNU_BUILD_ID 3

#define UNUSEDARG __attribute__((unused))

static int
get_name_from_abstract_origin(Dwarf_Debug dbg,
                              int is_info,
                              Dwarf_Die die,
                              char **name,
                              Dwarf_Error *errp);

static int
getlowhighpc(Dwarf_Die die,
             int *have_pc_range,
             Dwarf_Addr *lowpc_out,
             Dwarf_Addr *highpc_out,
             Dwarf_Error *error);


void InlinedFunctionInfo::dump() {
  for (int i = 0; i < depth; i++)
    std::cout << "  ";
  std::cout << "  Inlined ";
  if (origin_fn) {
    std::cout << origin_fn->full_name.toStdString() << "\n";
  } else {
    std::cout << "(no name)\n";
  }

  if (!ranges.empty()) {
    for (int i = 0; i < depth; i++)
      std::cout << "  ";
    std::cout << "  Ranges: ";
    print_ranges(ranges);
    std::cout << "\n";
  }

  if (!inline_functions.empty()) {
    for (int i = 0; i < depth; i++)
      std::cout << "  ";
    std::cout << "  Inlines (" << inline_functions.size() << "):\n";
    for (auto &i : inline_functions) {
      i->dump();
    }
  }
}

QString InlinedFunctionInfo::getFullName() {
  return origin_fn->getFullName();
}

void ConcreteFunctionInfo::dump() {
  std::cout << full_name.toStdString() << "\n";

  if (!ranges.empty()) {
    std::cout << "  Ranges: ";
    print_ranges(ranges);
    std::cout << "\n";
  }

  if (!inline_functions.empty()) {
    std::cout << "  Inlines (" << inline_functions.size() << "):\n";
    for (auto &i : inline_functions) {
      i->dump();
    }
  }
}

struct DwarfInfo::Internal {
  int fd = 0;
  Elf *elf = nullptr;
};

DwarfInfo::DwarfInfo(
        QString &&file, std::vector<uint8_t> &&buildId, std::vector<std::unique_ptr<ConcreteFunctionInfo>> &&functions,
        DwarfInfo::LineTable &&lineTable)
        : file(std::move(file)), loaded_time(std::time(nullptr)), buildId(std::move(buildId)),
          functions(std::move(functions)), lineTable(std::move(lineTable)), internal(std::make_unique<Internal>()) {}


DwarfInfo::~DwarfInfo() {

//  int res = dwarf_finish(debug_info, &error);
//  if (res != DW_DLV_OK) {
//    std::cout << "dwarf_finish failed!" << std::endl;
//  }

// TODO cleanup dwarf

  if (internal && internal->fd >= 0) {
    close(internal->fd);
    internal->fd = -1;
  }
}

DwarfInfo::DwarfInfo(DwarfInfo &&other) = default;

static void dwarf_load_err(Dwarf_Error error, Dwarf_Ptr arg) {
  std::cout << "dwarf_load_err() called" << std::endl;
}

static size_t align_up(size_t value, size_t align) {
  auto mod = value % align;
  return mod ? (value - mod + align) : value;
}


constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static std::string bytesToStr(const uint8_t *data, int len) {
  std::string s(len * 2, ' ');
  for (int i = 0; i < len; ++i) {
    s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
    s[2 * i + 1] = hexmap[data[i] & 0x0F];
  }
  return s;
}

static std::vector<uint8_t> find_build_id(Elf *elf) {
  Elf_Scn *scn = nullptr;
  GElf_Shdr shdr;
  Elf_Data *data;

  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    gelf_getshdr(scn, &shdr);
    if (shdr.sh_type == SHT_NOTE) {

      data = elf_getdata(scn, NULL);
      auto buf = (size_t) data->d_buf;
      auto buf_end = buf + data->d_size;

      while (buf + sizeof(Elf64_Nhdr) < buf_end) {
        Elf64_Nhdr note = *(Elf64_Nhdr *) buf;
        // we only care about "GNU\0" namespace

        uint8_t *name = note.n_namesz == 0 ? nullptr : (uint8_t *) (buf + sizeof(note));

        uint8_t *desc = note.n_namesz == 0 ? nullptr : (uint8_t *) (buf + sizeof(note) + align_up(note.n_namesz, 4));

        // set buf to next note
        buf = buf + sizeof(note) + align_up(note.n_namesz, 4) + align_up(note.n_descsz, 4);

        // we overflowed, and our name and/or descriptors are not actually valid...
        if (buf > buf_end) {
          break;
        }

        if (note.n_namesz != 4 || memcmp(name, "GNU\0", 4) != 0) {
          continue;
        }

        if (note.n_type != NT_GNU_BUILD_ID) {
          continue;
        }

        std::vector<uint8_t> vec;
        vec.reserve(note.n_descsz);
        for (size_t i = 0; i < note.n_descsz; i++)
          vec.push_back(desc[i]);
        return std::move(vec);
      }

    }
  }

  return std::vector<uint8_t>();
}

std::optional<std::pair<ConcreteFunctionInfo *, std::vector<InlinedFunctionInfo *>>>
DwarfInfo::resolve_address(uint64_t address) {

  auto func_it = std::lower_bound(functions.cbegin(), functions.cend(), address, [](const auto &func, const auto addr) {
    if (func->ranges.empty())
      return false;
    return func->ranges.rbegin()->second <= addr;
  });

  while (true) {
    // address too high
    // or we hit a function that doesnt exist in the binary
    // or the lowest func starts above our address...
    // this is an exit condition.
    if (func_it == functions.cend() || (*func_it)->ranges.empty() || address < (*func_it)->ranges.cbegin()->first) {
      return {};
    }

    // address is not in our range, try next function.
    if (!ranges_contains_address((*func_it)->ranges, address)) {
      func_it++;
      continue;
    }

    std::vector<InlinedFunctionInfo *> inline_stack;

    auto *inlined_functions = &(*func_it)->inline_functions;
    bool fresh = true;
    while (fresh) {
      fresh = false;
      for (auto &inline_func : *inlined_functions) {
        if (!ranges_contains_address(inline_func->ranges, address))
          continue;
        if (inline_stack.empty())
          inline_stack.reserve(8);
        inline_stack.push_back(inline_func.get());
        inlined_functions = &inline_func->inline_functions;
        fresh = true;
        break;
      }
    }

    // FIXME hack
    func_it->get()->buildId = getBuildId();

    return {std::make_pair(func_it->get(), std::move(inline_stack))};
  }
}

std::pair<QString, bool> DwarfInfo::symbolicate(uint64_t address) {
  auto funcs = resolve_address(address);
  if (funcs) {
    auto &[main_func, inlines] = *funcs;
    return std::make_pair(main_func->full_name, true);
  }

  std::cout << "failed to symbolicate address = 0x" << std::hex << address << std::dec << std::endl;
  QString name("0x");
  name += QString::number(address, 16);
  return std::make_pair(name, false);
}

static QString handle_error(Dwarf_Debug debug_info, Dwarf_Error error) {
  QString msg;
  msg += "error ";
  msg += QString::number(dwarf_errno(error));
  msg += ": ";
  msg += dwarf_errmsg(error);
  dwarf_dealloc_error(debug_info, error);
  return std::move(msg);
}

#define TRY_ERROR(type, debugInfo, res, error) do {if ((res) == DW_DLV_ERROR)return Result<type, QString>::with_error(handle_error(debugInfo, error));}while(0)


struct DieWalkInfo {
  Dwarf_Debug debug_info;
  bool is_info;

  // callback "args"
  int level;
  int sibling_index;
};

template<typename Func>
static Result<int, QString>
doThing(Dwarf_Debug debugInfo, DieWalkInfo &info, Dwarf_Die die, Func func, int level = 0, int sibling_index = 0) {
  info.level = level;
  info.sibling_index = sibling_index;
  func(info, die, false);

  Dwarf_Die child_die;
  Dwarf_Error error;
  int res = dwarf_child(die, &child_die, &error);
  TRY_ERROR(int, debugInfo, res, error);
  if (res == DW_DLV_OK) {
    auto result = doThing(debugInfo, info, child_die, func, level + 1, 0);

    dwarf_dealloc_die(child_die);
    child_die = nullptr;

    if (!result.is_ok()) {
      return std::move(result);
    }
  }

  info.level = level;
  info.sibling_index = sibling_index;
  func(info, die, true);

  if (sibling_index == 0) {
    bool our_die = false;
    for (int i = 1;; i++) {
      Dwarf_Die next_die;
      res = dwarf_siblingof_b(info.debug_info, die, info.is_info, &next_die, &error);
      TRY_ERROR(int, debugInfo, res, error);
      if (res == DW_DLV_NO_ENTRY)
        break;

      if (our_die) {
        dwarf_dealloc_die(die);
        die = nullptr;
      }

      die = next_die;
      our_die = true;

      auto result = doThing(debugInfo, info, die, func, level, i);

      if (!result.is_ok()) {
        return std::move(result);
      }
    }
  }

  return Result<int, QString>::with_value(0);
}

template<typename Func>
static int forEachAttribute(Dwarf_Debug debug_info, Dwarf_Die die, Dwarf_Error *error, Func func) {
  Dwarf_Signed atcount = 0;
  Dwarf_Attribute *atlist = nullptr;

  int res = dwarf_attrlist(die, &atlist, &atcount, error);
  if (res == DW_DLV_ERROR)
    return res;

  for (Dwarf_Signed i = 0; i < atcount; ++i) {
    func(i, atlist[i]);
    dwarf_dealloc(debug_info, atlist[i], DW_DLA_ATTR);
  }
  dwarf_dealloc(debug_info, atlist, DW_DLA_LIST);

  return res;
}


std::vector<std::pair<uint64_t, uint64_t>>
build_ranges(Dwarf_Debug debug_info, CompilationUnitInfo &cu_info, Dwarf_Die die) {
  Dwarf_Error error;
  Dwarf_Attribute at_ranges = nullptr;

  int res = dwarf_attr(die, DW_AT_ranges, &at_ranges, &error);
  assert(res != DW_DLV_ERROR);

  if (at_ranges) {
    // use AT_ranges
    Dwarf_Off range_offset = 0;
    res = dwarf_global_formref(at_ranges, &range_offset, &error);
    assert(res == DW_DLV_OK);

    range_offset += cu_info.range_lists_base;

    Dwarf_Ranges *ranges;
    Dwarf_Signed ranges_count;
    Dwarf_Unsigned byte_count;
    Dwarf_Signed i = 0;
    Dwarf_Addr baseaddr = cu_info.default_base_address;

    res = dwarf_get_ranges_a(debug_info, range_offset, die, &ranges, &ranges_count, &byte_count, &error);
    assert(res != DW_DLV_ERROR);

    std::vector<std::pair<uint64_t, uint64_t>> ranges_vector;

    for (i = 0; i < ranges_count; ++i) {
      Dwarf_Ranges *cur = &ranges[i];
      switch (cur->dwr_type) {
        case DW_RANGES_ENTRY: {
          Dwarf_Addr lowpc = cur->dwr_addr1 + baseaddr;
          Dwarf_Addr highpc = cur->dwr_addr2 + baseaddr;

          ranges_vector.emplace_back(lowpc, highpc);
          break;
        }
        case DW_RANGES_ADDRESS_SELECTION:
          baseaddr = cur->dwr_addr2;
          break;
        case DW_RANGES_END:
          i = ranges_count; // done loop
          break;
        default:
          printf("Impossible debug_ranges content!"
                 " enum val %d \n", (int) cur->dwr_type);
          assert(0);
      }
    }
    dwarf_ranges_dealloc(debug_info, ranges, ranges_count);

    std::sort(ranges_vector.begin(), ranges_vector.end(), [](auto &a, auto &b) {
      if (a.second < b.first)
        return -1;
      if (a.first > b.second)
        return 1;
      return 0;
    });

    return ranges_vector;
  } else {
    int found_range;
    Dwarf_Addr low_pc;
    Dwarf_Addr high_pc;

    res = getlowhighpc(die, &found_range, &low_pc, &high_pc, &error);
    if (res == DW_DLV_OK && found_range) {
      return {std::pair(low_pc, high_pc)};
    } else {
      return {};
    }
  }
}

static Result<std::vector<std::unique_ptr<ConcreteFunctionInfo>>, QString> scan_debug_info(Dwarf_Debug debug_info) {
  CompilationUnitHeader header{};
  bool is_info = true;
  constexpr auto dump_tags = false;

  std::vector<std::unique_ptr<ConcreteFunctionInfo>> functions;
  Dwarf_Error error;

  for (int cu_index = 0;; cu_index++) {
    // iterate through the compilation units...
    // returns NO_ENTRY and resets state after progressing all CUs

    int res = dwarf_next_cu_header_d(
            debug_info, is_info, &header.header_length, &header.version_stamp,
            &header.abbrev_offset, &header.address_size, &header.offset_size,
            &header.extension_size, &header.signature, &header.type_offset,
            &header.next_cu_header, &header.header_cu_type, &error);
    TRY_ERROR(std::vector<std::unique_ptr<ConcreteFunctionInfo>>, debug_info, res, error);

    if (res == DW_DLV_NO_ENTRY)
      break; // no more entries, exit.

    Dwarf_Die cu_die;
    res = dwarf_siblingof_b(debug_info, nullptr, is_info, &cu_die, &error);
    TRY_ERROR(std::vector<std::unique_ptr<ConcreteFunctionInfo>>, debug_info, res, error);
    if (res == DW_DLV_NO_ENTRY)
      continue;

    CompilationUnitInfo cu_info;

    res = forEachAttribute(debug_info, cu_die, &error, [&](auto i, Dwarf_Attribute attr) {
      Dwarf_Half at_code;
      int res = dwarf_whatattr(attr, &at_code, &error);
      assert(res == DW_DLV_OK);

      switch (at_code) {
        case DW_AT_rnglists_base:
        case DW_AT_GNU_ranges_base: {
          res = dwarf_global_formref(attr, &cu_info.range_lists_base, &error);
          assert(res == DW_DLV_OK);
          break;
        }
        case DW_AT_low_pc: {
          res = dwarf_lowpc(cu_die, &cu_info.default_base_address, &error);
          assert(res == DW_DLV_OK);
          break;
        }
        case DW_AT_ranges: {
          cu_info.has_ranges_attr = true;
          break;
        }
        default:
          break;
      }
    });
    assert(res != DW_DLV_ERROR);

    if (!cu_info.has_ranges_attr) {
      cu_info.default_base_address = 0;
    }

    std::vector<DieStackEntry> die_stack;

    DieWalkInfo walkInfo{};
    walkInfo.debug_info = debug_info;
    walkInfo.is_info = is_info;

    if (dump_tags) {
      doThing(debug_info, walkInfo, cu_die, [&](DieWalkInfo &info, Dwarf_Die die, bool is_exit) {
        Dwarf_Error err;

        Dwarf_Off offset;
        res = dwarf_dieoffset(die, &offset, &err);
        assert(res == DW_DLV_OK);

        Dwarf_Half tag;
        res = dwarf_tag(die, &tag, &err);
        assert(res == DW_DLV_OK);

        if (is_exit) {
          return;
        }

        DieStackEntry die_info;
        die_info.tag = tag;
        die_info.die_offset = offset;

        const char *tag_name;
        dwarf_get_TAG_name(tag, &tag_name);

        for (int i = 0; i < info.level; i++)
          std::cout << " ";
        std::cout << tag_name;

        std::cout << " [";

        res = forEachAttribute(debug_info, die, &err, [&](auto i, Dwarf_Attribute attr) {
          if (i > 0)
            std::cout << ", ";

          Dwarf_Half at_code;
          dwarf_whatattr(attr, &at_code, &err);

          die_info.attribute_codes.push_back(at_code);

          const char *at_name = "";
          dwarf_get_AT_name(at_code, &at_name);

          std::cout << at_name;
        });
        assert(res != DW_DLV_ERROR);

        std::cout << "]\n";

        char *attr_name = nullptr;
        res = dwarf_diename(die, &attr_name, &err);
        if (res == DW_DLV_NO_ENTRY) {
          get_name_from_abstract_origin(debug_info, is_info, die, &attr_name, &err);
        }

        die_info.name = attr_name;

        for (int j = 0; j < info.level + 2; j++)
          std::cout << " ";
        std::cout << "AT_name = " << (attr_name ? attr_name : "") << "\n";

      });
    }

    doThing(debug_info, walkInfo, cu_die, [&](DieWalkInfo &info, Dwarf_Die die, bool is_exit) {
      Dwarf_Error err;

      Dwarf_Off offset;
      res = dwarf_dieoffset(die, &offset, &err);
      assert(res == DW_DLV_OK);

      Dwarf_Half tag;
      res = dwarf_tag(die, &tag, &err);
      assert(res == DW_DLV_OK);

      switch (tag) {
        case DW_TAG_compile_unit:
        case DW_TAG_namespace:
        case DW_TAG_structure_type:
        case DW_TAG_union_type:
        case DW_TAG_class_type:
        case DW_TAG_enumeration_type:
        case DW_TAG_subprogram:
        case DW_TAG_inlined_subroutine:
          break;
        default:
          return;
      }

      if (is_exit) {
        die_stack.pop_back();
        return;
      }

      DieStackEntry die_info;
      die_info.tag = tag;
      die_info.die_offset = offset;

      const char *tag_name;
      dwarf_get_TAG_name(tag, &tag_name);

      if (dump_tags) {
        for (int i = 0; i < info.level; i++)
          std::cout << " ";
        std::cout << tag_name;
      }

      if (dump_tags) {
        std::cout << " [";
      }

      res = forEachAttribute(debug_info, die, &err, [&](auto i, Dwarf_Attribute attr) {
        if (dump_tags) {
          if (i > 0)
            std::cout << ", ";
        }

        Dwarf_Half at_code;
        dwarf_whatattr(attr, &at_code, &err);

        die_info.attribute_codes.push_back(at_code);

        if (dump_tags) {
          const char *at_name = "";
          dwarf_get_AT_name(at_code, &at_name);

          std::cout << at_name;
        }
      });
      assert(res == DW_DLV_OK);

      if (dump_tags) {
        std::cout << "]\n";
      }

      char *attr_name = nullptr;
      res = dwarf_diename(die, &attr_name, &err);
      if (res == DW_DLV_NO_ENTRY) {
        get_name_from_abstract_origin(debug_info, is_info, die, &attr_name, &err);
      }

      die_info.name = attr_name;

      if (dump_tags) {
        for (int j = 0; j < info.level + 2; j++)
          std::cout << " ";
        std::cout << "AT_name = " << (attr_name ? attr_name : "") << "\n";
      }

      auto ranges = build_ranges(debug_info, cu_info, die);

      switch (tag) {
        case DW_TAG_subprogram: {
          QString full_name;
          for (auto &ns : die_stack) {
            switch (ns.tag) {
              case DW_TAG_namespace:
              case DW_TAG_structure_type:
              case DW_TAG_union_type:
              case DW_TAG_class_type:
              case DW_TAG_enumeration_type:
                full_name += ns.name;
                full_name += "::";
              default:
                break;
            }
          }

          full_name += die_info.name;

          auto function = std::make_unique<ConcreteFunctionInfo>();
          function->name = die_info.name;
          function->die_offset = die_info.die_offset;
          function->full_name = full_name;
          function->ranges = ranges;

          die_info.function = function.get();
          functions.push_back(std::move(function));
          break;
        }
        case DW_TAG_inlined_subroutine: {

          Dwarf_Attribute ab_attr = nullptr;
          Dwarf_Off ab_offset = 0;

          res = dwarf_attr(die, DW_AT_abstract_origin, &ab_attr, &err);
          assert(res == DW_DLV_OK);

          res = dwarf_global_formref(ab_attr, &ab_offset, &err);
          assert(res == DW_DLV_OK);

          int depth = 1;
          for (auto &entry : die_stack) {
            if (entry.tag == DW_TAG_inlined_subroutine)
              depth++;
          }

          auto function = std::make_unique<InlinedFunctionInfo>();
          function->die_offset = die_info.die_offset;
          function->depth = depth;
          function->origin_offset = ab_offset;
          function->ranges = ranges;

          die_info.inlined_function = function.get();

          assert(!die_stack.empty());
          auto &parent = *die_stack.rbegin();
          if (parent.tag == DW_TAG_subprogram)
            parent.function->inline_functions.push_back(std::move(function));
          else if (parent.tag == DW_TAG_inlined_subroutine)
            parent.inlined_function->inline_functions.push_back(std::move(function));
          else
            assert(0 && "unexpected tag");

          break;
        }
        default:
          break;
      }

      die_stack.push_back(die_info);

    });
  }

  {
    // sort functions for binary search later

    std::sort(functions.begin(), functions.end(), [](const auto &a, const auto &b) {
      return compare_ranges(a->ranges, b->ranges);
    });
  }

  {
    // link all the inline functions to their definition

    std::unordered_map<Dwarf_Off, ConcreteFunctionInfo *> func_lookup;
    for (auto &func : functions) {
      func_lookup[func->die_offset] = func.get();
    }

    std::vector<InlinedFunctionInfo *> inline_func_stack;
    for (auto &func : functions) {
      for (auto &inline_func : func->inline_functions) {
        inline_func_stack.push_back(inline_func.get());
      }
    }

    while (!inline_func_stack.empty()) {
      auto ptr = *inline_func_stack.rbegin();
      inline_func_stack.pop_back();

      auto it = func_lookup.find(ptr->origin_offset);
      if (it != func_lookup.end())
        ptr->origin_fn = it->second;

      for (auto &inline_func : ptr->inline_functions) {
        inline_func_stack.push_back(inline_func.get());
      }
    }

  }

//  for (auto &func : functions) {
//    func->dump();
//  }

  return Result<std::vector<std::unique_ptr<ConcreteFunctionInfo>>, QString>::with_value(std::move(functions));
}

static Result<DwarfInfo::LineTable, QString> generate_line_table(Dwarf_Debug debug_info) {
  CompilationUnitHeader header{};
  bool is_info = true;

  DwarfInfo::LineTable lineTable;
  Dwarf_Error error;

  for (int cu_index = 0;; cu_index++) {
    // iterate through the compilation units...
    // returns NO_ENTRY and resets state after progressing all CUs

    int res = dwarf_next_cu_header_d(
            debug_info, is_info, &header.header_length, &header.version_stamp,
            &header.abbrev_offset, &header.address_size, &header.offset_size,
            &header.extension_size, &header.signature, &header.type_offset,
            &header.next_cu_header, &header.header_cu_type, &error);
    TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);

    if (res == DW_DLV_NO_ENTRY)
      break; // no more entries, exit.

    Dwarf_Die cu_die;
    res = dwarf_siblingof_b(debug_info, nullptr, is_info, &cu_die, &error);
    TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);
    if (res == DW_DLV_NO_ENTRY)
      continue;

    Dwarf_Unsigned line_version = 0;
    Dwarf_Small table_type = 0;
    Dwarf_Line_Context line_context = nullptr;

    res = dwarf_srclines_b(cu_die, &line_version,
                           &table_type, &line_context, &error);
    TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);

    if (table_type != 1) {
      std::cout << std::dec << "unexpected table_type = " << (int) table_type << ", line_version = " << line_version
                << std::endl;
      continue;
    }

    char **srcFiles = nullptr;
    Dwarf_Signed srcFilesCount = 0;

    res = dwarf_srcfiles(cu_die, &srcFiles, &srcFilesCount, &error);
    if (res != DW_DLV_OK) {
      return Result<DwarfInfo::LineTable, QString>::with_error(handle_error(debug_info, error));
    }

    size_t fileIndexOffset = lineTable.files.size();

    for (Dwarf_Signed j = 0; j < srcFilesCount; j++) {
      char *name = srcFiles[j];
      // std::cout << "file: " << name << std::endl;

      lineTable.files.push_back(DwarfInfo::LineFileInfo{QString(name), 0});

      dwarf_dealloc(debug_info, name, DW_DLA_STRING);
    }

    dwarf_dealloc(debug_info, srcFiles, DW_DLA_LIST);
    srcFiles = nullptr;

    Dwarf_Line *linebuf = 0;
    Dwarf_Signed linecount = 0;

    res = dwarf_srclines_from_linecontext(line_context, &linebuf, &linecount, &error);
    if (res != DW_DLV_OK) {
      dwarf_srclines_dealloc_b(line_context);
      line_context = 0;
      return Result<DwarfInfo::LineTable, QString>::with_error(handle_error(debug_info, error));
    }

    for (int i = 0; i < linecount; ++i) {
      Dwarf_Addr lineaddr = 0;
      Dwarf_Unsigned filenum = 0;
      Dwarf_Unsigned lineno = 0;

      res = dwarf_lineno(linebuf[i], &lineno, &error);
      TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);

      res = dwarf_line_srcfileno(linebuf[i], &filenum, &error);
      TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);

      assert(filenum != 0 && "filenum was zero");
      if (filenum) {
        filenum -= 1;
      }
      res = dwarf_lineaddr(linebuf[i], &lineaddr, &error);
      TRY_ERROR(DwarfInfo::LineTable, debug_info, res, error);

      int32_t fileIndex = -1;
      if (fileIndexOffset + filenum < lineTable.files.size()) {
        fileIndex = (int32_t) (fileIndexOffset + filenum);
      }

      lineTable.lines.push_back(DwarfInfo::LineInfo{lineaddr, fileIndex, (uint32_t) lineno});
    }

    dwarf_srclines_dealloc_b(line_context);
  }

  std::stable_sort(lineTable.lines.begin(), lineTable.lines.end(), [](auto &a, auto &b) {
    return a.address < b.address;
  });

//  for (auto &addr : lineTable.lines) {
//    auto *file = addr.fileIndex >= 0 ? &lineTable.files[addr.fileIndex] : nullptr;
//    std::cout << "address = 0x" << std::hex << addr.address << std::dec << ", file = " << (file ? file->name.toStdString() : "") << ":" << addr.line << "\n";
//  }

  std::cout << "loaded " << lineTable.files.size() << "files, " << lineTable.lines.size() << " addr2line infos"
            << std::endl;

  return Result<DwarfInfo::LineTable, QString>::with_value(std::move(lineTable));
}


Result<DwarfLoader, QString> DwarfLoader::openFile(const QString &file) {
  int fd = open(file.toUtf8().data(), O_RDONLY);
  if (fd < 0) {
    QString msg("error: ");
    msg += QString::number(fd);
    return Result<DwarfLoader, QString>::with_error(msg);
  }

  elf_version(EV_CURRENT);

  auto *elf = elf_begin(fd, ELF_C_READ, nullptr);
  if (auto no = elf_errno(); no) {
    return Result<DwarfLoader, QString>::with_error(QString(elf_errmsg(no)));
  }

  auto buildId = find_build_id(elf);

  return Result<DwarfLoader, QString>::with_value(DwarfLoader{fd, elf, std::move(buildId), file});
}

uint64_t DwarfLoader::getBuildId() {
  uint64_t num = 0;
  for (size_t i = 0; i < 8 && i < buildId.size(); i++) {
    num |= static_cast<uint64_t>(buildId[i]) << (i * 8);
  }
  return num;
}

DwarfLoader::~DwarfLoader() = default;

Result<DwarfInfo, QString> DwarfLoader::load() {
  Dwarf_Error error;
  Dwarf_Debug debug_info;

  auto dwarf_err = dwarf_elf_init(elf, 0, nullptr, nullptr, &debug_info, &error);

  switch (dwarf_err) {
    case DW_DLV_OK: {

      auto functions = scan_debug_info(debug_info);
      if (functions.is_error()) {
        return Result<DwarfInfo, QString>::with_error(std::move(functions).into_error());
      }

      auto lineTable = generate_line_table(debug_info);
      if (lineTable.is_error()) {
        return Result<DwarfInfo, QString>::with_error(std::move(lineTable).into_error());
      }

      DwarfInfo info(std::move(file), std::move(buildId), std::move(functions).into_value(),
                     std::move(lineTable).into_value());

      info.internal->fd = fd;
      info.internal->elf = elf;

      return Result<DwarfInfo, QString>::with_value(std::move(info));
    }
    case DW_DLV_NO_ENTRY: {
      QString msg("dwarf_elf_init() -> no_entry");
      return Result<DwarfInfo, QString>::with_error(msg);
    }
    case DW_DLV_ERROR: {
      QString msg;
      msg += QString::number(dwarf_errno(error));
      msg += ", ";
      msg += dwarf_errmsg(error);
      dwarf_dealloc_error(debug_info, error);
      return Result<DwarfInfo, QString>::with_error(msg);
    }
    default: {
      QString msg;
      msg += "unknown dwarf_elf_init() error code = ";
      msg += QString::number(dwarf_err);
      dwarf_dealloc_error(debug_info, error);
      return Result<DwarfInfo, QString>::with_error(msg);
    }
  }
}

uint64_t DwarfInfo::getBuildId() {
  uint64_t num = 0;
  for (size_t i = 0; i < 8 && i < buildId.size(); i++) {
    num |= static_cast<uint64_t>(buildId[i]) << (i * 8);
  }
  return num;
}

void DwarfInfo::readFromAddress(uint8_t *buffer, uint64_t address, size_t length) {

  Elf64_Phdr *first_phdr = elf64_getphdr(internal->elf);
  size_t count;
  elf_getphdrnum(internal->elf, &count);

  for (size_t i = 0; i < count; i++) {
    Elf64_Phdr *phdr = first_phdr + i;
    if (phdr->p_type != PT_LOAD)
      continue;

    if (address < phdr->p_vaddr && address >= phdr->p_vaddr + phdr->p_memsz)
      continue;

    size_t amt = length;
    if (address + length > phdr->p_vaddr + phdr->p_memsz)
      amt = phdr->p_vaddr + phdr->p_memsz - address;

    size_t offset = address - phdr->p_vaddr + phdr->p_offset;

    lseek(internal->fd, offset, SEEK_SET);
    auto amtRead = read(internal->fd, buffer, amt);
    if (amtRead == length)
      return;

    buffer += amtRead;
    address += amtRead;
    length -= amtRead;
    i = std::numeric_limits<size_t>::max(); // start from beginning again
  }

}

static int
getlowhighpc(Dwarf_Die die,
             int *have_pc_range,
             Dwarf_Addr *lowpc_out,
             Dwarf_Addr *highpc_out,
             Dwarf_Error *error) {
  Dwarf_Addr hipc = 0;
  int res = 0;
  Dwarf_Half form = 0;
  enum Dwarf_Form_Class formclass = DW_FORM_CLASS_UNKNOWN;

  *have_pc_range = FALSE;
  res = dwarf_lowpc(die, lowpc_out, error);
  if (res == DW_DLV_OK) {
    res = dwarf_highpc_b(die, &hipc, &form, &formclass, error);
    if (res == DW_DLV_OK) {
      if (formclass == DW_FORM_CLASS_CONSTANT) {
        hipc += *lowpc_out;
      }
      *highpc_out = hipc;
      *have_pc_range = TRUE;
      return DW_DLV_OK;
    }
  }
  /*  Cannot check ranges yet, we don't know the ranges base
      offset yet. */
  return DW_DLV_NO_ENTRY;
}

/*  The return value is not interesting. Getting
    the name is interesting. */
static int
get_name_from_abstract_origin(Dwarf_Debug dbg,
                              int is_info,
                              Dwarf_Die die,
                              char **name,
                              Dwarf_Error *errp) {
  int res = 0;

  Dwarf_Die abrootdie = 0;
  Dwarf_Attribute ab_attr = 0;
  Dwarf_Off ab_offset = 0;

  res = dwarf_attr(die, DW_AT_abstract_origin, &ab_attr, errp);
  if (res != DW_DLV_OK) {
    return res;
  }

  res = dwarf_global_formref(ab_attr, &ab_offset, errp);
  if (res != DW_DLV_OK) {
    return res;
  }

  res = dwarf_offdie_b(dbg, ab_offset, is_info, &abrootdie, errp);
  if (res != DW_DLV_OK) {
    return res;
  }
  res = dwarf_diename(abrootdie, name, errp);
  if (res == DW_DLV_OK) {
  }
  return res;
}


