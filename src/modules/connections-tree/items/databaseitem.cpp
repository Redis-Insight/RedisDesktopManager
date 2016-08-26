#include "databaseitem.h"
#include "namespaceitem.h"
#include "keyitem.h"
#include <typeinfo>
#include <functional>
#include <algorithm>
#include <QDebug>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include "connections-tree/utils.h"

using namespace ConnectionsTree;

DatabaseItem::DatabaseItem(unsigned int index, int keysCount,
                           QSharedPointer<Operations> operations,
                           QWeakPointer<TreeItem> parent)
    : m_index(index),
      m_keysCount(keysCount),
      m_locked(false),
      m_operations(operations),
      m_keys(new DatabaseKeys()),
      m_parent(parent)
{    
    QObject::connect(&m_keysLoadingWatcher, SIGNAL(finished()), this, SLOT(onKeysRendered()));

    m_eventHandlers.insert("click", [this]() {
        if (m_rawKeys.size() != 0)
            return;

        loadKeys();
    });

    m_eventHandlers.insert("add_key", [this]() {
        m_operations->openNewKeyDialog(m_index, [this]()
        {
            confirmAction(nullptr,
                          tr("Key was added. Do you want to reload keys in the selected database?"),
                          [this]() { reload(); m_keysCount++; }, tr("Key was added"));
        });
    });

    m_eventHandlers.insert("reload", [this]() {
        if (m_locked) {
            QMessageBox::warning(nullptr, tr("Another operation is currently in progress"),
                                 tr("Please wait until another operation will be finised."));
            return;
        }

        reload();
    });

    QSettings settings;
    m_liveUpdateTimer.setInterval(settings.value("app/liveUpdateInterval", 10).toInt() * 1000);
    m_liveUpdateTimer.setSingleShot(true);
    connect(&m_liveUpdateTimer, &QTimer::timeout, this, [this]() {
        liveUpdate();
    });
}

DatabaseItem::~DatabaseItem()
{
    if (m_operations) m_operations->notifyDbWasUnloaded(m_index);
}

QString DatabaseItem::getDisplayName() const
{
    if (m_keys->isEmpty()) {
        return QString("db%1 (%2)").arg(m_index).arg(m_keysCount);
    } else {
        QString filter =  m_filter.isEmpty()? "" : QString("[filter: %1]").arg(m_filter.pattern());

        return QString("db%1 %2 (%3/%4)").arg(m_index).arg(filter).arg(m_rawKeys.size()).arg(m_keysCount);
    }
}

QString DatabaseItem::getIconUrl() const
{
    if (m_locked) return QString("qrc:/images/wait.svg");
    return QString("qrc:/images/db.svg");
}

QList<QSharedPointer<TreeItem> > DatabaseItem::getAllChilds() const
{
    return *m_keys;
}

uint DatabaseItem::childCount(bool) const
{
    return m_keys->size();
}

QSharedPointer<TreeItem> DatabaseItem::child(uint row) const
{
    if (row < childCount())
        return m_keys->at(row);

    return QSharedPointer<TreeItem>();
}

QWeakPointer<TreeItem> DatabaseItem::parent() const
{
    return m_parent;
}

bool DatabaseItem::isLocked() const {return m_locked;}

bool DatabaseItem::isEnabled() const {return true;}

void DatabaseItem::loadKeys()
{
    m_locked = true;
    emit updateIcon(m_index);

    if (m_rawKeys.size() > 0) {
        renderRawKeys(m_rawKeys);
        return;
    }

    QString filter = (m_filter.isEmpty())? "" : m_filter.pattern();

    m_operations->getDatabaseKeys(m_index, filter, [this](const RedisClient::Connection::RawKeysList& rawKeys, const QString& err) {
        if (!err.isEmpty()) {
            m_locked = false;
            emit error(err);
            emit updateIcon(m_index);

            QMessageBox::warning(nullptr, tr("Keys error"), err);

            return;
        }
        m_rawKeys = rawKeys;
        renderRawKeys(rawKeys);
    });
}

int DatabaseItem::getIndex() const
{
    return m_index;
}

