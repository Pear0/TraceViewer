//
// Created by Will Gulian on 12/19/20.
//

#include <iostream>

#include <QMouseEvent>
#include <QPainter>

#include "TraceTimeline.h"

TraceTimeline::TraceTimeline(std::shared_ptr<TraceData> traceData, QWidget *parent) : QWidget(parent), traceData(std::move(traceData)) {
  setFixedHeight(100);

}

void TraceTimeline::paintEvent(QPaintEvent *event) {

  traceData->generateTimeline(timeline, width());

  int maxCount = 0;
  for (auto col : timeline.columns) {
    maxCount = std::max(maxCount, col.count);
  }

  QPainter p(this);

  p.setPen(Qt::PenStyle::NoPen);
  p.setBrush(palette().base());
  p.drawRect(QRect(0, 0, width(), height()));

  if (mousePressEntry.valid && mouseReleaseEntry.valid && timeline.valid()) {
    auto oldTime = std::min(mousePressEntry.oldest_timestamp, mouseReleaseEntry.oldest_timestamp);
    auto newTime = std::max(mousePressEntry.newest_timestamp, mouseReleaseEntry.newest_timestamp);

    auto oldX = timeline.timeToIndex(oldTime);
    auto newX = timeline.timeToIndex(newTime);

    p.setBrush(palette().alternateBase());
    p.drawRect(QRect(oldX, 0, newX - oldX + 1, height()));
  }

  p.setBrush(palette().text());

  for (size_t i = 0; i < std::min(timeline.columns.size(), (size_t) width()); i++) {
    auto entry = timeline.columns[i];
    auto start = static_cast<size_t>(maxCount - entry.count) * static_cast<size_t>(height()) / static_cast<size_t>(maxCount + 1);
    p.drawRect(i, (int) start, 1, height() - (int)start);
  }

}

void TraceTimeline::mousePressEvent(QMouseEvent *event) {
  std::cout << "press\n";
  event->accept();
  auto x = event->pos().x();

  mousePressEntry = TimelineEntry{};
  auto [t1, t2] = timeline.indexToTime(x);
  mousePressEntry.oldest_timestamp = t1;
  mousePressEntry.newest_timestamp = t2;
  mousePressEntry.valid = true;

  mouseReleaseEntry = TimelineEntry{};
  repaint();
}

void TraceTimeline::mouseReleaseEvent(QMouseEvent *event) {
  std::cout << "release\n";
  event->accept();
  auto x = event->pos().x();

  mouseReleaseEntry = TimelineEntry{};
  auto [t1, t2] = timeline.indexToTime(x);
  mouseReleaseEntry.oldest_timestamp = t1;
  mouseReleaseEntry.newest_timestamp = t2;
  mouseReleaseEntry.valid = true;

  repaint();
}
