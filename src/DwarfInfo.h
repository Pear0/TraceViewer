//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_DWARFINFO_H
#define TRACEVIEWER2_DWARFINFO_H

#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "libdwarf.h"
#include "Result.h"

struct ConcreteFunctionInfo;
struct InlinedFunctionInfo;
struct DwarfLoader;

struct AbstractFunctionInfo {
  virtual bool isInline() = 0;

  virtual QString getFullName() = 0;
};

struct InlinedFunctionInfo : public AbstractFunctionInfo {

  int depth = 0;
  Dwarf_Off die_offset = 0;
  Dwarf_Off origin_offset = 0;
  ConcreteFunctionInfo *origin_fn = nullptr;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  void dump();

  bool isInline() override {
    return true;
  }

  QString getFullName() override;
};

struct ConcreteFunctionInfo : public AbstractFunctionInfo {
  Dwarf_Off die_offset = 0;
  const char *name = nullptr;
  QString full_name;
  uint64_t buildId = 0;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  void dump();

  bool isInline() override {
    return false;
  }

  QString getFullName() override {
    return full_name;
  }
};

class DwarfInfo {
  struct Internal;
  friend DwarfLoader;

public:
  struct LineFileInfo {
    QString name;
    Dwarf_Unsigned length { 0 };
  };

  struct LineInfo {
    uint64_t address { 0 };
    int32_t fileIndex { 0 };
    uint32_t line { 0 };
  };

  struct LineTable {
    std::vector<LineFileInfo> files;
    std::vector<LineInfo> lines;
  };

private:
  std::vector<uint8_t> buildId;
  std::vector<std::unique_ptr<ConcreteFunctionInfo>> functions;
  std::unique_ptr<Internal> internal;

  LineTable lineTable;

public:
  QString file;
  std::time_t loaded_time;

  explicit DwarfInfo(QString &&file, std::vector<uint8_t> &&buildId, std::vector<std::unique_ptr<ConcreteFunctionInfo>> &&functions, LineTable &&lineTable);

  explicit DwarfInfo(const DwarfInfo &other) = delete;

  DwarfInfo(DwarfInfo &&other);

  virtual ~DwarfInfo();

  void readFromAddress(uint8_t *buffer, uint64_t address, size_t length);

  std::optional<std::pair<ConcreteFunctionInfo *, std::vector<InlinedFunctionInfo *>>> resolve_address(uint64_t address);

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
