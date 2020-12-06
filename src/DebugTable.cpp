//
// Created by Will Gulian on 11/26/20.
//

#include <iostream>

extern "C" {
  // #include "dwarf.h"
  #include "libdwarf.h"
}

#include "DebugTable.h"
#include "DwarfInfo.h"


QString DebugTable::loadFile(const QString &file, bool replace) {

  auto loader = DwarfLoader::openFile(file.toUtf8().data());
  if (loader.is_error()) {
    return loader.as_error();
  }

  auto buildId = loader.as_value().getBuildId();
  if (!replace && hasBuildId(buildId)) {
    return QString();
  }

  auto loaded = loader.as_value().load();
  if (loaded.is_error()) {
    return loaded.as_error();
  }

  std::lock_guard<std::mutex> _guard(lock);
  loadedFiles.insert({ buildId, std::move(loaded).into_value() });

  std::cout << "loaded build id 0x" << std::hex << buildId << std::hex << "\n";

  return QString();
}
