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

  FunctionInfo *function = nullptr;
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

struct srcfilesdata {
  char **srcfiles;
  Dwarf_Signed srcfilescount;
  int srcfilesres;
};
struct target_data_s {
  Dwarf_Debug td_dbg;
  Dwarf_Unsigned td_target_pc;     /* from argv */
  int td_print_details; /* from argv */
  int td_reportallfound; /* from argv */
  int td_print_report;

  /*  cu die data. */
  Dwarf_Unsigned td_cu_lowpc;
  Dwarf_Unsigned td_cu_highpc;
  int td_cu_haslowhighpc;
  Dwarf_Die td_cu_die;
  char *td_cu_name;
  char *td_cu_comp_dir;
  Dwarf_Unsigned td_cu_number;
  struct srcfilesdata td_cu_srcfiles;
  /*  Help deal with DW_AT_ranges */
  /*  This is base offset of ranges, the value from
      DW_AT_rnglists_base. */
  Dwarf_Unsigned td_cu_ranges_base;

  /*  Following subprog related. Offset has
      tc_cu_ranges_base added in.   */
  Dwarf_Off td_ranges_offset;
  /*  DIE data of appropriate DIE */
  /*  From DW_AT_name */
  char *td_subprog_name;
  Dwarf_Unsigned td_subprog_fileindex;
  Dwarf_Die td_subprog_die;
  Dwarf_Unsigned td_subprog_lowpc;
  Dwarf_Unsigned td_subprog_highpc;
  int td_subprog_haslowhighpc;
  Dwarf_Unsigned td_subprog_lineaddr;
  Dwarf_Unsigned td_subprog_lineno;
  char *td_subprog_srcfile; /* dealloc */

  int die_count;
};

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
getlowhighpc(UNUSEDARG Dwarf_Debug dbg,
             UNUSEDARG struct target_data_s *td,
             Dwarf_Die die,
             UNUSEDARG int level,
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

void FunctionInfo::dump() {
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


DwarfInfo::~DwarfInfo() {

//  int res = dwarf_finish(debug_info, &error);
//  if (res != DW_DLV_OK) {
//    std::cout << "dwarf_finish failed!" << std::endl;
//  }
}

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

std::optional<std::pair<FunctionInfo *, std::vector<InlinedFunctionInfo *>>>
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
        inline_stack.push_back(inline_func.get());
        inlined_functions = &inline_func->inline_functions;
        fresh = true;
        break;
      }
    }

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

    res = getlowhighpc(nullptr, nullptr, die, 0, &found_range, &low_pc, &high_pc, &error);
    if (res == DW_DLV_OK && found_range) {
      return {std::pair(low_pc, high_pc)};
    } else {
      return {};
    }
  }
}

