//
// Created by Will Gulian on 12/18/20.
//

#ifndef TRACEVIEWER2_QTOUTPUTSTREAM_H
#define TRACEVIEWER2_QTOUTPUTSTREAM_H

#include <QFile>

#include <kj/io.h>

class QtOutputStream : public kj::OutputStream {

public:
  explicit QtOutputStream(QFile &file): file(file) {}
  KJ_DISALLOW_COPY(QtOutputStream);
  ~QtOutputStream() noexcept(false) override = default;

  void write(const void* buffer, size_t size) override;
  void write(kj::ArrayPtr<const kj::ArrayPtr<const uint8_t>> pieces) override;


private:
  QFile &file;
};


#endif //TRACEVIEWER2_QTOUTPUTSTREAM_H
