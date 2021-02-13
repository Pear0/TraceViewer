//
// Created by Will Gulian on 1/2/21.
//

#include "DisassemblyInlinesModel.h"

DisassemblyInlinesModel::DisassemblyInlinesModel(
        std::shared_ptr<TraceData> traceData, std::shared_ptr<DebugTable> debugTable, QObject *parent)
        : QAbstractItemModel(parent), traceData(std::move(traceData)), debugTable(std::move(debugTable)), root(nullptr) {

}

DisassemblyInlinesModel::~DisassemblyInlinesModel() = default;

void DisassemblyInlinesModel::disassembleRegion(const ConcreteFunctionInfo &func, const std::unordered_map<uint64_t, size_t> *addrCounts) {
  auto [startAddress, endAddress] = func.ranges.front();

  beginResetModel();
  buildId = func.buildId;

  root.lines.clear();

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

  std::vector<CodeSection *> sectionStack;
  sectionStack.push_back(&root);

  for (auto &&row : std::move(segment.instructions)) {

    // roll back in the inline function tree
    while (sectionStack.back()->inlineInfo && !sectionStack.back()->inlineInfo->containsAddress(row.address)) {
      sectionStack.pop_back();
    }

    // traverse down into inline function tree
    {
      bool keepTraversing = true;
      while (keepTraversing) {
        keepTraversing = false;
        auto &inline_functions = (sectionStack.back()->inlineInfo ? sectionStack.back()->inlineInfo->inline_functions
                                                                  : func.inline_functions);
        for (auto &inlineFunc : inline_functions) {
          if (inlineFunc->containsAddress(row.address)) {

            std::optional<std::pair<QString, uint32_t>> sourceLine;
            auto info = std::make_unique<CodeSection>(inlineFunc.get(), 0, std::move(sourceLine));

            auto ptr = info.get();
            sectionStack.back()->lines.push_back(std::move(info));

            // push onto stack
            sectionStack.push_back(ptr);

            keepTraversing = true;
            break;
          }
        }
      }
    }

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

    auto info = std::make_unique<RowInfo>(std::move(row), samples, jumpOffset, row.jumpTarget, std::move(sourceLine));

    sectionStack.back()->lines.push_back(std::move(info));
  }

  root.assignChildIndices();

//  std::cout << "dis " << rows.size() << " instructions" << std::endl;

  endResetModel();
}

QModelIndex DisassemblyInlinesModel::index(int row, int column, const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto parentItem = static_cast<Common *>(parent.internalPointer());

    auto func = dynamic_cast<CodeSection *>(parentItem);
    assert(func && "not a func!");

    return createIndex(row, column, static_cast<void *>(func->lines.at(row).get()));
  }

  return createIndex(row, column, static_cast<void *>(root.lines.at(row).get()));
}

QModelIndex DisassemblyInlinesModel::parent(const QModelIndex &child) const {
  auto item = static_cast<Common *>(child.internalPointer());
  return selfModelIndex(item->parent);
}

int DisassemblyInlinesModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    auto item = static_cast<Common *>(parent.internalPointer());
    if (auto func = dynamic_cast<CodeSection *>(item); func) {
      return func->lines.size();
    } else {
      return 0;
    }
  }

  return root.lines.size();
}

int DisassemblyInlinesModel::columnCount(const QModelIndex &parent) const {
  return 3;
}

QVariant DisassemblyInlinesModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Orientation::Horizontal)
    return QVariant();

  if (role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
    case 0:
      return QString("Instruction");
    case 1:
      return QString("Samples");
    case 2:
      return QString("File");
    default:
      return QString("???");
  }
}

QVariant DisassemblyInlinesModel::data(const QModelIndex &index, int role) const {
  auto item = static_cast<Common *>(index.internalPointer());

  switch (index.column()) {
    case 0: {
      if (role != Qt::DisplayRole)
        return QVariant();

      if (auto inst = dynamic_cast<RowInfo *>(item); inst) {

        QString name = inst->instruction.mnemonic + " " + inst->instruction.operands;
        if (inst->jumpOffset) {
          name += "    < ";
          name += QString::number(inst->jumpOffset.value());
          name += " >";
        } else if (inst->jumpLocation) {
          if (auto it = debugTable->loadedFiles.find(buildId); it != debugTable->loadedFiles.end()) {
            if (auto funcs = it->second.resolve_address(inst->jumpLocation.value()); funcs) {
              name += "    < ";
              name += funcs->first->full_name;
              name += " >";
            }
          }
        }

        return name;

      } else {
        auto func = dynamic_cast<CodeSection *>(item);

        return func->inlineInfo->getFullName();
      }
    }
    case 1: {
      if (role != Qt::DisplayRole)
        return QVariant();

      if (auto inst = dynamic_cast<RowInfo *>(item); inst) {

        auto samples = inst->samples;
        if (samples == 0)
          return QString("");

        return QString::number(samples);

      } else {
        return QString("");
      }
    }
    case 2: {
      if (auto inst = dynamic_cast<RowInfo *>(item); inst) {

        auto &row = *inst;
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

      } else {
        auto func = dynamic_cast<CodeSection *>(item);
        if (!func->inlineInfo->sourceLine)
          return QVariant();

        if (role == Qt::DisplayRole) {
          auto file = func->inlineInfo->sourceLine->first.section('/', -1);
          file += ":";
          file += QString::number(func->inlineInfo->sourceLine->second);
          return file;
        } else if (role == Qt::ToolTipRole) {
          return func->inlineInfo->sourceLine->first + ":" + QString::number(func->inlineInfo->sourceLine->second);
        } else {
          return QVariant();
        }
      }

    }
    default: {
      if (role != Qt::DisplayRole)
        return QVariant();

      return QString("??? ") + QString::number(index.row());
    }
  }
}

QModelIndex DisassemblyInlinesModel::selfModelIndex(Common *item, int column) const {
  assert(item && "item was null");

  if (item == &root)
    return QModelIndex();

  return createIndex(item->selfIndex, column, static_cast<void *>(item));
}


