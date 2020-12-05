//
// Created by Will Gulian on 12/4/20.
//

#ifndef TRACEVIEWER2_TRACEVIEWWINDOW_H
#define TRACEVIEWER2_TRACEVIEWWINDOW_H

#include <unordered_map>

#include <QMainWindow>
#include <QTreeWidget>
#include <QFileSystemWatcher>

#include "TraceData.h"
#include "DwarfInfo.h"
#include "DebugTable.h"
#include "FileLoader.h"

class TraceViewWindow : public QMainWindow {
  Q_OBJECT

  std::shared_ptr<TraceData> trace_data;
  std::shared_ptr<DebugTable> debugTable;

  QTimer *updateTimer = nullptr;
  QTreeWidget *treeWidget = nullptr;
  FileLoader *fileLoader = nullptr;
  QThread *fileLoaderThread = nullptr;
  QAction *reloadTracesAct = nullptr;
  QAction *autoReloadTracesAct = nullptr;

  uint64_t last_trace_change = 0;
public:
  QFileSystemWatcher *fileWatcher;

  explicit TraceViewWindow(std::shared_ptr<TraceData> trace_data, std::shared_ptr<DebugTable> debugTable);

private slots:
  void updateTimerHit();
  void onFileChanged(const QString &path);
  void reloadTraces();

  void fileLoaded(QString path);

private:
  void performReload(const TraceData &data);

  void createMenus();

  QList<QString> generateFunctionStack(const TraceEvent &event);


};


#endif //TRACEVIEWER2_TRACEVIEWWINDOW_H
