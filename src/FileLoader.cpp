//
// Created by Will Gulian on 12/4/20.
//

#include "FileLoader.h"

void FileLoader::loadFile(QString path) {

  auto error = debugTable->loadFile(path, true);
  if (error.length() != 0) {
    std::cout << "load error: " << error.toStdString() << "\n";
  }

  fileLoaded(path);
}

