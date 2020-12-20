#include <QApplication>

#include "ListeningThread.h"
#include "DebugTable.h"
#include "View/TraceViewWindow.h"

int main(int argc, char **argv) {
  auto *app = new QApplication(argc, argv);

  const char *file_name = "/Users/will/Work/Pear0/cs3210-rustos/kern/build/kernel.elf";

  auto debugTable = std::make_shared<DebugTable>();
  auto data = std::make_shared<TraceData>();

  auto *foo = new TraceViewWindow(data, debugTable);

  QString file_path(file_name);
  foo->addFile(file_path);

  foo->resize(1280 / 2, 720);
  foo->show();

  auto *t = new ListeningThread(data);
  t->start();

  return app->exec();
}