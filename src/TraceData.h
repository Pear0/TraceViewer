//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_TRACEDATA_H
#define TRACEVIEWER2_TRACEDATA_H

#include <mutex>
#include <vector>

struct TraceFrame {
  uint64_t pc;
};

struct TraceEvent {
  uint64_t nanoseconds;
  uint64_t build_id;
  uint64_t event_index;
  std::vector<TraceFrame> frames;
};

class TraceData {
  std::mutex lock{};


public:
  // sorted by nanoseconds
  std::vector<TraceEvent> events{};
  uint64_t change_count;

public:
  TraceData() = default;
  TraceData(const TraceData &);

  void insert(TraceEvent event);

  void parse(uint8_t *data, size_t len);

};


#endif //TRACEVIEWER2_TRACEDATA_H