QVariant DatabaseItem::metadata(const QString &key)
{
    if (key == "filter")
        return m_filter.pattern();
    if (key == "live_update")
        return m_liveUpdateTimer.isActive();

    return QVariant();
}

void DatabaseItem::setMetadata(const QString &key, QVariant value)
{
    bool isResetValue = (value.isNull() || !value.canConvert<QString>() || value.toString().isEmpty());

    if (key == "filter") {
        if (!m_filter.isEmpty() && isResetValue)
            return resetFilter();
        else if (isResetValue)
            return;

        QRegExp pattern(value.toString(), Qt::CaseSensitive, QRegExp::PatternSyntax::WildcardUnix);
        return filterKeys(pattern);
    } else if (key == "live_update") {
        if (m_liveUpdateTimer.isActive() && isResetValue) {
            m_liveUpdateTimer.stop();
            return;
        }

        m_liveUpdateTimer.start();
        return;
    }
}

void DatabaseItem::onKeysRendered()
{
    m_keys = m_keysLoadingWatcher.result();
    m_locked = false;
    emit keysLoaded(m_index);
}

void DatabaseItem::unload()
{
    if (m_keys->size() == 0)
        return;

    m_locked = true;
    emit unloadStarted(m_index);
    m_operations->notifyDbWasUnloaded(m_index);
    m_keys = QSharedPointer<DatabaseKeys>(new DatabaseKeys());
    m_rawKeys.clear();
    m_locked = false;
}

void DatabaseItem::reload()
{
    unload();
    loadKeys();
}

void DatabaseItem::liveUpdate()
{
    if (m_locked) {
        qDebug() << "Another loading operation is in progress. Skip this live update...";
        m_liveUpdateTimer.start();
        return;
    }

    m_locked = true;
    emit updateIcon(m_index);

    QString filter = (m_filter.isEmpty())? "" : m_filter.pattern();

    m_operations->getDatabaseKeys(m_index, filter, [this](const RedisClient::Connection::RawKeysList& rawKeys, const QString& err) {
        if (!err.isEmpty()) {
            m_locked = false;
            emit error(err);
            emit updateIcon(m_index);

            QMessageBox::warning(nullptr, tr("Keys error"), err);

            return;
        }
        m_rawKeys = rawKeys;

        QSettings settings;
        if (m_rawKeys.size() >= settings.value("app/liveUpdateKeysLimit", 1000).toInt()) {
            m_liveUpdateTimer.stop();
            QMessageBox::warning(nullptr, tr("Live update was disabled"),
                                 tr("Live update was disabled due to exceeded keys limit. "
                                    "Please specify more accurate filter or change limit in settings."));
        } else {
            emit unloadStarted(m_index);
            m_keys = QSharedPointer<DatabaseKeys>(new DatabaseKeys());
            renderRawKeys(rawKeys);
            m_liveUpdateTimer.start();
        }
    });
}

void DatabaseItem::filterKeys(const QRegExp &filter)
{
    m_filter = filter;
    emit unloadStarted(m_index);
    loadKeys();
}

void DatabaseItem::resetFilter()
{
    m_filter = QRegExp();
    emit unloadStarted(m_index);
    loadKeys();
}

void DatabaseItem::renderRawKeys(const RedisClient::Connection::RawKeysList &rawKeys)
{
    qDebug() << "Render keys: " << rawKeys.size();

    if (rawKeys.size() == 0) {
        m_locked = false;
        return;
    }

    QString separator(m_operations->getNamespaceSeparator());

    QSharedPointer<TreeItem> server = parent().toStrongRef();

    if (!server || !server->child(row())) {
        qDebug() << "Cannot render keys: invalid parent item";
        return;
    }

    QSharedPointer<DatabaseItem> self = server->child(row()).staticCast<DatabaseItem>();

    QFuture<QSharedPointer<QList<QSharedPointer<TreeItem>>>> keysLoadingResult =
            QtConcurrent::run(&KeysTreeRenderer::renderKeys,
                              m_operations, rawKeys, m_filter, separator, self);

    m_keysLoadingWatcher.setFuture(keysLoadingResult);
}

