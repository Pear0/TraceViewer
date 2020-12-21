//
// Created by Will Gulian on 12/19/20.
//

#include <unordered_set>

#include <QPalette>
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
  size_t selfCount{0};
  ItemType itemType{ItemType::Invalid};

  // RawAddress fields
  uint64_t address{0};

  // Function fields
  AbstractFunctionInfo *functionInfo{nullptr};

  // Invalid
  HierarchyItem() = default;

  explicit HierarchyItem(uint64_t address)
          : itemType(ItemType::RawAddress), address(address) {}

  explicit HierarchyItem(AbstractFunctionInfo *functionInfo)
          : itemType(ItemType::Function), functionInfo(functionInfo) {}

  explicit HierarchyItem(HierarchyItem &&other) = default;

  virtual ~HierarchyItem() = default;

  HierarchyItem &operator=(HierarchyItem &&) = default;

  [[nodiscard]] bool targetMatch(const HierarchyItem *other) const {
    return itemType == other->itemType && address == other->address && functionInfo == other->functionInfo;
  }

  [[nodiscard]] std::pair<ItemType, uint64_t> matchId() const {
    return std::make_pair(itemType,
                          itemType == ItemType::Function ? reinterpret_cast<uint64_t>(functionInfo) : address);
  }

  [[nodiscard]] QString getName() const {
    if (itemType == ItemType::Function) {
      if (functionInfo->isInline())
        return QString("~") + functionInfo->getFullName();

      return functionInfo->getFullName();
    } else {
      return QString("0x") + QString::number(address, 16);
    }
  }
};

struct TraceHierarchyModel::SymbolCache {
  HierarchyItem root{std::numeric_limits<uint64_t>::max()};
  uint64_t lastTraceChangeCount{0};
  ViewPerspective viewPerspective{ViewPerspective::BottomUp};
  bool showInlineFuncs { true };
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
  updateModelTraces(symbolCache->viewPerspective, symbolCache->showInlineFuncs);
}

void TraceHierarchyModel::generateHierarchyItems(std::vector<HierarchyItem> &items, const TraceEvent &event,
                                                 const TraceFrame &frame) const {
  items.clear();

  if (auto dwarf = debugTable->loadedFiles.find(event.build_id); dwarf != debugTable->loadedFiles.end()) {
    if (auto func = dwarf->second.resolve_address(frame.pc); func) {

      items.emplace_back(func->first);

      if (symbolCache->showInlineFuncs) {
        for (auto *inlineFunc : func->second) {
          items.emplace_back(inlineFunc);
        }
      }

      return;
    }
  }

  items.emplace_back(frame.pc);
}

template <typename Container, typename Func>
static void iterateContainer(Container &container, bool reverse, Func func) {
  if (reverse) {
    for (auto it = container.rbegin(); it != container.rend(); ++it) {
      func(*it, (it+1) != container.rend());
    }
  } else {
    for (auto it = container.begin(); it != container.end(); ++it) {
      func(*it, (it+1) != container.end());
    }
  }
}

void TraceHierarchyModel::updateModelTraces(ViewPerspective viewPerspective, bool showInlineFuncs) {
  bool needsReset = viewPerspective != symbolCache->viewPerspective || showInlineFuncs != symbolCache->showInlineFuncs;

  const std::lock_guard<std::mutex> g(traceData->lock);
  const std::lock_guard<std::mutex> g2(debugTable->lock);

  if (needsReset) {
    beginResetModel();
    symbolCache->viewPerspective = viewPerspective;
    symbolCache->showInlineFuncs = showInlineFuncs;
    symbolCache->root.children.clear();
    symbolCache->lastTraceChangeCount = 0;
  }

  std::vector<HierarchyItem> currentHierarchyItems;

  for (auto &event : traceData->events) {
    // We've ingested this event already
    if (event.change_index <= symbolCache->lastTraceChangeCount)
      continue;

    assert(event.change_index <= traceData->change_count && "event change index too high");

    HierarchyItem *parent = &symbolCache->root;

    // frames are bottom-up by default
    iterateContainer(event.frames, viewPerspective != ViewPerspective::BottomUp, [&](auto &frame, bool hasMoreFrames) {
      generateHierarchyItems(currentHierarchyItems, event, frame);

      iterateContainer(currentHierarchyItems, viewPerspective != ViewPerspective::TopDown, [&](auto &item, bool hasMoreItems) {

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

          if (!needsReset) {
            auto parentModelIndex = selfModelIndex(parent);
            beginInsertRows(parentModelIndex, index, index);
          }

          auto itemPtr = std::make_unique<HierarchyItem>(std::move(item));
          matchedItem = itemPtr.get();
          parent->children.push_back(std::move(itemPtr));

          if (!needsReset) {
            endInsertRows();
          }
        }

        matchedItem->count++;

        if (!needsReset) {
          // Update the count column
          auto itemIndex = selfModelIndex(matchedItem, 1);
          dataChanged(itemIndex, itemIndex);
        }

        if (!hasMoreFrames && !hasMoreItems) {
          // contribute to self count

          matchedItem->selfCount++;

          if (!needsReset) {
            auto itemIndex = selfModelIndex(matchedItem, 2);
            dataChanged(itemIndex, itemIndex);
          }
        }

        // follow the chain...
        parent = matchedItem;

      });
    });
  }

  symbolCache->lastTraceChangeCount = traceData->change_count;

  if (needsReset) {
    endResetModel();
  }
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
      switch (index.column()) {
        case 0:
          return item->getName();
        case 1:
          return QString::number(item->count);
        case 2:
          return QString::number(item->selfCount);
        default:
          return QString("??? col: ") + QString::number(index.column());
      }
    case UserQt::UserSortRole:
      switch (index.column()) {
        case 0:
          return item->getName();
        case 1:
          return (int) item->count;
        case 2:
          return (int) item->selfCount;
        default:
          return QVariant();
      }
    default:
      return QVariant();
  }
}

void TraceHierarchyModel::setViewPerspective(TraceHierarchyModel::ViewPerspective viewPerspective) {
  updateModelTraces(viewPerspective, symbolCache->showInlineFuncs);
}

void TraceHierarchyModel::setShowInlineFuncs(bool showInlineFuncs) {
  updateModelTraces(symbolCache->viewPerspective, showInlineFuncs);
}

TraceHierarchyModel::ViewPerspective TraceHierarchyModel::getViewPerspective() {
  return symbolCache->viewPerspective;
}

bool TraceHierarchyModel::getShowInlineFuncs() {
  return symbolCache->showInlineFuncs;
}








