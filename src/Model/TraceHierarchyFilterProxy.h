//
// Created by Will Gulian on 12/20/20.
//

#ifndef TRACEVIEWER2_TRACEHIERARCHYFILTERPROXY_H
#define TRACEVIEWER2_TRACEHIERARCHYFILTERPROXY_H

#include <QSortFilterProxyModel>

class TraceHierarchyFilterProxy : public QSortFilterProxyModel {
  Q_OBJECT

public:
  explicit TraceHierarchyFilterProxy(QObject *parent = nullptr);
  ~TraceHierarchyFilterProxy() override;

};


#endif //TRACEVIEWER2_TRACEHIERARCHYFILTERPROXY_H
