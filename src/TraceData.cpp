//
// Created by Will Gulian on 11/26/20.
//

#include <cstdio>
#include <iostream>

#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <kj/io.h>

#include "TraceData.h"
#include "schema/tracestreaming.capnp.h"

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

void TraceData::parse(uint8_t *data, size_t len) {
  if (len < 8) {
    std::cout << "received packet with size:" << len << std::endl;
    return;
  }

  kj::ArrayInputStream dataStream(kj::ArrayPtr(data, len));
  capnp::PackedMessageReader reader(dataStream);

  net_trace::TraceGroup::Reader root = reader.getRoot<net_trace::TraceGroup>();
  std::cout << "build id: " << root.getBuildId() << "\n";

  auto events = root.getEvents();
  for (auto it = events.begin(); it != events.end(); ++it) {
    TraceEvent event{};
    event.nanoseconds = it->getTimestamp();
    event.event_index = it->getEventIndex();

    auto frames = it->getFrames();
    for (auto it2 = frames.begin(); it2 != frames.end(); ++it2) {
      TraceFrame frame{};
      frame.pc = it2->getPc();

      event.frames.push_back(frame);
    }

    insert(event);
  }
}
