//
// Created by Will Gulian on 12/22/20.
//

#include "DisassemblyModel.h"
#include "../Disassembler/Disassembler.h"

struct DisassemblyModel::RowInfo {
  Disassembler::Instruction instruction;
  int samples;
  std::optional<int> jumpOffset;
  std::optional<std::pair<QString, uint32_t>> sourceLine;
};

DisassemblyModel::DisassemblyModel(
        std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable, QObject *parent)
        : QAbstractTableModel(parent), traceData(std::move(traceData)), debugTable(std::move(debugTable)), rows() {

}

DisassemblyModel::~DisassemblyModel() = default;

void DisassemblyModel::disassembleRegion(uint64_t buildId, uint64_t startAddress, uint64_t endAddress, const std::unordered_map<uint64_t, size_t> *addrCounts) {
  beginResetModel();

  rows.clear();
  Disassembler dis(Disassembler::Arch::Aarch64);

  const std::lock_guard<std::mutex> g(traceData->lock);
  const std::lock_guard<std::mutex> g2(debugTable->lock);

  auto debugFile = debugTable->loadedFiles.find(buildId);
  if (debugFile == debugTable->loadedFiles.end()) {
    std::cout << "no file for build id" << std::endl;
    return;
  }

  std::vector<uint8_t> data;
  data.resize(endAddress - startAddress, 0);

  debugFile->second.readFromAddress(data.data(), startAddress, endAddress - startAddress);

  auto segment = dis.disassemble(data.data(), data.size(), startAddress);

  for (auto &&row : std::move(segment.instructions)) {
    int samples = 0;
    if (addrCounts) {
      if (auto it = addrCounts->find(row.address); it != addrCounts->end()) {
        samples = (int)it->second;
      }
    }

    std::optional<int> jumpOffset;
    if (row.jumpTarget && row.jumpTarget.value() >= startAddress && row.jumpTarget.value() < endAddress) {
      jumpOffset = (int)(row.jumpTarget.value() - row.address);
    }

    auto sourceLine = debugFile->second.getLineForAddress(row.address);

    rows.push_back(RowInfo { std::move(row), samples, jumpOffset, std::move(sourceLine) });
  }

  std::cout << "dis " << rows.size() << " instructions" << std::endl;

  endResetModel();
}

int DisassemblyModel::rowCount(const QModelIndex &parent) const {
  return rows.size();
}

int DisassemblyModel::columnCount(const QModelIndex &parent) const {
  return 3;
}

QVariant DisassemblyModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Orientation::Horizontal)
    return QVariant();

  if (role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
    case 0:
      return QString("Samples");
    case 1:
      return QString("Instruction");
    case 2:
      return QString("File");
    default:
      return QString("???");
  }
}

QVariant DisassemblyModel::data(const QModelIndex &index, int role) const {
  switch (index.column()) {
    case 0: {
      if (role != Qt::DisplayRole)
        return QVariant();

      auto samples = rows.at(index.row()).samples;
      if (samples == 0)
        return QString("");

      return QString::number(samples);
    }
    case 1: {
      if (role != Qt::DisplayRole)
        return QVariant();

      auto &row = rows.at(index.row());

      QString name = row.instruction.mnemonic + " " + row.instruction.operands;
      if (row.jumpOffset) {
        name += "    < ";
        name += QString::number(row.jumpOffset.value());
        name += " >";
      }

      return name;
    }
    case 2: {
      auto &row = rows.at(index.row());
      if (!row.sourceLine)
        return QVariant();

      if (role == Qt::DisplayRole) {
        auto file = row.sourceLine->first.section('/', -1);
        file += ":";
        file += QString::number(row.sourceLine->second);
        return file;
      } else if (role == Qt::ToolTipRole) {
        return row.sourceLine->first + ":" + QString::number(row.sourceLine->second);
      } else {
        return QVariant();
      }
    }
    default: {
      if (role != Qt::DisplayRole)
        return QVariant();

      return QString("??? ") + QString::number(index.row());
    }
  }
}



