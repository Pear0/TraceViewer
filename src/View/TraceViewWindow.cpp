//
// Created by Will Gulian on 12/4/20.
//

#include "TraceViewWindow.h"
#include "../DebugTable.h"

#include <QAction>
#include <QHeaderView>
#include <QMenuBar>
#include <QTimer>
#include <QThread>
#include <QGridLayout>
#include <QFileDialog>

TraceViewWindow::TraceViewWindow(std::shared_ptr<TraceData> trace_data,
                                 std::shared_ptr<DebugTable> debugTable)
        : QMainWindow(), trace_data(std::move(trace_data)), debugTable(debugTable) {
  auto *widget = new QWidget;
  setCentralWidget(widget);

  fileWatcher = new QFileSystemWatcher(this);

  traceTimeline = new TraceTimeline(this->trace_data, this);

  {
    treeWidget = new QTreeView(this);
    // treeWidget->setColumnCount(2);

    traceModel = new TraceHierarchyModel(this->trace_data, this->debugTable, this);
    traceModel->setViewPerspective(TraceHierarchyModel::ViewPerspective::TopDown);
    traceModel->setShowInlineFuncs(false);

    traceFilter = new TraceHierarchyFilterProxy(this);
    traceFilter->setSourceModel(traceModel);

    treeWidget->setModel(traceFilter);
    treeWidget->reset();

    // Logically the name column is first, but move it visually to the right side.
    treeWidget->header()->moveSection(0, treeWidget->header()->count() - 1);

    // All rows are 1 line. Massive perf improvement
    // when updates are frequent.
    treeWidget->setUniformRowHeights(true);
    treeWidget->setSortingEnabled(true);
  }

  {
    asmView = new QTableView(this);

    asmModel = new DisassemblyModel(this->trace_data, this->debugTable, this);

    asmView->setModel(asmModel);

    asmView->verticalHeader()->hide();
    asmView->verticalHeader()->setDefaultSectionSize(asmView->verticalHeader()->fontMetrics().height());
    asmView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
    asmView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::Stretch);
    asmView->setSelectionBehavior(QAbstractItemView::SelectRows);
    asmView->setShowGrid(false);
  }

  fileLoaderThread = new QThread(this);
  fileLoaderThread->start();


  fileLoader = new FileLoader(std::move(debugTable));
  fileLoader->moveToThread(fileLoaderThread);

  connect(treeWidget->selectionModel(), &QItemSelectionModel::currentChanged, this, &TraceViewWindow::traceSelectionChanged);

  connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &TraceViewWindow::onFileChanged);
  connect(fileWatcher, &QFileSystemWatcher::fileChanged, fileLoader, &FileLoader::loadFile);
  connect(this, &TraceViewWindow::doLoadFile, fileLoader, &FileLoader::loadFile);
  connect(fileLoader, &FileLoader::fileLoaded, this, &TraceViewWindow::fileLoaded);


  auto *mainLayout = new QGridLayout;

  mainLayout->addWidget(traceTimeline, 0, 0);
  mainLayout->addWidget(treeWidget, 1, 0);
  mainLayout->addWidget(asmView, 2, 0);
  mainLayout->setRowStretch(1, 1);
  mainLayout->setRowStretch(2, 1);

  widget->setLayout(mainLayout);

  createMenus();
}

void TraceViewWindow::onFileChanged(const QString &path) {
  std::cout << "file changed: " << path.toStdString() << std::endl;

}

