//
// Created by Will Gulian on 11/26/20.
//

#include <cstdio>
#include <iostream>
#include <unordered_map>

#include <QFile>

#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <kj/io.h>

#include "TraceData.h"
#include "schema/tracestreaming.capnp.h"
#include "QtOutputStream.h"

TraceData::TraceData(const TraceData &other) : events(), change_count(0) {
  const std::lock_guard<std::mutex> g(lock);
  events = other.events;
  change_count = other.change_count;
}

void TraceData::insert(TraceEvent event) {
  const std::lock_guard<std::mutex> g(lock);
  change_count++;

  auto it = events.end();
  while (it != events.begin()) {
    it--;

    if (it->nanoseconds < event.nanoseconds) {
      it++;
      events.insert(it, event);
      return;
    }
  }

  events.insert(events.begin(), event);
}

static void parseTraceGroup(TraceData *data, net_trace::TraceGroup::Reader &root) {
  std::cout << "build id: " << root.getBuildId() << "\n";

  auto events = root.getEvents();
  for (auto it = events.begin(); it != events.end(); ++it) {
    TraceEvent event{};
    event.nanoseconds = it->getTimestamp();
    event.build_id = root.getBuildId();
    event.event_index = it->getEventIndex();

    auto frames = it->getFrames();
    for (auto it2 = frames.begin(); it2 != frames.end(); ++it2) {
      TraceFrame frame{};
      frame.pc = it2->getPc();

      event.frames.push_back(frame);
    }

    data->insert(event);
  }
}

void TraceData::parse(uint8_t *data, size_t len) {
  if (len < 8) {
    std::cout << "received packet with size:" << len << std::endl;
    return;
  }

  kj::ArrayInputStream dataStream(kj::ArrayPtr(data, len));
  capnp::PackedMessageReader reader(dataStream);

  net_trace::TraceGroup::Reader root = reader.getRoot<net_trace::TraceGroup>();
  parseTraceGroup(this, root);
}

void TraceData::exportToFile(QString name) {
  const std::lock_guard<std::mutex> g(lock);

  std::unordered_map<uint64_t, size_t> buildIds;

  for (auto &event : events) {
    buildIds[event.build_id]++;
  }

  ::capnp::MallocMessageBuilder message;

  net_trace::SavedTraces::Builder saved = message.initRoot<net_trace::SavedTraces>();
  auto cGroups = saved.initGroups(buildIds.size());

  size_t groupIdx = 0;
  for (auto [buildId, count] : buildIds) {
    auto cGroup = cGroups[groupIdx++];
    cGroup.setBuildId(buildId);

    auto evIt = events.begin();

    auto cEvents = cGroup.initEvents(count);
    for (size_t eventIdx = 0; eventIdx < count; eventIdx++) {
      auto cEvent = cEvents[eventIdx];

      // find a matching event.
      while (evIt->build_id != buildId) evIt++;

      auto &event = *evIt;

      cEvent.setEventIndex(event.event_index);
      cEvent.setTimestamp(event.nanoseconds);

      auto cFrames = cEvent.initFrames(event.frames.size());
      for (size_t frameIdx = 0; frameIdx < event.frames.size(); frameIdx++) {
        cFrames[frameIdx].setPc(event.frames[frameIdx].pc);
      }

      evIt++;
    }

  }

  QFile file(name);
  if (!file.open(QIODevice::WriteOnly))
    return;

  QtOutputStream stream(file);
  ::capnp::writePackedMessage(stream, message);

}

void TraceData::importFromFile(QString name) {
  QFile file(name);
  if (!file.open(QIODevice::ReadOnly))
    return;

  auto data = file.map(0, file.size());

  kj::ArrayInputStream dataStream(kj::ArrayPtr(data, file.size()));
  capnp::PackedMessageReader reader(dataStream);

  net_trace::SavedTraces::Reader root = reader.getRoot<net_trace::SavedTraces>();

  for (auto group : root.getGroups()) {
    parseTraceGroup(this, group);
  }

}

void TraceData::generateTimeline(Timeline &timeline, int width) {
  const std::lock_guard<std::mutex> g(lock);

  timeline.newest_timestamp = std::numeric_limits<uint64_t>::min();
  timeline.oldest_timestamp = std::numeric_limits<uint64_t>::max();

  for (auto &event : events) {
    timeline.oldest_timestamp = std::min(timeline.oldest_timestamp, event.nanoseconds);
    timeline.newest_timestamp = std::max(timeline.newest_timestamp, event.nanoseconds);
  }

  timeline.columns.clear();
  {
    TimelineEntry entry;
    entry.valid = true;
    timeline.columns.resize(width, entry);
  }

  if (events.empty())
    return;

  for (auto &event : events) {
    size_t x = timeline.timeToIndex(event.nanoseconds);
    auto &entry = timeline.columns.at(x);
    entry.count++;
    entry.oldest_timestamp = std::min(entry.oldest_timestamp, event.nanoseconds);
    entry.newest_timestamp = std::max(entry.newest_timestamp, event.nanoseconds);
  }

}
