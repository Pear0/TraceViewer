//
// Created by Will Gulian on 12/5/20.
//

#ifndef TRACEVIEWER2_CUSTOMTRACEDIALOG_H
#define TRACEVIEWER2_CUSTOMTRACEDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTimer>
#include "DebugTable.h"

class CustomTraceDialog : public QDialog {
  Q_OBJECT

  QRegularExpression reNewline;

  QTimer *updateTimer;

  QComboBox *buildIdCombo = nullptr;
  QPlainTextEdit *addressesEdit = nullptr;
  QPlainTextEdit *functionsEdit = nullptr;

  uint64_t selectedBuildId = 0;
  bool buildIdValid = false;
public:
  explicit CustomTraceDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

  void updateSymbols();

private:
  std::shared_ptr<DebugTable> getDebugTable();

private slots:
  void addressesChanged();
  void buildIdChanged(int index);
  void timerTick();

};


#endif //TRACEVIEWER2_CUSTOMTRACEDIALOG_H
