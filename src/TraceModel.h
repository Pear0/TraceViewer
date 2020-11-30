//
// Created by Will Gulian on 11/27/20.
//

#ifndef TRACEVIEWER2_TRACEMODEL_H
#define TRACEVIEWER2_TRACEMODEL_H

#include <iostream>
#include <map>
#include <vector>

#include <QAbstractItemModel>
#include <QTimer>
#include <QVariant>
#include "TraceData.h"

struct Entry {
  uint64_t address;
  size_t count;
};

class TraceModel : public QAbstractItemModel {
Q_OBJECT
private:
  QTimer *timer;
  std::shared_ptr<TraceData> trace_data;
  std::vector<Entry> addresses;

public:

  explicit TraceModel(std::shared_ptr<TraceData> trace_data, QObject *parent = nullptr)
          : QAbstractItemModel(parent), trace_data(std::move(trace_data)), timer(new QTimer(this)) {
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &TraceModel::timerHit);
    timer->start();
  }

  void timerHit() {
    std::cout << "timer" << std::endl;

    emit beginResetModel();

    addresses.clear();

    TraceData trace_data(*this->trace_data);

    std::map<uint64_t, Entry> entries;

    for (auto &event : trace_data.events) {
      for (auto &frame : event.frames) {
        Entry &entry = entries[frame.pc];
        entry.address = frame.pc;
        entry.count++;
      }
    }

    for (auto [_, v] : entries) {
      addresses.push_back(v);
    }

    emit endResetModel();

  }

  void bruh() {

  }

  [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent) const override {
    return createIndex(row, column, parent.isValid() ? parent.internalId() + 1 : (uintptr_t) 0);
  }

  [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override {
    if (child.internalId() == 0) {
      return QModelIndex();
    }
    return createIndex(child.row(), child.column(), (uintptr_t) (child.internalId() - 1));
  }

  [[nodiscard]] int rowCount(const QModelIndex &parent) const override {
    return addresses.size();
  }

  [[nodiscard]] int columnCount(const QModelIndex &parent) const override {
    return 2;
  }

  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override {

    if (role == Qt::DisplayRole && index.row() < addresses.size()) {
      switch (index.column()) {
        case 0:
          return QString::number(addresses[index.row()].address);
        case 1:
          return QString::number(addresses[index.row()].count);
      }
    }

    return QVariant();
  }

};


#endif //TRACEVIEWER2_TRACEMODEL_H
