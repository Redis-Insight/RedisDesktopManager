#include "keysrendering.h"

#include <QtGlobal>

#include "items/abstractnamespaceitem.h"
#include "items/keyitem.h"
#include "items/namespaceitem.h"
#include "model.h"

using namespace ConnectionsTree;

QSharedPointer<AbstractNamespaceItem> ConnectionsTree::resolveRootItem(QSharedPointer<AbstractNamespaceItem> item) {
  if (!item) return QSharedPointer<AbstractNamespaceItem>();

  if (item->type() == "database") {
      return item;
  }

  auto parent = item->parent().toStrongRef();

  if (!parent) return QSharedPointer<AbstractNamespaceItem>();

  if (parent->type() == "database")
    return parent.dynamicCast<AbstractNamespaceItem>();

  return resolveRootItem(parent.dynamicCast<AbstractNamespaceItem>());
}

void KeysTreeRenderer::renderKeys(QSharedPointer<Operations> operations,
                                  RedisClient::Connection::RawKeysList keys,
                                  QSharedPointer<AbstractNamespaceItem> parent,
                                  RenderingSettigns settings,
                                  const QSet<QByteArray> &expandedNamespaces) {
  // init
  QElapsedTimer timer;
  timer.start();

  int unprocessedPartStart = 0;
  if (parent->getFullPath().size() > 0 || parent->type() == "namespace") {
      int nsLength = 0;

      if (keys.size() > 0) {
        QString firstKey = QString::fromUtf8(keys[0]);
        int res = firstKey.indexOf(settings.nsSeparator, parent->getFullPath().size());

        qDebug() << "NSs regex pos:" << res;

        nsLength = settings.nsSeparator.matchedLength();
      }

      unprocessedPartStart =
          parent->getFullPath().size() + nsLength;

  }

  auto rootItem = resolveRootItem(parent);

  QHash<QByteArray, QWeakPointer<KeyItem>> preRenderedKeys;

  if (rootItem) {
      qDebug() << "Root item resolved";
      preRenderedKeys = rootItem->getKeysIndex();
  }
  qDebug() << "Pre-rendered keys: " << preRenderedKeys.size();

  auto preRenderedKeysList = preRenderedKeys.keys();
  QSet<QByteArray> preRenderedKeysSet = QSet<QByteArray>(preRenderedKeysList.begin(),
                                                         preRenderedKeysList.end());
  QSet<QByteArray> preRenderedKeysToBeRemoved;

  if (settings.checkPreRenderedItems) {
    preRenderedKeysToBeRemoved = QSet<QByteArray>(preRenderedKeysSet.begin(),
                                                  preRenderedKeysSet.end());
  }

  qDebug() << "Live update: " << settings.checkPreRenderedItems;

  QByteArray rawKey;
  QByteArray nextKey;

  QList<QByteArray> bulkInsertItems;

  auto isBulkInsert = [settings, preRenderedKeysSet, unprocessedPartStart](
                          const QByteArray &current, const QByteArray &next) {
    QString currentKey = QString::fromUtf8(current);
    QString nextKey = QString::fromUtf8(current);
    return (settings.appendNewItems &&
            currentKey.indexOf(settings.nsSeparator, unprocessedPartStart) == -1 &&
            !next.isEmpty() &&
            nextKey.indexOf(settings.nsSeparator, unprocessedPartStart) == -1 &&
            !preRenderedKeysSet.contains(next));
  };

  while (!keys.isEmpty()) {
    rawKey = keys.takeFirst();

    if (preRenderedKeysSet.contains(rawKey)) {
        if (preRenderedKeysToBeRemoved.contains(rawKey)) {
            preRenderedKeysToBeRemoved.remove(rawKey);
        }
        continue;
    }

    if (keys.size() > 0) {
        nextKey = keys[0];
    } else {
        nextKey = QByteArray();
    }

    if (isBulkInsert(rawKey, nextKey)) {
        bulkInsertItems.append(rawKey);
        continue;
    } else if (bulkInsertItems.size() > 0 && parent) {
      int itemsAboutToBeInserted =
          qMin(static_cast<uint>(bulkInsertItems.size()),
               settings.renderLimit - parent->getAllChilds().size());

      qDebug() << "Bulk insert" << itemsAboutToBeInserted;

      if (itemsAboutToBeInserted > 0)
          parent->model().beforeChildLoaded(parent.toWeakRef(),
                                            itemsAboutToBeInserted);

      for (const auto &item : bulkInsertItems) {
        if (parent->getAllChilds().size() >= settings.renderLimit) {
          parent->appendRawKey(item);
        } else {
          QSharedPointer<KeyItem> newKey(new KeyItem(
              item, parent, parent->model(), settings.shortKeysRendering));

          parent->append(newKey, false);

          if (rootItem && rootItem->type() == "database") {
            rootItem->appendKeyToIndex(newKey);
          }
        }
      }
      if (itemsAboutToBeInserted > 0)
        parent->model().childLoaded(parent.toWeakRef());

      bulkInsertItems.clear();
    }

    try {
      renderLazily(rootItem, parent, rawKey.mid(unprocessedPartStart), rawKey, operations,
                   settings, expandedNamespaces, 0, nextKey);
    } catch (std::bad_alloc &) {
      parent->showLoadingError("Not enough memory to render all keys");
      break;
    }
  }

  if (preRenderedKeysToBeRemoved.size() > 0) {
    QList<QWeakPointer<KeyItem>> obsoleteKeys;

    for (const auto &keyFullPath : qAsConst(preRenderedKeysToBeRemoved)) {
      obsoleteKeys.append(preRenderedKeys[keyFullPath]);
    }

    parent->removeObsoleteKeys(obsoleteKeys);
  }

  qDebug() << "Tree builded in: " << timer.elapsed() << " ms";
}

