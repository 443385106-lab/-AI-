#include "DocumentIO.hpp"

#include <QFile>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QJsonDocument>
#include <QPainterPath>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;

QJsonObject rectToJson(const QRectF &rect)
{
    return {{"x", rect.x()}, {"y", rect.y()}, {"w", rect.width()}, {"h", rect.height()}};
}

QRectF rectFromJson(const QJsonObject &json)
{
    return {json["x"].toDouble(), json["y"].toDouble(), json["w"].toDouble(), json["h"].toDouble()};
}

QJsonObject transformToJson(const QTransform &t)
{
    return {{"m11", t.m11()}, {"m12", t.m12()}, {"m21", t.m21()}, {"m22", t.m22()}, {"dx", t.dx()}, {"dy", t.dy()}};
}

QTransform transformFromJson(const QJsonObject &j)
{
    return {j["m11"].toDouble(1.0), j["m12"].toDouble(), j["m21"].toDouble(),
            j["m22"].toDouble(1.0), j["dx"].toDouble(), j["dy"].toDouble()};
}

QJsonObject penToJson(const QPen &pen)
{
    return {{"color", pen.color().name(QColor::HexArgb)}, {"width", pen.widthF()},
            {"style", static_cast<int>(pen.style())}};
}

QPen penFromJson(const QJsonObject &j)
{
    return {QColor(j["color"].toString("#ff222222")), j["width"].toDouble(2.0),
            static_cast<Qt::PenStyle>(j["style"].toInt(static_cast<int>(Qt::SolidLine))),
            Qt::RoundCap, Qt::RoundJoin};
}

QJsonObject baseItemJson(QGraphicsItem *item)
{
    return {{"kind", item->data(KindRole).toString()}, {"name", item->data(NameRole).toString()},
            {"x", item->pos().x()}, {"y", item->pos().y()}, {"rotation", item->rotation()},
            {"z", item->zValue()}, {"opacity", item->opacity()}, {"transform", transformToJson(item->transform())}};
}

QJsonObject serializeOne(QGraphicsItem *item)
{
    QJsonObject json = baseItemJson(item);
    const QString kind = json["kind"].toString();
    if (kind == "rectangle") {
        const auto *shape = static_cast<QGraphicsRectItem *>(item);
        json["rect"] = rectToJson(shape->rect()); json["pen"] = penToJson(shape->pen());
        json["fill"] = shape->brush().color().name(QColor::HexArgb);
    } else if (kind == "ellipse") {
        const auto *shape = static_cast<QGraphicsEllipseItem *>(item);
        json["rect"] = rectToJson(shape->rect()); json["pen"] = penToJson(shape->pen());
        json["fill"] = shape->brush().color().name(QColor::HexArgb);
    } else if (kind == "line") {
        const auto *shape = static_cast<QGraphicsLineItem *>(item);
        const QLineF line = shape->line();
        json["line"] = QJsonArray {line.x1(), line.y1(), line.x2(), line.y2()}; json["pen"] = penToJson(shape->pen());
    } else if (kind == "path") {
        const auto *shape = static_cast<QGraphicsPathItem *>(item);
        QJsonArray elements;
        for (int i = 0; i < shape->path().elementCount(); ++i) {
            const auto e = shape->path().elementAt(i);
            elements.append(QJsonObject {{"x", e.x}, {"y", e.y}, {"type", static_cast<int>(e.type)}});
        }
        json["path"] = elements; json["pen"] = penToJson(shape->pen());
    } else if (kind == "text") {
        const auto *text = static_cast<QGraphicsTextItem *>(item);
        json["text"] = text->toPlainText(); json["color"] = text->defaultTextColor().name(QColor::HexArgb);
        json["font"] = QJsonObject {{"family", text->font().family()}, {"size", text->font().pointSizeF()},
                                     {"bold", text->font().bold()}, {"italic", text->font().italic()}};
    } else if (kind == "group") {
        json["children"] = DocumentIO::serializeItems(item->childItems());
    }
    return json;
}

void applyBase(QGraphicsItem *item, const QJsonObject &json, const QPointF &offset)
{
    item->setData(KindRole, json["kind"].toString());
    item->setData(NameRole, json["name"].toString());
    item->setPos(json["x"].toDouble() + offset.x(), json["y"].toDouble() + offset.y());
    item->setRotation(json["rotation"].toDouble()); item->setZValue(json["z"].toDouble());
    item->setOpacity(json["opacity"].toDouble(1.0)); item->setTransform(transformFromJson(json["transform"].toObject()));
    item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
}

