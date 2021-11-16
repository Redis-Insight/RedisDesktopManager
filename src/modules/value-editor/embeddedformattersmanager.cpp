#include "embeddedformattersmanager.h"

#include <qpython.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/models/configmanager.h"

ValueEditor::EmbeddedFormattersManager::EmbeddedFormattersManager()
    : m_python(nullptr) {}

void ValueEditor::EmbeddedFormattersManager::init(QSharedPointer<QPython> p) {
  if (!p) {
    emit error("Failed to load python");
    return;
  }

  m_python = p;

  QObject::connect(m_python.data(), &QPython::error, this,
                   &EmbeddedFormattersManager::error);
}

void ValueEditor::EmbeddedFormattersManager::loadFormatters(QJSValue callback) {
  if (!m_python) {
    qWarning() << "EmbeddedFormattersManager is not ready";
    return;
  }

  m_python->importModule_sync("formatters");
  m_python->call("formatters.get_formatters_list", QVariantList(), callback);
}

void ValueEditor::EmbeddedFormattersManager::decode(
    const QString &formatterName, const QByteArray &data, QJSValue jsCallback) {
  if (!m_python) {
    qWarning() << "EmbeddedFormattersManager is not ready";
    return;
  }
  m_python->call("formatters.decode", QVariantList{formatterName, data},
                 jsCallback);
}

void ValueEditor::EmbeddedFormattersManager::isValid(
    const QString &formatterName, const QByteArray &data, QJSValue jsCallback) {
  if (!m_python) {
    qWarning() << "EmbeddedFormattersManager is not ready";
    return;
  }
  m_python->call("formatters.validate", QVariantList{formatterName, data},
                 jsCallback);
}

void ValueEditor::EmbeddedFormattersManager::encode(
    const QString &formatterName, const QByteArray &data, QJSValue jsCallback) {
  if (!m_python) {
    qWarning() << "EmbeddedFormattersManager is not ready";
    return;
  }
  m_python->call("formatters.encode", QVariantList{formatterName, data},
                 jsCallback);
}