void KeysTreeRenderer::renderLazily(QSharedPointer<AbstractNamespaceItem> root,
    QSharedPointer<AbstractNamespaceItem> parent,
    const QByteArray &notProcessedKeyPart, const QByteArray &fullKey,
    QSharedPointer<Operations> m_operations, const RenderingSettigns &settings,
    const QSet<QByteArray> &expandedNamespaces, unsigned long level,
    const QByteArray &nextKey) {
  Q_ASSERT(parent);

  if (level > 0 && parent->isExpanded() == false) {
    parent->appendRawKey(fullKey);
    return;
  }

  QWeakPointer<TreeItem> currentParent =
      parent.staticCast<TreeItem>().toWeakRef();

  int indexOfNaspaceSeparator = -1;
  auto nsSeparator = settings.nsSeparator;
  int nsSeparatorLength = nsSeparator.pattern().size();

  if (!nsSeparator.isEmpty() && nsSeparator.patternSyntax() == QRegExp::RegExp) {
    QString keyPart = QString::fromUtf8(notProcessedKeyPart);
    indexOfNaspaceSeparator = keyPart.indexOf(nsSeparator);

    qDebug() << "NSs regex pos:" << indexOfNaspaceSeparator << nsSeparator.cap();
    nsSeparatorLength = nsSeparator.matchedLength();
  }

  if (indexOfNaspaceSeparator == -1) {
    if (parent->getAllChilds().size() >= settings.renderLimit) {
      parent->appendRawKey(fullKey);
    } else {
      QSharedPointer<KeyItem> newKey(new KeyItem(fullKey, currentParent,
                                                 parent->model(),
                                                 settings.shortKeysRendering));

      if (settings.appendNewItems) {
        parent->append(newKey);
      } else {
        parent->insertChild(newKey);
      }

      if (root && root->type() == "database") {
        root->appendKeyToIndex(newKey);
      }
    }
    return;
  }

  QByteArray firstNamespaceName =
      notProcessedKeyPart.mid(0, indexOfNaspaceSeparator);

  QSharedPointer<AbstractNamespaceItem> namespaceItem =
      parent->findChildNamespace(firstNamespaceName);

  if (namespaceItem.isNull()) {
    long nsPos =
        fullKey.size() - notProcessedKeyPart.size() + firstNamespaceName.size();
    QByteArray namespaceFullPath = fullKey.mid(0, nsPos);

    // Single namespaced key
if (nsSeparator.patternSyntax() != QRegExp::RegExp
            && (nextKey.isEmpty() || nextKey.indexOf(namespaceFullPath) == -1)) {
      QSharedPointer<KeyItem> newKey(new KeyItem(fullKey, currentParent,
                                                 parent->model(),
                                                 settings.shortKeysRendering));
      parent->append(newKey);
      return;
    }

    namespaceItem = QSharedPointer<NamespaceItem>(
        new NamespaceItem(namespaceFullPath, m_operations, currentParent,
                          parent->model(), settings.dbIndex, settings.filter, nsSeparator.cap()));

    if (expandedNamespaces.contains(namespaceFullPath)) {
      namespaceItem->setExpanded(true);
    }

    parent->appendNamespace(namespaceItem);
  }

  renderLazily(root, namespaceItem,
               notProcessedKeyPart.mid(indexOfNaspaceSeparator + nsSeparatorLength),
               fullKey, m_operations, settings, expandedNamespaces,
               level + 1, nextKey);
}
