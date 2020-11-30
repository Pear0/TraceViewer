//
// Created by Will Gulian on 11/26/20.
//

#include <cstdio>
#include <iostream>
#include "TraceData.h"

/* all values little endian
 *
 * Packet:
 *    MAGIC - 4 bytes
 *    VERSION - 2 bytes
 *    num_events - 2 bytes
 *    EVENTS:                                  | events may or may not be ordered by timestamp
 *      timestamp - 8 bytes in nanoseconds
 *      event_index - 8 bytes
 *      num_frames - 2 bytes
 *      FRAMES:                                | the "topmost"/"current" frame is first
 *        pc - 8 bytes
 *
 */

#define MAGIC 0x54445254
#define VERSION 1

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

  uint8_t *data_end = data + len;

  uint32_t magic = *(uint32_t *)(data);
  uint16_t version = *(uint16_t *)(data + 4);
  uint16_t num_events = *(uint16_t *)(data + 6);

  if (magic != MAGIC) {
    std::cout << "discarding bad magic 0x" << std::hex << magic << std::dec << std::endl;
    return;
  }

  if (version != VERSION) {
    std::cout << "unknown version number = " << version << std::endl;
    return;
  }

  std::cout << "num_events = " << num_events << std::endl;

  data += 8; // pointer at start of events

  for (uint16_t event_i = 0; event_i < num_events; event_i++) {
    TraceEvent event{};

    if (data_end - data < 18) {
      std::cout << "parse error during event_i = " << event_i << std::endl;
      return;
    }

    event.nanoseconds = *(uint64_t *)(data);
    event.event_index = *(uint64_t *)(data + 8);
    uint16_t num_frames = *(uint16_t *)(data + 16);

    std::cout << "num_frames = " << num_frames << std::endl;

    data += 18; // point to frames

    for (uint16_t frame_i = 0; frame_i < num_frames; frame_i++) {
      TraceFrame frame{};

      if (data_end - data < 8) {
        std::cout << "parse error during event_i = " << event_i << ", frame_i = " << frame_i << std::endl;
        return;
      }

      frame.pc = *(uint64_t *)(data);
      data += 8;

      event.frames.push_back(frame);
    }

    insert(event);
  }
}
