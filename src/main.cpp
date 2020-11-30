#include <QApplication>
#include <QQuickView>
#include <QVBoxLayout>
#include <QTreeWidget>
#include "ListeningThread.h"
#include "DebugTable.h"
#include "TraceModel.h"
#include "TraceViewWindow.h"
#include "DwarfInfo.h"

int main(int argc, char **argv) {
  auto *app = new QApplication(argc, argv);

//    QPushButton button("Hello world !");
//    button.show();

//    QQuickView view;
//    view.setSource(QUrl::fromLocalFile("../TestComponent.qml"));
//    view.show();

  const char *file_name = "/Users/will/Work/Pear0/cs3210-rustos/kern/build/kernel.elf";

  auto v = DwarfInfo::load_file(file_name);

  std::optional<std::shared_ptr<DwarfInfo>> debug_info;

  if (v.is_ok()) {
    std::cout << "loaded!" << std::endl;

    v.as_value().bruh();

    std::cout << "done!" << std::endl;

    debug_info = std::move(std::make_shared<DwarfInfo>(std::move(std::move(v).into_value())));
  } else {
    std::cout << "error: " << v.as_error() << std::endl;
  }

//  auto *tree = new QTreeWidget;
//  tree->setColumnCount(1);
//
//  QList<QTreeWidgetItem *> items;
//  for (int i = 0; i < 10; ++i) {
//    auto *item = new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList(QString("item: %1").arg(i)));
//
//    item->addChild(new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList(QString("item: %1").arg(i))));
//
//    items.append(item);
//  }
//
//  tree->insertTopLevelItems(0, items);
//
//  tree->show();

  auto data = std::make_shared<TraceData>();

//  auto *tree2 = new QTreeView;
//  auto *mod = new TraceModel(data, tree2);
//
//
//  tree2->setModel(mod);
//  tree2->update();
//
//  tree2->resize(1280 / 2, 720);
//  tree2->show();

  auto *foo = new TraceViewWindow(data, debug_info);

  QString file_path(file_name);
  foo->fileWatcher->addPath(file_path);

  foo->resize(1280 / 2, 720);
  foo->show();

//  mod->bruh();

  auto *t = new ListeningThread(data);
  t->start();

  return app->exec();
}