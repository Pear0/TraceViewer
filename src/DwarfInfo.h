//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_DWARFINFO_H
#define TRACEVIEWER2_DWARFINFO_H

#include <ctime>
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
  std::vector<uint8_t> buildId;
  std::vector<std::unique_ptr<FunctionInfo>> functions;

public:
  QString file;
  std::time_t loaded_time;

  explicit DwarfInfo(QString &&file, std::vector<uint8_t> &&buildId, std::vector<std::unique_ptr<FunctionInfo>> &&functions)
          : file(std::move(file)), loaded_time(std::time(nullptr)), buildId(std::move(buildId)), functions(std::move(functions)) {}

  explicit DwarfInfo(const DwarfInfo &other) = delete;

  DwarfInfo(DwarfInfo &&other) = default;

  virtual ~DwarfInfo();

  std::optional<std::pair<FunctionInfo *, std::vector<InlinedFunctionInfo *>>> resolve_address(uint64_t address);

  std::pair<QString, bool> symbolicate(uint64_t address);

  uint64_t getBuildId();

private:

};

struct DwarfLoader {
  int fd;
  Elf *elf;
  std::vector<uint8_t> buildId;
  QString file;

  static Result<DwarfLoader, QString> openFile(const QString &file);

  DwarfLoader(int fd, Elf *elf, std::vector<uint8_t> buildId, QString file)
          : fd(fd), elf(elf), buildId(std::move(buildId)), file(std::move(file)) {}

  virtual ~DwarfLoader();

  uint64_t getBuildId();

  Result<DwarfInfo, QString> load();

};


#endif //TRACEVIEWER2_DWARFINFO_H
