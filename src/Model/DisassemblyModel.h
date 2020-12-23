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
Q_OBJECT

  std::shared_ptr<TraceData> traceData;
  std::shared_ptr<DebugTable> debugTable;
public:
  DisassemblyModel(std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable,
          QObject *parent = nullptr);

  ~DisassemblyModel() override;

  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

  [[nodiscard]] int columnCount(const QModelIndex &parent) const override;

  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

};


#endif //TRACEVIEWER2_DISASSEMBLYMODEL_H
