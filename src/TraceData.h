//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_TRACEDATA_H
#define TRACEVIEWER2_TRACEDATA_H

#include <mutex>
#include <vector>

#include <QString>

struct TraceFrame {
  uint64_t pc;
};

struct TraceEvent {
  uint64_t nanoseconds;
  uint64_t build_id;
  uint64_t event_index;
  std::vector<TraceFrame> frames;
};

struct TimelineEntry {
  int count { 0 };
  uint64_t oldest_timestamp { std::numeric_limits<uint64_t>::max() };
  uint64_t newest_timestamp { std::numeric_limits<uint64_t>::min() };
  bool valid { false };
};

struct Timeline {
  std::vector<TimelineEntry> columns;
  uint64_t oldest_timestamp { std::numeric_limits<uint64_t>::max() };
  uint64_t newest_timestamp { std::numeric_limits<uint64_t>::min() };

  [[nodiscard]] inline bool valid() const {
    return oldest_timestamp <= newest_timestamp;
  }

  inline size_t timeToIndex(uint64_t time) {
    return (time - oldest_timestamp) * columns.size() / (newest_timestamp - oldest_timestamp + 1);
  }

  inline std::pair<uint64_t, uint64_t> indexToTime(size_t index) {
    auto x = index * (newest_timestamp - oldest_timestamp) / columns.size() + oldest_timestamp;
    auto y = (index+1) * (newest_timestamp - oldest_timestamp) / columns.size() + oldest_timestamp;
    return std::make_pair(x, y);
  }
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

  void exportToFile(QString file);

  void importFromFile(QString file);

  void generateTimeline(Timeline &timeline, int width);

};


#endif //TRACEVIEWER2_TRACEDATA_H
