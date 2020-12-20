//
// Created by Will Gulian on 12/19/20.
//

#include <unordered_set>

#include <QSize>

#include "TraceHierarchyModel.h"

struct TraceHierarchyModel::AbstractItem {
  std::vector<std::unique_ptr<AbstractItem>> children;
  AbstractItem *parent { nullptr };
  int selfIndex { -1 };
  size_t count { 0 };

  virtual ~AbstractItem() = default;

  virtual bool targetMatch(const AbstractItem *other) = 0;

  virtual QString getName() = 0;
};

struct TraceHierarchyModel::RawAddressItem : public AbstractItem {
  uint64_t address { 0 };

  explicit RawAddressItem(uint64_t address) : address(address) {}

  ~RawAddressItem() override = default;

  bool targetMatch(const AbstractItem *other) override {
    auto *ptr = dynamic_cast<const RawAddressItem *>(other);
    return ptr && ptr->address == address;
  }

  QString getName() override {
    return QString("0x") + QString::number(address, 16);
  }
};

struct TraceHierarchyModel::FunctionItem : public AbstractItem {
  AbstractFunctionInfo *functionInfo = nullptr;

  explicit FunctionItem(AbstractFunctionInfo *functionInfo) : functionInfo(functionInfo) {}

  ~FunctionItem() override = default;

  bool targetMatch(const AbstractItem *other) override {
    auto *ptr = dynamic_cast<const FunctionItem *>(other);
    return ptr && ptr->functionInfo == functionInfo;
  }

  QString getName() override {
    return functionInfo->getFullName();
  }
};


struct TraceHierarchyModel::SymbolCache {
  RawAddressItem root { std::numeric_limits<uint64_t>::max() };
};

TraceHierarchyModel::TraceHierarchyModel(
        std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable, QObject *parent)
        : QAbstractItemModel(parent), traceData(std::move(traceData)), debugTable(std::move(debugTable)),
          symbolCache(std::make_unique<SymbolCache>()) {




  connect(this->traceData.get(), &TraceData::dataChanged, this, &TraceHierarchyModel::tracesChanged);
}

TraceHierarchyModel::~TraceHierarchyModel() = default;

QModelIndex TraceHierarchyModel::selfModelIndex(AbstractItem *item, int column) const {
  assert(item && "item was null");

  if (item == &symbolCache->root)
    return QModelIndex();

  return createIndex(item->selfIndex, column, static_cast<void *>(item));
}


void TraceHierarchyModel::tracesChanged() {

  const std::lock_guard<std::mutex> g(traceData->lock);
  const std::lock_guard<std::mutex> g2(debugTable->lock);

  for (auto &event : traceData->events) {
    AbstractItem *parent = &symbolCache->root;

    for (auto &frame : event.frames) {
      std::unique_ptr<AbstractItem> item;
      if (auto dwarf = debugTable->loadedFiles.find(event.build_id); dwarf != debugTable->loadedFiles.end()) {
        if (auto func = dwarf->second.resolve_address(frame.pc); func) {

          item = std::make_unique<FunctionItem>(func->first);
        }
      }
      if (!item)
        item = std::make_unique<RawAddressItem>(frame.pc);

      // Find an existing child that matches.
      auto matchedItem = item.get();
      for (auto &child : parent->children) {
        if (child->targetMatch(item.get())) {
          matchedItem = child.get();
          break;
        }
      }

      // No child matches, add it
      if (matchedItem == item.get()) {
        item->parent = parent;
        auto index = item->selfIndex = parent->children.size();

        auto parentModelIndex = selfModelIndex(parent);

        beginInsertRows(parentModelIndex, index, index);

        parent->children.push_back(std::move(item));
        item = nullptr;

        endInsertRows();

      }

      matchedItem->count++;

      // Update the count column
      auto itemIndex = selfModelIndex(matchedItem, 1);
      dataChanged(itemIndex, itemIndex);

      // follow the chain...
      parent = matchedItem;
    }

  }

}

QModelIndex TraceHierarchyModel::index(int row, int column, const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto parentItem = static_cast<AbstractItem *>(parent.internalPointer());

    return createIndex(row, column, static_cast<void *>(parentItem->children.at(row).get()));
  }

  return createIndex(row, column, static_cast<void *>(symbolCache->root.children.at(row).get()));
}

QModelIndex TraceHierarchyModel::parent(const QModelIndex &child) const {
  auto item = static_cast<AbstractItem *>(child.internalPointer());
  return selfModelIndex(item->parent);
}

int TraceHierarchyModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto item = static_cast<AbstractItem *>(parent.internalPointer());
    return item->children.size();
  }

  return symbolCache->root.children.size();
}

int TraceHierarchyModel::columnCount(const QModelIndex &parent) const {
  return 3;
}

QVariant TraceHierarchyModel::headerData(int section, Qt::Orientation orientation, int role) const {
  switch (role) {
    case Qt::DisplayRole: {
      switch (section) {
        case 0: return QString("Function");
        case 1: return QString("Samples (%)");
        case 2: return QString("Self Samples (%)");
        default: return QString("???");
      }
    }
    default:
      return QVariant();
  }
}

QVariant TraceHierarchyModel::data(const QModelIndex &index, int role) const {
  auto item = static_cast<AbstractItem *>(index.internalPointer());

  switch (role) {
    case Qt::DisplayRole:
      if (index.column() == 0)
        return item->getName();

      return QString::number(item->count);
//    case Qt::SizeHintRole:
//      return QSize();
    default:
      return QVariant();
  }
}








