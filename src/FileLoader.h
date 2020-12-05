//
// Created by Will Gulian on 12/4/20.
//

#ifndef TRACEVIEWER2_FILELOADER_H
#define TRACEVIEWER2_FILELOADER_H

#include <memory>

#include <QObject>

#include "DebugTable.h"

class FileLoader : public QObject {
Q_OBJECT

  std::shared_ptr<DebugTable> debugTable;
public:
  FileLoader(std::shared_ptr<DebugTable> debugTable)
          : QObject(), debugTable(std::move(debugTable)) {}


public slots:

  void loadFile(QString path);

signals:

  void fileLoaded(QString path);

};


#endif //TRACEVIEWER2_FILELOADER_H
