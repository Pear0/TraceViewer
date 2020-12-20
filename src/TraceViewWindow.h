//
// Created by Will Gulian on 12/4/20.
//

#ifndef TRACEVIEWER2_TRACEVIEWWINDOW_H
#define TRACEVIEWER2_TRACEVIEWWINDOW_H

#include <unordered_map>

#include <QAction>
#include <QActionGroup>
#include <QFileSystemWatcher>
#include <QMainWindow>
#include <QTimer>
#include <QTreeWidget>

#include "View/TraceTimeline.h"
#include "TraceData.h"
#include "DwarfInfo.h"
#include "DebugTable.h"
#include "FileLoader.h"
#include "CustomTraceDialog.h"
#include "TraceHierarchyModel.h"

class TraceViewWindow : public QMainWindow {
Q_OBJECT

  QTimer *updateTimer = nullptr;
  TraceTimeline *traceTimeline = nullptr;
  QTreeView *treeWidget = nullptr;
  TraceHierarchyModel *traceModel = nullptr;
  FileLoader *fileLoader = nullptr;
  QThread *fileLoaderThread = nullptr;
  CustomTraceDialog *customTraceDialog = nullptr;

  QAction *exportTracesAct = nullptr;
  QAction *importTracesAct = nullptr;
  QAction *reloadTracesAct = nullptr;
  QAction *autoReloadTracesAct = nullptr;
  QActionGroup *traceOrderGroup = nullptr;
  QAction *traceOrderTopDownAct = nullptr;
  QAction *traceOrderBottomUpAct = nullptr;
  QAction *traceOrderAllFuncsAct = nullptr;
  QAction *traceShowInlinedFuncsAct = nullptr;
  QAction *customTraceWindowAct = nullptr;

  uint64_t last_trace_change = 0;
public:
  std::shared_ptr<TraceData> trace_data;
  std::shared_ptr<DebugTable> debugTable;
  QFileSystemWatcher *fileWatcher;

  explicit TraceViewWindow(std::shared_ptr<TraceData> trace_data, std::shared_ptr<DebugTable> debugTable);

  void addFile(const QString &path);

signals:
  void doLoadFile(const QString &path);

private slots:

  void updateTimerHit();

  void onFileChanged(const QString &path);

  void reloadTraces();

  void fileLoaded(QString path);

  void openCustomTraceDialog();

  void openExportTracesDialog();

  void openImportTracesDialog();

private:
  void performReload(const TraceData &data);

  void createMenus();

  QList<QString> generateFunctionStack(const TraceEvent &event);


};


#endif //TRACEVIEWER2_TRACEVIEWWINDOW_H
