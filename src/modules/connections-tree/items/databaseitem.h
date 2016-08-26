#pragma once
#include "treeitem.h"
#include "connections-tree/operations.h"
#include <QtConcurrent>
#include <qredisclient/connection.h>

namespace ConnectionsTree {

class NamespaceItem;

typedef QList<QSharedPointer<TreeItem>> DatabaseKeys;

class DatabaseItem : public QObject, public TreeItem
{
    Q_OBJECT
public:
    DatabaseItem(unsigned int index, int keysCount,
                 QSharedPointer<Operations> operations,
                 QWeakPointer<TreeItem> parent);

    ~DatabaseItem();

    QString getDisplayName() const override;

    QString getIconUrl() const override;

    QString getType() const override { return "database"; }    

    QList<QSharedPointer<TreeItem>> getAllChilds() const override;

    uint childCount(bool recursive = false) const override;

    QSharedPointer<TreeItem> child(uint row) const override;

    QWeakPointer<TreeItem> parent() const override;    

    bool isLocked() const override;

    bool isEnabled() const override;    

    void loadKeys();

    void unload();

    int getIndex() const;

    QVariant metadata(const QString&) override;

    void setMetadata(const QString&, QVariant) override;

signals:
    void keysLoaded(unsigned int dbIndex);
    void unloadStarted(unsigned int dbIndex);
    void updateIcon(unsigned int dbIndex);
    void updateLayout(unsigned int dbIndex);
    void error(const QString&);

protected slots:
    void onKeysRendered();

protected:
    void reload();
    void liveUpdate();
    void filterKeys(const QRegExp& filter);
    void resetFilter();
    void renderRawKeys(const RedisClient::Connection::RawKeysList& rawKeys);

private:
    class KeysTreeRenderer
    {
    public:
        static QSharedPointer<DatabaseKeys> renderKeys(QSharedPointer<Operations> operations,
                                                       RedisClient::Connection::RawKeysList keys,
                                                       QRegExp filter,
                                                       QString namespaceSeparator,
                                                       QSharedPointer<DatabaseItem>);
    private:                  
         static void renderNamaspacedKey(QSharedPointer<NamespaceItem> currItem,
                                          const QByteArray &notProcessedKeyPart,
                                          const QByteArray &fullKey,
                                          QSharedPointer<Operations> operations,
                                          const QString& namespaceSeparator,
                                          QSharedPointer<DatabaseKeys> m_result,
                                          QSharedPointer<DatabaseItem>,
                                          QSharedPointer<QHash<QString, QSharedPointer<NamespaceItem>>> m_rootNamespaces
                                          );
    };
private:
    unsigned short int m_index;
    unsigned int m_keysCount;
    bool m_locked;
    QSharedPointer<Operations> m_operations;
    QSharedPointer<DatabaseKeys> m_keys;
    QFutureWatcher<QSharedPointer<DatabaseKeys>> m_keysLoadingWatcher;
    QWeakPointer<TreeItem> m_parent;
    RedisClient::Connection::RawKeysList m_rawKeys;
    QRegExp m_filter;
    QTimer m_liveUpdateTimer;
};

}
