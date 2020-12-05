#include <QApplication>
#include <QQuickView>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QMenuBar>

#include "ListeningThread.h"
#include "DebugTable.h"
#include "TraceModel.h"
#include "DwarfInfo.h"
#include "TraceViewWindow.h"

int main(int argc, char **argv) {
  auto *app = new QApplication(argc, argv);

//    QPushButton button("Hello world !");
//    button.show();

//    QQuickView view;
//    view.setSource(QUrl::fromLocalFile("../TestComponent.qml"));
//    view.show();

  const char *file_name = "/Users/will/Work/Pear0/cs3210-rustos/kern/build/kernel.elf";

  auto debugTable = std::make_shared<DebugTable>();
  auto data = std::make_shared<TraceData>();

  auto *foo = new TraceViewWindow(data, debugTable);

  QString file_path(file_name);
  foo->fileWatcher->addPath(file_path);

  foo->resize(1280 / 2, 720);
  foo->show();

  auto *t = new ListeningThread(data);
  t->start();

  return app->exec();
}