static Result<std::vector<std::unique_ptr<FunctionInfo>>, QString> scan_debug_info(Dwarf_Debug debug_info) {
  CompilationUnitHeader header{};
  bool is_info = true;
  constexpr auto dump_tags = false;

  std::vector<std::unique_ptr<FunctionInfo>> functions;
  Dwarf_Error error;

  for (int cu_index = 0;; cu_index++) {
    // iterate through the compilation units...
    // returns NO_ENTRY and resets state after progressing all CUs

    int res = dwarf_next_cu_header_d(
            debug_info, is_info, &header.header_length, &header.version_stamp,
            &header.abbrev_offset, &header.address_size, &header.offset_size,
            &header.extension_size, &header.signature, &header.type_offset,
            &header.next_cu_header, &header.header_cu_type, &error);
    TRY_ERROR(std::vector<std::unique_ptr<FunctionInfo>>, debug_info, res, error);

    if (res == DW_DLV_NO_ENTRY)
      break; // no more entries, exit.

    Dwarf_Die cu_die;
    res = dwarf_siblingof_b(debug_info, nullptr, is_info, &cu_die, &error);
    TRY_ERROR(std::vector<std::unique_ptr<FunctionInfo>>, debug_info, res, error);
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

          auto function = std::make_unique<FunctionInfo>();
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

    std::unordered_map<Dwarf_Off, FunctionInfo *> func_lookup;
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

  return Result<std::vector<std::unique_ptr<FunctionInfo>>, QString>::with_value(std::move(functions));
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

DwarfLoader::~DwarfLoader() {
// TODO cleanup dwarf

  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

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

      return Result<DwarfInfo, QString>::with_value(DwarfInfo(std::move(file), std::move(buildId), std::move(functions).into_value()));
    }
    case DW_DLV_NO_ENTRY: {
      QString msg("no_entry");
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


static int
read_line_data(UNUSEDARG Dwarf_Debug dbg,
               struct target_data_s *td,
               Dwarf_Error *errp) {
  int res = 0;
  Dwarf_Unsigned line_version = 0;
  Dwarf_Small table_type = 0;
  Dwarf_Line_Context line_context = 0;
  Dwarf_Signed i = 0;
  Dwarf_Signed baseindex = 0;
  Dwarf_Signed endindex = 0;
  Dwarf_Signed file_count = 0;
  Dwarf_Unsigned dirindex = 0;

  res = dwarf_srclines_b(td->td_cu_die, &line_version,
                         &table_type, &line_context, errp);
  if (res != DW_DLV_OK) {
    return res;
  }
  if (td->td_print_details) {
    printf(" Srclines: linetable version %" DW_PR_DUu
           " table count %d\n", line_version, table_type);
  }
  if (table_type == 0) {
    /* There are no lines here, just table header and names. */
    int sres = 0;

    sres = dwarf_srclines_files_indexes(line_context,
                                        &baseindex, &file_count, &endindex, errp);
    if (sres != DW_DLV_OK) {
      dwarf_srclines_dealloc_b(line_context);
      line_context = 0;
      return sres;
    }
    if (td->td_print_details) {
      printf("  Filenames base index %" DW_PR_DSd
             " file count %" DW_PR_DSd
             " endindex %" DW_PR_DSd "\n",
             baseindex, file_count, endindex);
    }
    for (i = baseindex; i < endindex; i++) {
      Dwarf_Unsigned modtime = 0;
      Dwarf_Unsigned flength = 0;
      Dwarf_Form_Data16 *md5data = 0;
      int vres = 0;
      const char *name = 0;

      vres = dwarf_srclines_files_data_b(line_context, i,
                                         &name, &dirindex, &modtime, &flength,
                                         &md5data, errp);
      if (vres != DW_DLV_OK) {
        dwarf_srclines_dealloc_b(line_context);
        line_context = 0;
        /* something very wrong. */
        return vres;
      }
      if (td->td_print_details) {
        printf("  [%" DW_PR_DSd "] "
               " directory index %" DW_PR_DUu
               " %s \n", i, dirindex, name);
      }
    }
    dwarf_srclines_dealloc_b(line_context);
    return DW_DLV_OK;
  } else if (table_type == 1) {
    const char *dir_name = 0;
    int sres = 0;
    Dwarf_Line *linebuf = 0;
    Dwarf_Signed linecount = 0;
    Dwarf_Signed dir_count = 0;
    Dwarf_Addr prev_lineaddr = 0;
    Dwarf_Unsigned prev_lineno = 0;
    char *prev_linesrcfile = 0;

    sres = dwarf_srclines_files_indexes(line_context,
                                        &baseindex, &file_count, &endindex, errp);
    if (sres != DW_DLV_OK) {
      dwarf_srclines_dealloc_b(line_context);
      line_context = 0;
      return sres;
    }
    if (td->td_print_details) {
      printf("  Filenames base index %" DW_PR_DSd
             " file count %" DW_PR_DSd
             " endindex %" DW_PR_DSd "\n",
             baseindex, file_count, endindex);
    }
    for (i = baseindex; i < endindex; i++) {
      Dwarf_Unsigned dirindexb = 0;
      Dwarf_Unsigned modtime = 0;
      Dwarf_Unsigned flength = 0;
      Dwarf_Form_Data16 *md5data = 0;
      int vres = 0;
      const char *name = 0;

      vres = dwarf_srclines_files_data_b(line_context, i,
                                         &name, &dirindexb, &modtime, &flength,
                                         &md5data, errp);
      if (vres != DW_DLV_OK) {
        dwarf_srclines_dealloc_b(line_context);
        line_context = 0;
        /* something very wrong. */
        return vres;
      }
      if (td->td_print_details) {
        printf("  [%" DW_PR_DSd "] "
               " directory index %" DW_PR_DUu
               " file: %s \n", i, dirindexb, name);
      }
    }
    sres = dwarf_srclines_include_dir_count(line_context,
                                            &dir_count, errp);
    if (sres != DW_DLV_OK) {
      dwarf_srclines_dealloc_b(line_context);
      line_context = 0;
      return sres;
    }
    if (td->td_print_details) {
      printf("  Directories count: %" DW_PR_DSd "\n",
             dir_count);
    }

    for (i = 1; i <= dir_count; ++i) {
      dir_name = 0;
      sres = dwarf_srclines_include_dir_data(line_context,
                                             i, &dir_name, errp);
      if (sres == DW_DLV_ERROR) {
        dwarf_srclines_dealloc_b(line_context);
        line_context = 0;
        return sres;
      }
      if (sres == DW_DLV_NO_ENTRY) {
        printf("Something wrong in dir tables line %d %s\n",
               __LINE__, __FILE__);
        break;
      }
      if (td->td_print_details) {
        printf("  [%" DW_PR_DSd "] directory: "
               " %s \n", i, dir_name);
      }
    }

    /*  For this case where we have a line table we will likely
        wish to get the line details: */
    sres = dwarf_srclines_from_linecontext(line_context,
                                           &linebuf, &linecount,
                                           errp);
    if (sres != DW_DLV_OK) {
      dwarf_srclines_dealloc_b(line_context);
      line_context = 0;
      return sres;
    }
    /* The lines are normal line table lines. */
    for (i = 0; i < linecount; ++i) {
      Dwarf_Addr lineaddr = 0;
      Dwarf_Unsigned filenum = 0;
      Dwarf_Unsigned lineno = 0;
      char *linesrcfile = 0;

      sres = dwarf_lineno(linebuf[i], &lineno, errp);
      if (sres == DW_DLV_ERROR) {
        return sres;
      }
      sres = dwarf_line_srcfileno(linebuf[i], &filenum, errp);
      if (sres == DW_DLV_ERROR) {
        return sres;
      }
      if (filenum) {
        filenum -= 1;
      }
      sres = dwarf_lineaddr(linebuf[i], &lineaddr, errp);
      if (sres == DW_DLV_ERROR) {
        return sres;
      }
      sres = dwarf_linesrc(linebuf[i], &linesrcfile, errp);
      if (td->td_print_details) {
        printf("  [%" DW_PR_DSd "] "
               " address 0x%" DW_PR_DUx
               " filenum %" DW_PR_DUu
               " lineno %" DW_PR_DUu
               " %s \n", i, lineaddr, filenum, lineno, linesrcfile);
      }
      if (lineaddr > td->td_target_pc) {
        /* Here we detect the right source and line */
        td->td_subprog_lineaddr = prev_lineaddr;
        td->td_subprog_lineno = prev_lineno;
        td->td_subprog_srcfile = prev_linesrcfile;
        return DW_DLV_OK;
      }
      prev_lineaddr = lineaddr;
      prev_lineno = lineno;
      if (prev_linesrcfile) {
        dwarf_dealloc(dbg, prev_linesrcfile, DW_DLA_STRING);
      }
      prev_linesrcfile = linesrcfile;
    }
    /*  Here we detect the right source and line (last such
        in this subprogram) */
    td->td_subprog_lineaddr = prev_lineaddr;
    td->td_subprog_lineno = prev_lineno;
    td->td_subprog_srcfile = prev_linesrcfile;
    dwarf_srclines_dealloc_b(line_context);
    return DW_DLV_OK;

  }
  return DW_DLV_ERROR;
#if 0
  /* ASSERT: table_type == 2,
    Experimental two-level line table. Version 0xf006
    We do not define the meaning of this non-standard
    set of tables here. */
    /*  For ’something C’ (two-level line tables)
        one codes something like this
        Note that we do not define the meaning
        or use of two-level li
        tables as these are experimental,
        not standard DWARF. */
    sres = dwarf_srclines_two_level_from_linecontext(line_context,
        &linebuf,&linecount,
        &linebuf_actuals,&linecount_actuals,
        &err);
    if (sres == DW_DLV_OK) {
        for (i = 0; i < linecount; ++i) {
            /*  use linebuf[i], these are
                the ’logicals’ entries. */
        }
        for (i = 0; i < linecount_actuals; ++i) {
            /*  use linebuf_actuals[i],
                these are the actuals entries */
        }
        dwarf_srclines_dealloc_b(line_context);
        line_context = 0;
        linebuf = 0;
        linecount = 0;
        linebuf_actuals = 0;
        linecount_actuals = 0;
    } else if (sres == DW_DLV_NO_ENTRY) {
        dwarf_srclines_dealloc_b(line_context);
        line_context = 0;
        linebuf = 0;
        linecount = 0;
        linebuf_actuals = 0;
        linecount_actuals = 0;
        return sres;
    } else { /*DW_DLV_ERROR */
        return sres;
    }
#endif /* 0 */
}


static int
getlowhighpc(UNUSEDARG Dwarf_Debug dbg,
             UNUSEDARG struct target_data_s *td,
             Dwarf_Die die,
             UNUSEDARG int level,
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