QSharedPointer<DatabaseKeys> DatabaseItem::KeysTreeRenderer::renderKeys(QSharedPointer<Operations> operations,
                                           RedisClient::Connection::RawKeysList keys,
                                           QRegExp filter,
                                           QString namespaceSeparator,
                                           QSharedPointer<DatabaseItem> parent)
{
    //init
    QElapsedTimer timer;
    timer.start();

    // Sort keys before rendering
    QSettings settings;
    if (settings.value("app/enableKeySortingInTree", true).toBool()) {
        std::sort(keys.begin(), keys.end());
        qDebug() << "Keys sorted in: " << timer.elapsed() << " ms";
    } else {
        qDebug() << "Keys sorting disabled in settings";
    }

    QSharedPointer<QList<QSharedPointer<TreeItem>>> result(new QList<QSharedPointer<TreeItem>>());
    QSharedPointer<QHash<QString, QSharedPointer<NamespaceItem>>> rootNamespaces(
                new QHash<QString, QSharedPointer<NamespaceItem>>());

    //render
    timer.restart();
    for (QByteArray rawKey : keys) {

        //if filter enabled - skip keys
        if (!filter.isEmpty()) {
            QString key = QString::fromUtf8(rawKey); // UTF filtering
            if (!key.contains(filter))
                continue;
        }

        renderNamaspacedKey(QSharedPointer<NamespaceItem>(),
                            rawKey, rawKey, operations,
                            namespaceSeparator, result, parent,
                            rootNamespaces);
    }
    qDebug() << "Tree builded in: " << timer.elapsed() << " ms";
    return result;
}

void DatabaseItem::KeysTreeRenderer::renderNamaspacedKey(
        QSharedPointer<NamespaceItem> currItem,
        const QByteArray &notProcessedKeyPart,
        const QByteArray &fullKey,
        QSharedPointer<Operations> m_operations,
        const QString& m_namespaceSeparator,
        QSharedPointer<DatabaseKeys> m_result,
        QSharedPointer<DatabaseItem> db,
        QSharedPointer<QHash<QString, QSharedPointer<NamespaceItem> > > m_rootNamespaces)
{
    QWeakPointer<TreeItem> currentParent = (currItem.isNull())? db.staticCast<TreeItem>().toWeakRef() :
                                                                currItem.staticCast<TreeItem>().toWeakRef();

    int indexOfNaspaceSeparator = (m_namespaceSeparator.isEmpty())?
                -1 : notProcessedKeyPart.indexOf(m_namespaceSeparator);

    if (indexOfNaspaceSeparator == -1) {
        QSharedPointer<KeyItem> newKey(
                    (new KeyItem(fullKey, db->getIndex(), m_operations, currentParent))
                    );

        if (currItem.isNull()) m_result->push_back(newKey);
        else currItem->append(newKey);
        return;
    }

    QString firstNamespaceName = notProcessedKeyPart.mid(0, indexOfNaspaceSeparator);
    QSharedPointer<NamespaceItem> namespaceItem;    

    if (currItem.isNull() && m_rootNamespaces->contains(firstNamespaceName)) {
        namespaceItem = (*m_rootNamespaces)[firstNamespaceName];
    } else if (!currItem.isNull()) {
        namespaceItem = currItem->findChildNamespace(firstNamespaceName);
    }

    if (namespaceItem.isNull()) {
        namespaceItem = QSharedPointer<NamespaceItem>(
                    new NamespaceItem(fullKey.mid(0, fullKey.indexOf(notProcessedKeyPart.mid(indexOfNaspaceSeparator))),
                                      m_operations, currentParent));

        if (currItem.isNull()) {
            m_result->push_back(namespaceItem);
            m_rootNamespaces->insert(namespaceItem->getDisplayPart(), namespaceItem);
        }
        else currItem->append(namespaceItem);
    }

    renderNamaspacedKey(namespaceItem, notProcessedKeyPart.mid(indexOfNaspaceSeparator+m_namespaceSeparator.length()),
                        fullKey, m_operations, m_namespaceSeparator, m_result, db, m_rootNamespaces);
}
