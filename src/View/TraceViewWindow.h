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

#include "TraceTimeline.h"
#include "../TraceData.h"
#include "../DwarfInfo.h"
#include "../DebugTable.h"
#include "../FileLoader.h"
#include "CustomTraceDialog.h"
#include "../Model/TraceHierarchyModel.h"
#include "../Model/TraceHierarchyFilterProxy.h"

class TraceViewWindow : public QMainWindow {
Q_OBJECT

  TraceTimeline *traceTimeline = nullptr;
  QTreeView *treeWidget = nullptr;
  TraceHierarchyModel *traceModel = nullptr;
  TraceHierarchyFilterProxy *traceFilter = nullptr;
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
  QAction *traceOrderTopFunctionsAct = nullptr;
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

  void onFileChanged(const QString &path);

  void reloadTraces();

  void tracesBottomUpTriggered();

  void tracesTopDownTriggered();

  void showInlineFuncsTriggered();

  void fileLoaded(QString path);

  void openCustomTraceDialog();

  void openExportTracesDialog();

  void openImportTracesDialog();

private:
  void createMenus();

};


#endif //TRACEVIEWER2_TRACEVIEWWINDOW_H
