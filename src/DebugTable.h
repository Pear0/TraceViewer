//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_DEBUGTABLE_H
#define TRACEVIEWER2_DEBUGTABLE_H

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "DwarfInfo.h"

class DebugTable {

public:
  std::unordered_map<uint64_t, DwarfInfo> loadedFiles;
  std::mutex lock;

  DebugTable() = default;

  QString loadFile(const QString &file, bool replace = false);

  bool hasBuildId(uint64_t id) {
    return loadedFiles.find(id) != loadedFiles.end();
  }

};


#endif //TRACEVIEWER2_DEBUGTABLE_H
