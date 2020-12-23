//
// Created by Will Gulian on 12/22/20.
//

#include "DisassemblyModel.h"
#include "../Disassembler/Disassembler.h"

struct DisassemblyModel::RowInfo {
  int samples;
  QString instruction;
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

  for (auto &row : segment.instructions) {
    QString instr;
    instr += row.mnemonic;
    instr += " ";
    instr += row.operands;

    int samples = 0;
    if (addrCounts) {
      if (auto it = addrCounts->find(row.address); it != addrCounts->end()) {
        samples = (int)it->second;
      }
    }

    rows.push_back(RowInfo { samples, std::move(instr) });
  }

  std::cout << "dis " << rows.size() << " instructions" << std::endl;

  endResetModel();
}

int DisassemblyModel::rowCount(const QModelIndex &parent) const {
  return rows.size();
}

int DisassemblyModel::columnCount(const QModelIndex &parent) const {
  return 2;
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
    default:
      return QString("???");
  }
}

QVariant DisassemblyModel::data(const QModelIndex &index, int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();

  switch (index.column()) {
    case 0: {
      auto samples = rows.at(index.row()).samples;
      if (samples == 0)
        return QString("");

      return QString::number(samples);
    }
    case 1:
      return rows.at(index.row()).instruction;
    default:
      return QString("??? ") + QString::number(index.row());
  }
}



