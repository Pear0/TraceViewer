//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_TRACEDATA_H
#define TRACEVIEWER2_TRACEDATA_H

#include <mutex>
#include <vector>

#include <QString>
#include <QTimer>

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

class TraceData : public QObject {
  Q_OBJECT

  std::mutex lock{};

  QTimer *updateDebouncer = nullptr;

public:
  // sorted by nanoseconds
  std::vector<TraceEvent> events{};
  uint64_t change_count { 0 };

public:
  TraceData();
  TraceData(const TraceData &);

signals:
  /// Triggered after a cool down period whenever the data changes.
  void dataChanged();

  /// Mark that data model has changed, and an update should be triggered.
  /// Thread-safe.
  void markChange();

public:
  void insert(TraceEvent event);

  void parse(uint8_t *data, size_t len);

  void exportToFile(QString file);

  void importFromFile(QString file);

  void generateTimeline(Timeline &timeline, int width);

private slots:
  /// Timer has elapsed -> forward to dataChanged().
  void debounceTriggered();

  /// triggered by markChange() after crossing thread boundaries.
  void rawDataChanged();

private:
  void initialize();

};


#endif //TRACEVIEWER2_TRACEDATA_H
