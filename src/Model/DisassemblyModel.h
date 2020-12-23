//
// Created by Will Gulian on 12/22/20.
//

#ifndef TRACEVIEWER2_DISASSEMBLYMODEL_H
#define TRACEVIEWER2_DISASSEMBLYMODEL_H

#include <memory>

#include <QAbstractTableModel>

#include "../TraceData.h"
#include "../DebugTable.h"

class DisassemblyModel : public QAbstractTableModel {
private:
  struct RowInfo;

Q_OBJECT

  std::shared_ptr<TraceData> traceData;
  std::shared_ptr<DebugTable> debugTable;
  std::vector<RowInfo> rows;
public:
  DisassemblyModel(std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable,
          QObject *parent = nullptr);

  ~DisassemblyModel() override;

public slots:
  void disassembleRegion(uint64_t buildId, uint64_t startAddress, uint64_t endAddress, const std::unordered_map<uint64_t, size_t> *addrCounts = nullptr);

public:
  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

  [[nodiscard]] int columnCount(const QModelIndex &parent) const override;

  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

};


#endif //TRACEVIEWER2_DISASSEMBLYMODEL_H
