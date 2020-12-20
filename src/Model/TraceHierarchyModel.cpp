//
// Created by Will Gulian on 12/19/20.
//

#include <unordered_set>

#include <QSize>

#include "../QtCustomUser.h"
#include "TraceHierarchyModel.h"

enum class ItemType {
  Invalid = 0,
  RawAddress,
  Function,
};

struct TraceHierarchyModel::HierarchyItem {
  std::vector<std::unique_ptr<HierarchyItem>> children;
  HierarchyItem *parent{nullptr};
  int selfIndex{-1};
  size_t count{0};
  ItemType itemType{ItemType::Invalid};

  // RawAddress fields
  uint64_t address{0};

  // Function fields

  AbstractFunctionInfo *functionInfo{nullptr};

  // Invalid
  HierarchyItem() {}

  explicit HierarchyItem(uint64_t address)
          : itemType(ItemType::RawAddress), address(address) {}

  explicit HierarchyItem(AbstractFunctionInfo *functionInfo)
          : itemType(ItemType::Function), functionInfo(functionInfo) {}

  explicit HierarchyItem(HierarchyItem &&other) = default;

  virtual ~HierarchyItem() = default;

  HierarchyItem& operator=(HierarchyItem &&) = default;

  [[nodiscard]] bool targetMatch(const HierarchyItem *other) const {
    return itemType == other->itemType && address == other->address && functionInfo == other->functionInfo;
  }

  [[nodiscard]] std::pair<ItemType, uint64_t> matchId() const {
    return std::make_pair(itemType,
                          itemType == ItemType::Function ? reinterpret_cast<uint64_t>(functionInfo) : address);
  }

  [[nodiscard]] QString getName() const {
    if (itemType == ItemType::Function)
      return functionInfo->getFullName();
    else
      return QString("0x") + QString::number(address, 16);
  }
};

struct TraceHierarchyModel::SymbolCache {
  HierarchyItem root{std::numeric_limits<uint64_t>::max()};
  uint64_t lastTraceChangeCount { 0 };
};

TraceHierarchyModel::TraceHierarchyModel(
        std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable, QObject *parent)
        : QAbstractItemModel(parent), traceData(std::move(traceData)), debugTable(std::move(debugTable)),
          symbolCache(std::make_unique<SymbolCache>()) {


  connect(this->traceData.get(), &TraceData::dataChanged, this, &TraceHierarchyModel::tracesChanged);
}

TraceHierarchyModel::~TraceHierarchyModel() = default;

QModelIndex TraceHierarchyModel::selfModelIndex(HierarchyItem *item, int column) const {
  assert(item && "item was null");

  if (item == &symbolCache->root)
    return QModelIndex();

  return createIndex(item->selfIndex, column, static_cast<void *>(item));
}


void TraceHierarchyModel::tracesChanged() {

  const std::lock_guard<std::mutex> g(traceData->lock);
  const std::lock_guard<std::mutex> g2(debugTable->lock);

  for (auto &event : traceData->events) {
    // We've ingested this event already
    if (event.change_index <= symbolCache->lastTraceChangeCount)
      continue;

    assert(event.change_index <= traceData->change_count && "event change index too high");

    HierarchyItem *parent = &symbolCache->root;

    for (auto &frame : event.frames) {
      HierarchyItem item;
      if (auto dwarf = debugTable->loadedFiles.find(event.build_id); dwarf != debugTable->loadedFiles.end()) {
        if (auto func = dwarf->second.resolve_address(frame.pc); func) {

          item = HierarchyItem(func->first);
        }
      }
      if (item.itemType == ItemType::Invalid)
        item = HierarchyItem(frame.pc);

      // Find an existing child that matches.
      HierarchyItem *matchedItem = nullptr;
      for (auto &child : parent->children) {
        if (child->targetMatch(&item)) {
          matchedItem = child.get();
          break;
        }
      }

      // No child matches, add it
      if (matchedItem == nullptr) {
        item.parent = parent;
        auto index = item.selfIndex = parent->children.size();

        auto parentModelIndex = selfModelIndex(parent);

        beginInsertRows(parentModelIndex, index, index);

        auto itemPtr = std::make_unique<HierarchyItem>(std::move(item));
        matchedItem = itemPtr.get();
        parent->children.push_back(std::move(itemPtr));

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

  symbolCache->lastTraceChangeCount = traceData->change_count;
}

QModelIndex TraceHierarchyModel::index(int row, int column, const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto parentItem = static_cast<HierarchyItem *>(parent.internalPointer());

    return createIndex(row, column, static_cast<void *>(parentItem->children.at(row).get()));
  }

  return createIndex(row, column, static_cast<void *>(symbolCache->root.children.at(row).get()));
}

QModelIndex TraceHierarchyModel::parent(const QModelIndex &child) const {
  auto item = static_cast<HierarchyItem *>(child.internalPointer());
  return selfModelIndex(item->parent);
}

int TraceHierarchyModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto item = static_cast<HierarchyItem *>(parent.internalPointer());
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
        case 0:
          return QString("Function");
        case 1:
          return QString("Samples (%)");
        case 2:
          return QString("Self Samples (%)");
        default:
          return QString("???");
      }
    }
    default:
      return QVariant();
  }
}

QVariant TraceHierarchyModel::data(const QModelIndex &index, int role) const {
  auto item = static_cast<HierarchyItem *>(index.internalPointer());

  switch (role) {
    case Qt::DisplayRole:
      if (index.column() == 0)
        return item->getName();

      return QString::number(item->count);
    case UserQt::UserNumericRole:
      return (int) item->count;

    default:
      return QVariant();
  }
}








