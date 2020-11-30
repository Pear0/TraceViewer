//
// Created by Will Gulian on 11/27/20.
//

#ifndef TRACEVIEWER2_TRACEVIEWWINDOW_H
#define TRACEVIEWER2_TRACEVIEWWINDOW_H

#include <unordered_map>
#include <QApplication>
#include <QFileSystemWatcher>
#include <QWidget>
#include <QTreeWidget>
#include "TraceData.h"
#include "DwarfInfo.h"

class TraceViewWindow : public QWidget {
  Q_OBJECT

  std::shared_ptr<TraceData> trace_data;
  std::optional<std::shared_ptr<DwarfInfo>> debug_info;
  QTimer *updateTimer;
  QTreeWidget *treeWidget;

  uint64_t last_trace_change = 0;
  std::unordered_map<uint64_t, QString> symbolication_cache{};
public:
  QFileSystemWatcher *fileWatcher;

  explicit TraceViewWindow(std::shared_ptr<TraceData> trace_data, std::optional<std::shared_ptr<DwarfInfo>> debug_info);

  void updateTimerHit();
  void onFileChanged(const QString &path);

private:
  QList<QString> generateFunctionStack(const TraceEvent &event);

};


#endif //TRACEVIEWER2_TRACEVIEWWINDOW_H
