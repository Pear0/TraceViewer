//
// Created by Will Gulian on 12/4/20.
//

#include "TraceViewWindow.h"
#include "DebugTable.h"

#include <QAction>
#include <QMenuBar>
#include <QTimer>
#include <QThread>
#include <QGridLayout>


class FrameItem : public QTreeWidgetItem {
public:
  QString name;
  uint64_t count;

  FrameItem(QTreeWidget *w, QString name, uint64_t count = 1)
          : QTreeWidgetItem(w), name(std::move(name)), count(count) {

    auto c = QString::number(count);
    setText(1, c);

    setText(0, this->name);

  }

  ~FrameItem() override = default;

  void incrementCount() {
    count++;
    auto c = QString::number(count);
    setText(1, c);
  }

};


TraceViewWindow::TraceViewWindow(std::shared_ptr<TraceData> trace_data,
                                 std::shared_ptr<DebugTable> debugTable)
        : QMainWindow(), trace_data(std::move(trace_data)), debugTable(debugTable) {
  auto *widget = new QWidget;
  setCentralWidget(widget);

  updateTimer = new QTimer(this);
  updateTimer->setInterval(1000);

  fileWatcher = new QFileSystemWatcher(this);

  treeWidget = new QTreeWidget(this);
  treeWidget->setColumnCount(2);

  fileLoaderThread = new QThread(this);
  fileLoaderThread->start();


  fileLoader = new FileLoader(std::move(debugTable));
  fileLoader->moveToThread(fileLoaderThread);

  connect(updateTimer, &QTimer::timeout, this, &TraceViewWindow::updateTimerHit);
  connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &TraceViewWindow::onFileChanged);
  connect(fileWatcher, &QFileSystemWatcher::fileChanged, fileLoader, &FileLoader::loadFile);
  connect(this, &TraceViewWindow::doLoadFile, fileLoader, &FileLoader::loadFile);
  connect(fileLoader, &FileLoader::fileLoaded, this, &TraceViewWindow::fileLoaded);


  auto *mainLayout = new QGridLayout;

  mainLayout->addWidget(treeWidget);

  widget->setLayout(mainLayout);

  createMenus();

  updateTimer->start();
}

static void mergeFooBar(QTreeWidget *widget, QList<QTreeWidgetItem *> &topItems, QList<QString> &names) {
  FrameItem *parent = nullptr;

  for (size_t i = 0; i < names.size(); i++) {
    auto &name = names[i];
    if (i == 0) {
      auto it = std::find_if(topItems.begin(), topItems.end(), [&](QTreeWidgetItem *item) {
        return dynamic_cast<FrameItem *>(item)->name >= name;
      });

      if (it != topItems.end() && dynamic_cast<FrameItem *>(*it)->name == name) {
        // we match!
        parent = dynamic_cast<FrameItem *>(*it);
        parent->incrementCount();
      } else {
        // make new entry...
        auto *item = new FrameItem(widget, name);
        topItems.insert(it, item);
        parent = item;
      }
    } else {
      int j;
      for (j = 0; j < parent->childCount(); j++) {
        auto child = dynamic_cast<FrameItem *>(parent->child(j));
        if (child->name >= name)
          break;
      }

      if (j < parent->childCount() && dynamic_cast<FrameItem *>(parent->child(j))->name == name) {
        // we match!
        parent = dynamic_cast<FrameItem *>(parent->child(j));
        parent->incrementCount();
      } else {
        // make new entry...
        auto *item = new FrameItem(widget, name);
        parent->insertChild(j, item);
        parent = item;
      }
    }
  }
}

void TraceViewWindow::updateTimerHit() {
  if (!autoReloadTracesAct->isChecked()) {
    return;
  }

  TraceData data(*trace_data);

  // not locked access
  if (last_trace_change == data.change_count) {
    return;
  }

  performReload(data);
}

void TraceViewWindow::performReload(const TraceData &data) {
  QList<QTreeWidgetItem *> topItems;

  std::cout << "change: " << data.events.size() << " events - " << data.change_count << " != " << last_trace_change
            << std::endl;

  for (auto &events : data.events) {
    auto list = generateFunctionStack(events);

    if (traceOrderBottomUpAct->isChecked()) {
      std::reverse(list.begin(), list.end());
    }

    QList<QString> subList;
    if (traceOrderAllFuncsAct->isChecked()) {
      for (size_t i = 0; i < list.size(); i++) {
        subList.clear();
        for (size_t j = i; j < list.size(); j++) {
          subList.push_back(list[j]);
        }

        mergeFooBar(nullptr, topItems, subList);
      }
    } else {
      mergeFooBar(nullptr, topItems, list);
    }
  }

  treeWidget->clear();
  treeWidget->addTopLevelItems(topItems);
  last_trace_change = data.change_count;
}

