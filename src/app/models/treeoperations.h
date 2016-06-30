#pragma once
#include <functional>
#include <QSharedPointer>
#include <QObject>
#include <qredisclient/connection.h>
#include "modules/connections-tree/operations.h"
#include "modules/connections-tree/items/keyitem.h"


class ConsoleTabs;

class TreeOperations : public QObject, public ConnectionsTree::Operations
{
    Q_OBJECT
public:
    TreeOperations(QSharedPointer<RedisClient::Connection> connection,
                   ConsoleTabs& tabs);

    void getDatabases(std::function<void(DatabaseList)>) override;

    void getDatabaseKeys(uint dbIndex, std::function<void(const RawKeysList&, const QString&)>) override;

    void disconnect() override;

    QString getNamespaceSeparator() override;    

    void openKeyTab(ConnectionsTree::KeyItem& key, bool openInNewTab = false) override;

    void openConsoleTab() override;

    void openNewKeyDialog(int dbIndex, std::function<void()> callback,
                          QString keyPrefix = QString()) override;

    void notifyDbWasUnloaded(int dbIndex) override;

    void deleteDbKey(ConnectionsTree::KeyItem& key, std::function<void(const QString&)> callback) override;

signals:
    void openValueTab(QSharedPointer<RedisClient::Connection> connection,
                      ConnectionsTree::KeyItem& key, bool inNewTab);

    void newKeyDialog(QSharedPointer<RedisClient::Connection> connection,
                      std::function<void()> callback,
                      int dbIndex, QString keyPrefix);

    void closeDbKeys(QSharedPointer<RedisClient::Connection> connection, int dbIndex,
                     const QRegExp& filter=QRegExp("*", Qt::CaseSensitive, QRegExp::Wildcard));

private:
     QSharedPointer<RedisClient::Connection> m_connection;
     ConsoleTabs& m_consoleTabs;
};
