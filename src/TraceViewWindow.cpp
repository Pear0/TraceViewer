//
// Created by Will Gulian on 11/27/20.
//

#include <iostream>

#include <QGridLayout>
#include <QTimer>
#include "TraceViewWindow.h"

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
                                 std::optional<std::shared_ptr<DwarfInfo>> debug_info)
        : QWidget(), trace_data(std::move(trace_data)), debug_info(std::move(debug_info)),
          updateTimer(new QTimer(this)), fileWatcher(new QFileSystemWatcher(this)),
          treeWidget(new QTreeWidget()) {
  setContentsMargins(0, 0, 0, 0);

  updateTimer->setInterval(1000);
  connect(updateTimer, &QTimer::timeout, this, &TraceViewWindow::updateTimerHit);

  connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &TraceViewWindow::onFileChanged);

  treeWidget->setColumnCount(2);

  auto *mainLayout = new QGridLayout;

  mainLayout->addWidget(treeWidget);

  setLayout(mainLayout);


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

  TraceData data(*trace_data);

  // not locked access
  if (last_trace_change == data.change_count) {
    std::cout << "no change" << std::endl;
    return;
  }

  QList<QTreeWidgetItem *> topItems;

  std::cout << "change: " << data.events.size() << " events - " << data.change_count << " != " << last_trace_change
            << std::endl;

  for (auto &events : data.events) {
    auto list = generateFunctionStack(events);
    mergeFooBar(nullptr, topItems, list);
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

  for (auto it = event.frames.crbegin(); it != event.frames.crend(); ++it) {
    auto &frame = *it;
    if (debug_info && frame.pc >= 0x1000) {
      auto funcs = debug_info.value()->resolve_address(frame.pc);
      if (funcs) {
        auto &[main_func, inlines] = *funcs;
        list.push_back(main_func->full_name);

        for (auto inline_func : inlines) {
          if (inline_func->origin_fn) {
            QString name("~");
            name += inline_func->origin_fn->full_name;
            list.push_back(name);
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


