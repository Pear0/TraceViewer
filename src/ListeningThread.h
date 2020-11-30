//
// Created by Will Gulian on 11/25/20.
//

#ifndef TRACEVIEWER2_LISTENINGTHREAD_H
#define TRACEVIEWER2_LISTENINGTHREAD_H

#include <atomic>

#include <QThread>

#include "TraceData.h"

class ListeningThread : public QThread {

  Q_OBJECT

private:
  std::shared_ptr<TraceData> trace_data;
public:
  explicit ListeningThread(std::shared_ptr<TraceData> trace_data) : QThread(), trace_data(std::move(trace_data)) {}

  std::atomic<bool> should_close{false};

  void run() override;

};


#endif //TRACEVIEWER2_LISTENINGTHREAD_H