void TraceViewWindow::createMenus() {

  exportTracesAct = new QAction("Export Traces...", this);
  importTracesAct = new QAction("Import Traces...", this);

  reloadTracesAct = new QAction("Reload Traces", this);
  reloadTracesAct->setShortcuts(QKeySequence::Refresh);

  connect(reloadTracesAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);

  autoReloadTracesAct = new QAction("Auto Reload Traces", this);
  autoReloadTracesAct->setCheckable(true);

  traceOrderGroup = new QActionGroup(this);
  traceOrderTopDownAct = new QAction("Top Down", traceOrderGroup);
  traceOrderTopDownAct->setShortcut(QKeySequence("Ctrl+7"));
  traceOrderTopDownAct->setCheckable(true);
  traceOrderTopDownAct->setChecked(traceModel->getViewPerspective() == TraceHierarchyModel::ViewPerspective::TopDown);
  traceOrderBottomUpAct = new QAction("Bottom Up", traceOrderGroup);
  traceOrderBottomUpAct->setShortcut(QKeySequence("Ctrl+8"));
  traceOrderBottomUpAct->setCheckable(true);
  traceOrderBottomUpAct->setChecked(traceModel->getViewPerspective() == TraceHierarchyModel::ViewPerspective::BottomUp);
  traceOrderTopFunctionsAct = new QAction("Top Functions", traceOrderGroup);
  traceOrderTopFunctionsAct->setCheckable(true);
  traceOrderTopFunctionsAct->setChecked(traceModel->getViewPerspective() == TraceHierarchyModel::ViewPerspective::TopFunctions);

  traceShowInlinedFuncsAct = new QAction("Show Inlined Funcs", this);
  traceShowInlinedFuncsAct->setShortcut(QKeySequence("Ctrl+I"));
  traceShowInlinedFuncsAct->setCheckable(true);
  traceShowInlinedFuncsAct->setChecked(traceModel->getShowInlineFuncs());

  customTraceWindowAct = new QAction("Custom Trace", this);
  customTraceWindowAct->setShortcut(QKeySequence("Ctrl+9"));

  auto *fileMenu = menuBar()->addMenu("File");
  fileMenu->addAction(exportTracesAct);
  fileMenu->addAction(importTracesAct);

  auto *viewMenu = menuBar()->addMenu("View");
  viewMenu->addAction(reloadTracesAct);
  viewMenu->addAction(autoReloadTracesAct);

  {
    auto *orderMenu = viewMenu->addMenu("Trace Order");
    orderMenu->addAction(traceOrderTopDownAct);
    orderMenu->addAction(traceOrderBottomUpAct);
    orderMenu->addAction(traceOrderTopFunctionsAct);
  }

  viewMenu->addAction(traceShowInlinedFuncsAct);

  viewMenu->addSeparator();

  auto *windowMenu = menuBar()->addMenu("Window");
  windowMenu->addAction(customTraceWindowAct);
  windowMenu->addSeparator();

  connect(exportTracesAct, &QAction::triggered, this, &TraceViewWindow::openExportTracesDialog);
  connect(importTracesAct, &QAction::triggered, this, &TraceViewWindow::openImportTracesDialog);

  // reloadTraces() inspects the current ordering, so all we need is to trigger a reload.
  connect(traceOrderTopDownAct, &QAction::triggered, this, &TraceViewWindow::tracesTopDownTriggered);
  connect(traceOrderBottomUpAct, &QAction::triggered, this, &TraceViewWindow::tracesBottomUpTriggered);
  connect(traceOrderTopFunctionsAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);
  connect(traceShowInlinedFuncsAct, &QAction::triggered, this, &TraceViewWindow::showInlineFuncsTriggered);

  connect(customTraceWindowAct, &QAction::triggered, this, &TraceViewWindow::openCustomTraceDialog);

}

void TraceViewWindow::reloadTraces() {
  std::cout << "reload" << std::endl;
  // TODO reset model
}

void TraceViewWindow::tracesBottomUpTriggered() {
  traceModel->setViewPerspective(TraceHierarchyModel::ViewPerspective::BottomUp);
}

void TraceViewWindow::tracesTopDownTriggered() {
  traceModel->setViewPerspective(TraceHierarchyModel::ViewPerspective::TopDown);
}

void TraceViewWindow::showInlineFuncsTriggered() {
  traceModel->setShowInlineFuncs(traceShowInlinedFuncsAct->isChecked());
}

void TraceViewWindow::fileLoaded(QString path) {
  std::cout << "reloaded " << path.toStdString() << std::endl;

}

void TraceViewWindow::openCustomTraceDialog() {

  if (!customTraceDialog) {
    customTraceDialog = new CustomTraceDialog(this);
  }

  customTraceDialog->show();
  customTraceDialog->raise();
  customTraceDialog->activateWindow();
}

void TraceViewWindow::addFile(const QString &path) {
  fileWatcher->addPath(path);
  doLoadFile(path);
}

void TraceViewWindow::openExportTracesDialog() {

  auto fileName = QFileDialog::getSaveFileName(
          this, "Export Traces", "/Users/will", "Traces (*.traces)");

  std::cout << "export file: " << fileName.toStdString() << "\n";

  trace_data->exportToFile(fileName);

}

void TraceViewWindow::openImportTracesDialog() {

  auto fileName = QFileDialog::getOpenFileName(
          this, "Import Traces", "/Users/will/traces", "Traces (*.traces)");

  std::cout << "import file: " << fileName.toStdString() << "\n";

  trace_data->importFromFile(fileName);

}

void TraceViewWindow::traceSelectionChanged(const QModelIndex &current, const QModelIndex &previous) {

  std::cout << "row: " << current.row() << ", col: " << current.column() << std::endl;

  auto sourceIndex = traceFilter->mapToSource(current);
  auto func = traceModel->getFunctionInfo(sourceIndex);
  if (!func) {
    std::cout << "null func!" << std::endl;
    return;
  }

  if (auto *func2 = dynamic_cast<ConcreteFunctionInfo *>(func); func2 != nullptr) {
    if (func2->ranges.empty()) {
      std::cout << "no ranges for func" << std::endl;
      return;
    }
    if (func2->ranges.size() != 1) {
      std::cout << "multiple ranges" << std::endl;
      return;
    }

    auto addrCounts = traceModel->getAddrCounts(sourceIndex);

    auto [start, end] = func2->ranges.front();
    asmModel->disassembleRegion(func2->buildId, start, end, &addrCounts);
    std::cout << "disassembled ranges" << std::endl;
  }

}



