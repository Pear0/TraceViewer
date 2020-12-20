//
// Created by Will Gulian on 12/5/20.
//

#include <iostream>
#include <string>

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

#include "CustomTraceDialog.h"
#include "TraceViewWindow.h"

CustomTraceDialog::CustomTraceDialog(QWidget *parent, Qt::WindowFlags f)
: QDialog(parent, f), reNewline("\\s+") {

  updateTimer = new QTimer(this);
  updateTimer->setInterval(500);

  auto *layout = new QGridLayout;

  auto *label = new QLabel("Build ID", this);
  layout->addWidget(label, 0, 0, 1, 3);

  buildIdCombo = new QComboBox(this);

  layout->addWidget(buildIdCombo, 1, 0, 1, 3);

  label = new QLabel("Addresses", this);
  layout->addWidget(label, 3, 0, 1, 1);

  addressesEdit = new QPlainTextEdit(this);
  layout->addWidget(addressesEdit, 4, 0, 1, 1);

  label = new QLabel("Symbolicated Functions", this);
  layout->addWidget(label, 3, 2, 1, 1);

  functionsEdit = new QPlainTextEdit(this);
  functionsEdit->setReadOnly(true);
  functionsEdit->setWordWrapMode(QTextOption::NoWrap);
  layout->addWidget(functionsEdit, 4, 2, 1, 1);

  layout->setRowStretch(4, 1);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(2, 1);

  setLayout(layout);

  connect(updateTimer, &QTimer::timeout, this, &CustomTraceDialog::timerTick);
  connect(buildIdCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CustomTraceDialog::buildIdChanged);
  connect(addressesEdit, &QPlainTextEdit::textChanged, this, &CustomTraceDialog::addressesChanged);

  updateTimer->start();
}

static std::pair<uint64_t, bool> parseInt(const QString &str) {
  try {
    size_t amtRead = 0;
    uint64_t id = std::stoll(str.toStdString(), &amtRead, 0);

    if (amtRead == str.length()) {
      return std::make_pair(id, true);
    }
  } catch (std::exception &) {
  }

  return std::make_pair(0, false);
}

void CustomTraceDialog::updateSymbols() {
  bool ok = false;
  auto id = buildIdCombo->currentData().toULongLong(&ok);
  if (!ok) {
    functionsEdit->setPlainText(QString());
    return;
  }

  QString result;

  auto debugTable = getDebugTable();
  auto info = debugTable->loadedFiles.find(id);

  auto lines = addressesEdit->toPlainText().split(reNewline);
  for (auto &line : lines) {
    auto [address, valid] = parseInt(line);
    if (valid) {
      auto [symbol, _] = info->second.symbolicate(address);
      result += symbol;
    }
    result += "\n";
  }

  functionsEdit->setPlainText(result);
}


void CustomTraceDialog::addressesChanged() {
  updateSymbols();
}

std::shared_ptr<DebugTable> CustomTraceDialog::getDebugTable() {
  return dynamic_cast<TraceViewWindow *>(parent())->debugTable;
}

void CustomTraceDialog::timerTick() {
  auto debugTable = getDebugTable();

  std::vector<uint64_t> buildIds;
  buildIds.reserve(debugTable->loadedFiles.size());
  for (auto &[key, _] : debugTable->loadedFiles) {
    buildIds.push_back(key);
  }

  for (int i = 0; i < buildIdCombo->count(); i++) {
    uint64_t knownId = buildIdCombo->itemData(i).toULongLong();
    buildIds.erase(std::remove(buildIds.begin(), buildIds.end(), knownId), buildIds.end());
  }

  // sort by most recent first.
  std::sort(buildIds.begin(), buildIds.end(), [&](auto idA, auto idB) {
    return debugTable->loadedFiles.find(idA)->second.loaded_time < debugTable->loadedFiles.find(idB)->second.loaded_time;
  });

  for (size_t i = 0; i < buildIds.size(); i++) {
    auto &[_, info] = *debugTable->loadedFiles.find(buildIds[i]);

    QString name("0x");
    name += QString::number(info.getBuildId(), 16);
    name += " - ";
    // get file name if file is a path.
    name += info.file.section('/', -1);

    buildIdCombo->insertItem(i, name, QVariant(buildIds[i]));
  }
}

void CustomTraceDialog::buildIdChanged(int index) {
  updateSymbols();
}
