#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>

class QGraphicsItem;
class QGraphicsScene;

namespace DocumentIO {
QJsonObject serializeDocument(QGraphicsScene *scene, const QRectF &pageRect);
bool restoreDocument(QGraphicsScene *scene, const QJsonObject &document, QRectF *pageRect, QString *error = nullptr);
QJsonArray serializeItems(const QList<QGraphicsItem *> &items);
QList<QGraphicsItem *> restoreItems(QGraphicsScene *scene, const QJsonArray &items, const QPointF &offset = {});
bool saveFile(const QString &fileName, const QJsonObject &document, QString *error = nullptr);
QJsonObject loadFile(const QString &fileName, QString *error = nullptr);
}
