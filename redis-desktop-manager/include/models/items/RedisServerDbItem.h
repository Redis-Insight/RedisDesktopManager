#pragma once

#include <QStandardItem>
#include <QtConcurrent>

class RedisServerItem;
class RedisKeyItem;

class RedisServerDbItem : public QObject, public QStandardItem
{
	Q_OBJECT

	friend class RedisKeyItem;
public:
	RedisServerDbItem(QString name, int keysCount, RedisServerItem * parent);
	~RedisServerDbItem();

	void loadKeys();

	void setFilter(QRegExp &);
	void resetFilter();

	int virtual type() const;

	const static int TYPE = 2100;

	int getDbIndex() const;

    bool operator<(const QStandardItem & other) const;

	struct Icons {

		Icons(QIcon k, QIcon n) 
			: keyIcon(k), namespaceIcon(n)
		{
				
		}

		QIcon keyIcon;
		QIcon namespaceIcon;
	};

private:
	RedisServerItem * server;

	bool isKeysLoaded;

	int dbIndex;

	unsigned int keysCount;

	QString name;

	QStringList rawKeys;

	QRegExp filter;

	QFutureWatcher<QList<QStandardItem *>> keysLoadingWatcher;
	QFuture<QList<QStandardItem *>> keysLoadingResult;

	Icons iconStorage;	

	RedisKeyItem * keysPool;

	RedisKeyItem * originalKeyPool;

	int currentKeysPoolPosition;

	void renderKeys(QStringList &);

	void setNormalIcon();

	void setBusyIcon();

private slots:
	void keysLoaded(const QVariant &, QObject *);
	void proccessError(QString srcError);
	void keysLoadingStatusChanged(int progress, QObject *);
	void keysLoadingFinished();
};

