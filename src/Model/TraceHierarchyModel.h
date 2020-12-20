//
// Created by Will Gulian on 12/19/20.
//

#ifndef TRACEVIEWER2_TRACEHIERARCHYMODEL_H
#define TRACEVIEWER2_TRACEHIERARCHYMODEL_H

#include <memory>

#include <QAbstractItemModel>

#include "../TraceData.h"
#include "../DebugTable.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-nodiscard"

class TraceHierarchyModel : public QAbstractItemModel {
  struct SymbolCache;
  struct HierarchyItem;
  struct RawAddressItem;
  struct FunctionItem;

  Q_OBJECT

  std::shared_ptr<TraceData> traceData;
  std::shared_ptr<DebugTable> debugTable;
  std::unique_ptr<SymbolCache> symbolCache;
public:
  TraceHierarchyModel(std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable,
                      QObject *parent = nullptr);

  ~TraceHierarchyModel() override;

private slots:
  void tracesChanged();

protected:
  QModelIndex selfModelIndex(HierarchyItem *item, int column = 0) const;

public:
  QModelIndex index(int row, int column, const QModelIndex &parent) const override;

  QModelIndex parent(const QModelIndex &child) const override;

  int rowCount(const QModelIndex &parent) const override;

  int columnCount(const QModelIndex &parent) const override;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  QVariant data(const QModelIndex &index, int role) const override;

};

#pragma clang diagnostic pop

#endif //TRACEVIEWER2_TRACEHIERARCHYMODEL_H
