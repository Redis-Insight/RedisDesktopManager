#include "RedisServerItem.h"
#include "RedisConnectionConfig.h"

RedisServerItem::RedisServerItem(ConnectionBridge * c) 
    : connection(c), isDbInfoLoaded(false), locked(false)
{                        
    setOfflineIcon();
    getItemNameFromConnection();
    setEditable(false);
}

void RedisServerItem::getItemNameFromConnection()
{
    setText(connection->getConfig().name);
}

void RedisServerItem::setConnection(ConnectionBridge * c)
{
    connection = c;
}

void RedisServerItem::runDatabaseLoading()
{        
    if (isDbInfoLoaded || locked) 
        return;

    setBusyIcon();
    getItemNameFromConnection();

    connect(connection, SIGNAL(error(QString)), this, SLOT(proccessError(QString)));
    connect(connection, SIGNAL(dbListLoaded(RedisConnectionAbstract::RedisDatabases)),
        this, SLOT(databaseDataLoaded(RedisConnectionAbstract::RedisDatabases)));

    connection->initWorker();
    connection->loadDatabasesList();
}

void RedisServerItem::databaseDataLoaded(RedisConnectionAbstract::RedisDatabases databases)
{
    if (databases.size() == 0) 
    {
        setNormalIcon();
        return;
    }

    connection->disconnect(this);

    QMap<QString, int>::const_iterator db = databases.constBegin();

    while (db != databases.constEnd()) {            
        QStandardItem * newDb = new RedisServerDbItem(db.key(), db.value(), this);
        appendRow(newDb);
        ++db;
    }

    sortChildren(0);

    setNormalIcon();

    isDbInfoLoaded = true;

    emit databasesLoaded();
    emit unlockUI();
}

QStringList RedisServerItem::getInfo()
{
    connection->initWorker();

//     if (!connection->isConnected() && !connection->connect()) {
//         // TODO : replace this code by bool checkConnection() { if no_connection -> set server in offline state }
//         // TODO: set error icon        
//         setOfflineIcon();
//         return QStringList();
//     }
// 
//     QVariant info = connection->execute("INFO");
// 
//     if (info.isNull()) {
//         return QStringList();
//     }
// 
//     return info.toString().split("\r\n");

    return QStringList();
}

void RedisServerItem::proccessError(QString srcError)
{
    connection->disconnect(this);
    setOfflineIcon();
    locked = false;

    QString message = QString("Can not connect to server %1. Error: %2")
        .arg(text())
        .arg(srcError);

    emit error(message);
}

ConnectionBridge * RedisServerItem::getConnection()
{
    return connection;
}

void RedisServerItem::reload()
{
    blockSignals(true);
    unload();
    blockSignals(false);

    runDatabaseLoading();    
}

void RedisServerItem::unload()
{
    setBusyIcon();

    removeRows(0, rowCount());

    isDbInfoLoaded = false;

    getItemNameFromConnection();

    setOfflineIcon();

    emit unlockUI();
}

void RedisServerItem::setBusyIcon()
{
    locked = true;
    setIcon(QIcon(":/images/wait.png"));
}

void RedisServerItem::setNormalIcon()
{
    locked = false;
    setIcon(QIcon(":/images/redisIcon.png"));
}

void RedisServerItem::setOfflineIcon()
{
    locked = false;
    setIcon(QIcon(":/images/redisIcon_offline.png"));
}

int RedisServerItem::type() const
{
    return TYPE;
}

bool RedisServerItem::isLocked()
{
    return locked;
}