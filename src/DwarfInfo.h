//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_DWARFINFO_H
#define TRACEVIEWER2_DWARFINFO_H

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "libdwarf.h"
#include "Result.h"

struct FunctionInfo;
struct InlinedFunctionInfo;

struct InlinedFunctionInfo {

  int depth = 0;
  Dwarf_Off die_offset = 0;
  Dwarf_Off origin_offset = 0;
  FunctionInfo *origin_fn = nullptr;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  void dump();
};

struct FunctionInfo {
  Dwarf_Off die_offset = 0;
  const char *name = nullptr;
  QString full_name;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  void dump();
};


class DwarfInfo {
  Dwarf_Debug debug_info;
  Dwarf_Error error;

  std::vector<std::unique_ptr<FunctionInfo>> functions;

public:
  static Result<DwarfInfo, std::string> load_file(const char *file_name);

  explicit DwarfInfo(Dwarf_Debug debug) : debug_info(debug), error(nullptr) {}
  explicit DwarfInfo(const DwarfInfo &other) = delete;
  explicit DwarfInfo(DwarfInfo &&other) = default;
  virtual ~DwarfInfo();

  void bruh();

  std::optional<std::pair<FunctionInfo *, std::vector<InlinedFunctionInfo *>>> resolve_address(uint64_t address);

  std::pair<QString, bool> symbolicate(uint64_t address);

  QString handle_error(Dwarf_Error error);
private:

  Result<int, QString> scan_debug_info();

};


#endif //TRACEVIEWER2_DWARFINFO_H
