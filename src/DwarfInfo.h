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
  [[nodiscard]] virtual bool isInline() const = 0;

  [[nodiscard]] virtual QString getFullName() const = 0;

  [[nodiscard]] virtual ConcreteFunctionInfo *getConcreteFunction() = 0;

  [[nodiscard]] virtual bool containsAddress(uint64_t address) const = 0;
};

struct InlinedFunctionInfo : public AbstractFunctionInfo {

  int depth = 0;
  Dwarf_Off die_offset = 0;
  Dwarf_Off origin_offset = 0;
  ConcreteFunctionInfo *origin_fn = nullptr;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  std::optional<std::pair<QString, uint32_t>> sourceLine;

  void dump();

  [[nodiscard]] bool isInline() const override {
    return true;
  }

  [[nodiscard]] QString getFullName() const override;

  ConcreteFunctionInfo *getConcreteFunction() override {
    return origin_fn;
  }

  [[nodiscard]] bool containsAddress(uint64_t address) const override;
};

struct ConcreteFunctionInfo : public AbstractFunctionInfo {
  Dwarf_Off die_offset = 0;
  const char *name = nullptr;
  QString full_name;
  QString linkageName;
  uint64_t buildId = 0;

  std::vector<std::unique_ptr<InlinedFunctionInfo>> inline_functions;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;

  void dump();

  [[nodiscard]] bool isInline() const override {
    return false;
  }

  [[nodiscard]] QString getFullName() const override {
    return full_name;
  }

  ConcreteFunctionInfo *getConcreteFunction() override {
    return this;
  }

  [[nodiscard]] bool containsAddress(uint64_t address) const override;
};

class DwarfInfo {
  struct Internal;
  friend DwarfLoader;

public:
  struct LineFileInfo {
    int cu_index;
    Dwarf_Signed file_index;
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

  std::optional<std::pair<ConcreteFunctionInfo *, std::vector<InlinedFunctionInfo *>>> resolve_address(uint64_t address) const;

  std::optional<std::pair<QString, uint32_t>> getLineForAddress(uint64_t address) const;

  std::pair<QString, bool> symbolicate(uint64_t address) const;

  uint64_t getBuildId() const;

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