QGraphicsItem *restoreOne(QGraphicsScene *scene, const QJsonObject &json, const QPointF &offset)
{
    const QString kind = json["kind"].toString();
    QGraphicsItem *item = nullptr;
    if (kind == "rectangle") {
        auto *shape = new QGraphicsRectItem(rectFromJson(json["rect"].toObject()));
        shape->setPen(penFromJson(json["pen"].toObject())); shape->setBrush(QColor(json["fill"].toString())); item = shape;
    } else if (kind == "ellipse") {
        auto *shape = new QGraphicsEllipseItem(rectFromJson(json["rect"].toObject()));
        shape->setPen(penFromJson(json["pen"].toObject())); shape->setBrush(QColor(json["fill"].toString())); item = shape;
    } else if (kind == "line") {
        const QJsonArray a = json["line"].toArray(); if (a.size() != 4) return nullptr;
        auto *shape = new QGraphicsLineItem(QLineF(a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble()));
        shape->setPen(penFromJson(json["pen"].toObject())); item = shape;
    } else if (kind == "path") {
        QPainterPath path; const QJsonArray a = json["path"].toArray();
        for (int i = 0; i < a.size(); ++i) { const QJsonObject e = a[i].toObject(); if (i == 0) path.moveTo(e["x"].toDouble(), e["y"].toDouble()); else path.lineTo(e["x"].toDouble(), e["y"].toDouble()); }
        auto *shape = new QGraphicsPathItem(path); shape->setPen(penFromJson(json["pen"].toObject())); item = shape;
    } else if (kind == "text") {
        auto *text = new QGraphicsTextItem(json["text"].toString()); const QJsonObject f = json["font"].toObject();
        QFont font(f["family"].toString("Microsoft YaHei")); font.setPointSizeF(f["size"].toDouble(24.0)); font.setBold(f["bold"].toBool()); font.setItalic(f["italic"].toBool());
        text->setFont(font); text->setDefaultTextColor(QColor(json["color"].toString("#ff222222"))); item = text;
    } else if (kind == "group") {
        auto *group = new QGraphicsItemGroup(); scene->addItem(group); applyBase(group, json, offset);
        const auto children = DocumentIO::restoreItems(scene, json["children"].toArray());
        for (QGraphicsItem *child : children) group->addToGroup(child);
        return group;
    }
    if (!item) return nullptr;
    scene->addItem(item); applyBase(item, json, offset); return item;
}
}

QJsonObject DocumentIO::serializeDocument(QGraphicsScene *scene, const QRectF &pageRect)
{
    QList<QGraphicsItem *> roots;
    for (QGraphicsItem *item : scene->items(Qt::AscendingOrder)) if (!item->parentItem()) roots.append(item);
    return {{"format", "JiangxinVectorDocument"}, {"version", 1}, {"page", rectToJson(pageRect)},
            {"items", serializeItems(roots)}};
}

bool DocumentIO::restoreDocument(QGraphicsScene *scene, const QJsonObject &document, QRectF *pageRect, QString *error)
{
    if (document["format"].toString() != "JiangxinVectorDocument") {
        if (error) *error = QStringLiteral("不是有效的匠心矢量文档");
        return false;
    }
    scene->clear(); if (pageRect) *pageRect = rectFromJson(document["page"].toObject());
    restoreItems(scene, document["items"].toArray()); return true;
}

QJsonArray DocumentIO::serializeItems(const QList<QGraphicsItem *> &items)
{
    QJsonArray array; for (QGraphicsItem *item : items) array.append(serializeOne(item)); return array;
}

QList<QGraphicsItem *> DocumentIO::restoreItems(QGraphicsScene *scene, const QJsonArray &items, const QPointF &offset)
{
    QList<QGraphicsItem *> restored;
    for (const QJsonValue &value : items) if (QGraphicsItem *item = restoreOne(scene, value.toObject(), offset)) restored.append(item);
    return restored;
}

bool DocumentIO::saveFile(const QString &fileName, const QJsonObject &document, QString *error)
{
    QFile file(fileName); if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    if (file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0) { if (error) *error = file.errorString(); return false; }
    return true;
}

QJsonObject DocumentIO::loadFile(const QString &fileName, QString *error)
{
    QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return {}; }
    QJsonParseError parseError; const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) { if (error) *error = parseError.errorString(); return {}; }
    return document.object();
}
