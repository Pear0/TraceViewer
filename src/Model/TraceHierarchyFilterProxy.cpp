//
// Created by Will Gulian on 12/20/20.
//

#include "../QtCustomUser.h"
#include "TraceHierarchyFilterProxy.h"

TraceHierarchyFilterProxy::TraceHierarchyFilterProxy(QObject *parent) : QSortFilterProxyModel(parent) {
  setSortRole(UserQt::UserSortRole);
}

TraceHierarchyFilterProxy::~TraceHierarchyFilterProxy() = default;






