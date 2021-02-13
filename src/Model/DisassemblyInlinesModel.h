//
// Created by Will Gulian on 1/2/21.
//

#ifndef TRACEVIEWER2_DISASSEMBLYINLINESMODEL_H
#define TRACEVIEWER2_DISASSEMBLYINLINESMODEL_H

#include <variant>
#include <vector>

#include <QAbstractItemModel>
#include "../Disassembler/Disassembler.h"
#include "../TraceData.h"
#include "../DebugTable.h"

class DisassemblyInlinesModel : public QAbstractItemModel {
  Q_OBJECT

  struct CodeSection;

  struct Common {
    int samples { 0 };
    int selfIndex { 0 };
    std::optional<std::pair<QString, uint32_t>> sourceLine;
    CodeSection *parent { nullptr };

    virtual ~Common() = default;

    virtual bool isFunc() = 0;
  };

  struct RowInfo : public Common {
    Disassembler::Instruction instruction;
    std::optional<int> jumpOffset;
    std::optional<uint64_t> jumpLocation;

    RowInfo(Disassembler::Instruction &&instruction, int samples, std::optional<int> jumpOffset, std::optional<uint64_t> jumpLocation,
            std::optional<std::pair<QString, uint32_t>> &&sourceLine)
      : instruction(std::move(instruction)), jumpOffset(jumpOffset), jumpLocation(jumpLocation) {
      this->samples = samples;
      this->sourceLine = std::move(sourceLine);
    }

    bool isFunc() override {
      return false;
    }
  };

  struct CodeSection : public Common {
    std::vector<std::unique_ptr<Common>> lines;
    InlinedFunctionInfo *inlineInfo { nullptr };

    CodeSection(InlinedFunctionInfo *inlineInfo)
            : inlineInfo(inlineInfo) {
    }

    CodeSection(InlinedFunctionInfo *inlineInfo, int samples, std::optional<std::pair<QString, uint32_t>> &&sourceLine)
    : inlineInfo(inlineInfo) {
      this->samples = samples;
      this->sourceLine = std::move(sourceLine);
    }

    bool isFunc() override {
      return true;
    }

    void assignChildIndices() {
      for (size_t i = 0; i < lines.size(); i++) {
        lines[i]->selfIndex = i;
        lines[i]->parent = this;

        if (auto sectionChild = dynamic_cast<CodeSection *>(lines[i].get()); sectionChild) {
          sectionChild->assignChildIndices();
        }
      }
    }

  };

  std::shared_ptr<TraceData> traceData;
  std::shared_ptr<DebugTable> debugTable;
  CodeSection root;
  uint64_t buildId { 0 };
public:
  DisassemblyInlinesModel(std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable,
          QObject *parent = nullptr);

  ~DisassemblyInlinesModel() override;

public slots:
  void disassembleRegion(const ConcreteFunctionInfo &func, const std::unordered_map<uint64_t, size_t> *addrCounts = nullptr);

public:
  [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent) const override;

  [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;

  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

  [[nodiscard]] int columnCount(const QModelIndex &parent) const override;

  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

protected:
  QModelIndex selfModelIndex(Common *item, int column = 0) const;


};


#endif //TRACEVIEWER2_DISASSEMBLYINLINESMODEL_H
