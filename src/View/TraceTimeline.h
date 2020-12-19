//
// Created by Will Gulian on 12/19/20.
//

#ifndef TRACEVIEWER2_TRACETIMELINE_H
#define TRACEVIEWER2_TRACETIMELINE_H

#include <memory>

#include <QWidget>
#include "../TraceData.h"

class TraceTimeline : public QWidget {
Q_OBJECT

  std::shared_ptr<TraceData> traceData;
  Timeline timeline{};

  TimelineEntry mousePressEntry{};
  TimelineEntry mouseReleaseEntry{};

public:
  explicit TraceTimeline(std::shared_ptr<TraceData> traceData, QWidget *parent = nullptr);

  ~TraceTimeline() override = default;

protected:
  void paintEvent(QPaintEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

};


#endif //TRACEVIEWER2_TRACETIMELINE_H