void TraceViewWindow::onFileChanged(const QString &path) {
  std::cout << "file changed: " << path.toStdString() << std::endl;

}

QList<QString> TraceViewWindow::generateFunctionStack(const TraceEvent &event) {
  QList<QString> list;
//  list.push_back("main");

  std::lock_guard<std::mutex> _guard(debugTable->lock);

  for (auto it = event.frames.crbegin(); it != event.frames.crend(); ++it) {
    auto &frame = *it;
    auto dwarfInfo = debugTable->loadedFiles.find(event.build_id);
    if (dwarfInfo != debugTable->loadedFiles.end() && frame.pc >= 0x1000) {
      auto funcs = dwarfInfo->second.resolve_address(frame.pc);
      if (funcs) {
        auto &[main_func, inlines] = *funcs;
        list.push_back(main_func->full_name);

        if (traceShowInlinedFuncsAct->isChecked()) {
          for (auto inline_func : inlines) {
            if (inline_func->origin_fn) {
              QString name("~");
              name += inline_func->origin_fn->full_name;
              list.push_back(name);
            }
          }
        }

        // don't generate address literal
        continue;
      }
    }

    QString name("0x");
    name += QString::number(frame.pc, 16);
    list.push_back(name);
  }


//  for (auto &frame : event.frames) {
//    if (auto it = symbolication_cache.find(frame.pc); it != symbolication_cache.end()) {
//      list.push_back(it->second);
//    } else if (debug_info) {
//      auto [name, _] = debug_info.value()->symbolicate(frame.pc);
//      symbolication_cache[frame.pc] = name;
//      list.push_back(name);
//    } else {
//      QString name("0x");
//      name += QString::number(frame.pc, 16);
//      list.push_back(name);
//    }
//  }

  return list;
}

void TraceViewWindow::createMenus() {

  reloadTracesAct = new QAction("Reload Traces", this);
  reloadTracesAct->setShortcuts(QKeySequence::Refresh);

  connect(reloadTracesAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);

  autoReloadTracesAct = new QAction("Auto Reload Traces", this);
  autoReloadTracesAct->setCheckable(true);

  traceOrderGroup = new QActionGroup(this);
  traceOrderTopDownAct = new QAction("Top Down", traceOrderGroup);
  traceOrderTopDownAct->setShortcut(QKeySequence("Ctrl+7"));
  traceOrderTopDownAct->setCheckable(true);
  traceOrderTopDownAct->setChecked(true);
  traceOrderBottomUpAct = new QAction("Bottom Up", traceOrderGroup);
  traceOrderBottomUpAct->setShortcut(QKeySequence("Ctrl+8"));
  traceOrderBottomUpAct->setCheckable(true);

  traceOrderAllFuncsAct = new QAction("All First", this);
  traceOrderAllFuncsAct->setCheckable(true);

  traceShowInlinedFuncsAct = new QAction("Show Inlined Funcs", this);
  traceShowInlinedFuncsAct->setShortcut(QKeySequence("Ctrl+I"));
  traceShowInlinedFuncsAct->setCheckable(true);
  traceShowInlinedFuncsAct->setChecked(true);

  customTraceWindowAct = new QAction("Custom Trace", this);
  customTraceWindowAct->setShortcut(QKeySequence("Ctrl+9"));

  auto *viewMenu = menuBar()->addMenu("View");
  viewMenu->addAction(reloadTracesAct);
  viewMenu->addAction(autoReloadTracesAct);

  {
    auto *orderMenu = viewMenu->addMenu("Trace Order");
    orderMenu->addAction(traceOrderTopDownAct);
    orderMenu->addAction(traceOrderBottomUpAct);
  }

  viewMenu->addAction(traceOrderAllFuncsAct);
  viewMenu->addAction(traceShowInlinedFuncsAct);

  viewMenu->addSeparator();

  auto *windowMenu = menuBar()->addMenu("Window");
  windowMenu->addAction(customTraceWindowAct);
  windowMenu->addSeparator();

  // reloadTraces() inspects the current ordering, so all we need is to trigger a reload.
  connect(traceOrderTopDownAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);
  connect(traceOrderBottomUpAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);
  connect(traceOrderAllFuncsAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);
  connect(traceShowInlinedFuncsAct, &QAction::triggered, this, &TraceViewWindow::reloadTraces);

  connect(customTraceWindowAct, &QAction::triggered, this, &TraceViewWindow::openCustomTraceDialog);

}

void TraceViewWindow::reloadTraces() {
  std::cout << "reload" << std::endl;

  TraceData data(*trace_data);
  performReload(data);

  // debugTable->loadFile("/Users/will/Work/Pear0/cs3210-rustos/kern/build/kernel.elf", true);

}

void TraceViewWindow::fileLoaded(QString path) {
  std::cout << "reloaded " << path.toStdString() << std::endl;

  TraceData data(*trace_data);
  performReload(data);


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

