//
// Created by Will Gulian on 12/18/20.
//

#include "QtOutputStream.h"

void QtOutputStream::write(const void *buffer, size_t size) {
  file.write((const char *)buffer, size);
}

void QtOutputStream::write(kj::ArrayPtr<const kj::ArrayPtr<const uint8_t>> pieces) {
  for (auto &piece : pieces) {
    write(piece.begin(), piece.size());
  }
}
