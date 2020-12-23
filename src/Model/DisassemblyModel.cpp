//
// Created by Will Gulian on 12/22/20.
//

#include "DisassemblyModel.h"

DisassemblyModel::DisassemblyModel(
        std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable, QObject *parent)
        : QAbstractTableModel(parent), traceData(std::move(traceData)), debugTable(std::move(debugTable)) {

}

DisassemblyModel::~DisassemblyModel() = default;

int DisassemblyModel::rowCount(const QModelIndex &parent) const {
  return 20;
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
    case 0:
      return QString("Samples ") + QString::number(index.row());
    case 1:
      return QString("Instruction ") + QString::number(index.row());
    default:
      return QString("??? ") + QString::number(index.row());
  }
}



