#include "treeoperations.h"
#include <qredisclient/redisclient.h>
#include <qredisclient/utils/compat.h>
#include "app/widgets/consoletabs.h"
#include "app/models/connectionconf.h"
#include "console/consoletab.h"
#include "consoleoperations.h"
#include "value-editor/view.h"

#include <algorithm>

TreeOperations::TreeOperations(QSharedPointer<RedisClient::Connection> connection, ConsoleTabs& tabs)
    : m_connection(connection), m_consoleTabs(tabs)
{
}

void TreeOperations::getDatabases(std::function<void (ConnectionsTree::Operations::DatabaseList)> callback)
{
    if (!m_connection->isConnected()) {

        bool result;

        try {
            result = m_connection->connect();
        } catch (const RedisClient::Connection::Exception& e) {
            throw ConnectionsTree::Operations::Exception("Cannot connect to host: " + QString(e.what()));
        }

        if (!result)
            throw ConnectionsTree::Operations::Exception("Cannot connect to host");
    }

    using namespace RedisClient;

    //  Get keys count
    Response result;
    try {
        result = m_connection->commandSync("info");
    } catch (const RedisClient::Connection::Exception& e) {
        throw ConnectionsTree::Operations::Exception("Connection error: " + QString(e.what()));
    }

    DatabaseList availableDatabeses;
    QSet<int> loadedDatabeses;

    if (result.isErrorMessage()) {
        return callback(availableDatabeses);
    }

    // Parse keyspace info
    QString keyspaceInfo = result.getValue().toString();
    QRegularExpression getDbAndKeysCount("^db(\\d+):keys=(\\d+)");
    getDbAndKeysCount.setPatternOptions(QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator iter = getDbAndKeysCount.globalMatch(keyspaceInfo);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        int dbIndex = match.captured(1).toInt();
        availableDatabeses.push_back({dbIndex, match.captured(2).toInt()});
        loadedDatabeses.insert(dbIndex);
    }

    int dbCount = (loadedDatabeses.isEmpty())? 0 : *std::max_element(loadedDatabeses.begin(),
                                                                     loadedDatabeses.end());
    //detect more db if needed
    if (dbCount == 0) {
        Response scanningResp;
        do {
            try {
                scanningResp = m_connection->commandSync("select", QString::number(dbCount));
            } catch (const RedisClient::Connection::Exception& e) {
                throw ConnectionsTree::Operations::Exception("Connection error: " + QString(e.what()));
            }
        } while (scanningResp.isOkMessage() && ++dbCount);
    }

    // build db list
    for (int dbIndex = 0; dbIndex < dbCount; ++dbIndex)
    {
        if (loadedDatabeses.contains(dbIndex))
            continue;
        availableDatabeses.push_back({dbIndex, 0});
    }

    std::sort(availableDatabeses.begin(), availableDatabeses.end(),
              [](QPair<int, int> l, QPair<int, int> r) {
        return l.first < r.first;
    });

    return callback(availableDatabeses);
}

void TreeOperations::getDatabaseKeys(uint dbIndex, std::function<void (const RawKeysList &, const QString &)> callback)
{
    QString keyPattern = static_cast<ConnectionConfig>(m_connection->getConfig()).keysPattern();

    if (m_connection->getServerVersion() >= 2.8) {
        QList<QByteArray> rawCmd {
            "scan", "0", "MATCH", keyPattern.toUtf8(), "COUNT", "10000"
        };
        QSharedPointer<RedisClient::ScanCommand> keyCmd(new RedisClient::ScanCommand(rawCmd, dbIndex));

        try {
            m_connection->retrieveCollection(keyCmd, [this, callback](QVariant r, QString err)
            {                
                if (!err.isEmpty())
                    callback(RawKeysList(), QString("Cannot load keys: %1").arg(err));

                callback(convertQVariantList(r.toList()), QString());
            });
        } catch (const RedisClient::Connection::Exception& error) {            
            callback(RawKeysList(), QString("Cannot load keys: %1").arg(error.what()));
        }
    } else {
        try {
            m_connection->command({"KEYS", keyPattern.toUtf8()}, this,
                                  [this, callback](RedisClient::Response r, QString)
            {
                callback(convertQVariantList(r.getValue().toList()), QString());
            }, dbIndex);
        } catch (const RedisClient::Connection::Exception& error) {
            callback(RawKeysList(), QString("Cannot load keys: %1").arg(error.what()));
        }
    }
}

void TreeOperations::disconnect()
{
    m_connection->disconnect();
}

QString TreeOperations::getNamespaceSeparator()
{
    return static_cast<ConnectionConfig>(m_connection->getConfig()).namespaceSeparator();
}

void TreeOperations::openKeyTab(ConnectionsTree::KeyItem& key, bool openInNewTab)
{
    emit openValueTab(m_connection, key, openInNewTab);
}

void TreeOperations::openConsoleTab()
{       
    QSharedPointer<ConsoleModel> model(new ConsoleModel(m_connection));
    QSharedPointer<Console::ConsoleTab> tab(new Console::ConsoleTab(model.staticCast<Console::Operations>()));
    m_consoleTabs.addTab(tab.staticCast<BaseTab>());
}

// TODO: add callback paramter to allow defining additional logic in db item
// TODO: fix issue #3328
void TreeOperations::openNewKeyDialog(int dbIndex, std::function<void()> callback,
                                      QString keyPrefix)
{
    emit newKeyDialog(m_connection, callback, dbIndex, keyPrefix);
}

void TreeOperations::notifyDbWasUnloaded(int dbIndex)
{
    emit closeDbKeys(m_connection, dbIndex);
}

void TreeOperations::deleteDbKey(ConnectionsTree::KeyItem& key, std::function<void(const QString&)> callback)
{
    RedisClient::Command::Callback cmdCallback = [this, &key, &callback](const RedisClient::Response&, const QString& error)
    {
        if (!error.isEmpty()) {
          callback(QString("Cannot remove key: %1").arg(error));
          return;
        }

        QRegExp filter(key.getFullPath(), Qt::CaseSensitive, QRegExp::Wildcard);
        emit closeDbKeys(m_connection, key.getDbIndex(), filter);
        key.setRemoved();
    };

    try {
        m_connection->command({"DEL", key.getFullPath()}, this, cmdCallback, key.getDbIndex());
    } catch (const RedisClient::Connection::Exception& e) {
        throw ConnectionsTree::Operations::Exception("Delete key error: " + QString(e.what()));
    }
}
