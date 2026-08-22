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
#include <QLinearGradient>
#include <QJsonDocument>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTextDocument>
#include <QTextOption>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;
constexpr int LayerRole = 2;
constexpr int LockedRole = 3;
constexpr int VisibleRole = 4;
constexpr int TextBoxHeightRole = 5;
constexpr int ParagraphRole = 6;

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

QJsonObject brushToJson(const QBrush &brush)
{
    QJsonObject json {{"style", static_cast<int>(brush.style())}, {"color", brush.color().name(QColor::HexArgb)}, {"transform", transformToJson(brush.transform())}};
    const QGradient *gradient = brush.gradient(); if (!gradient) return json;
    json["gradientType"] = static_cast<int>(gradient->type()); json["spread"] = static_cast<int>(gradient->spread()); json["coordinateMode"] = static_cast<int>(gradient->coordinateMode());
    QJsonArray stops; for (const auto &stop : gradient->stops()) stops.append(QJsonObject {{"at", stop.first}, {"color", stop.second.name(QColor::HexArgb)}}); json["stops"] = stops;
    if (gradient->type() == QGradient::LinearGradient) {
        const auto *linear = static_cast<const QLinearGradient *>(gradient); json["start"] = rectToJson(QRectF(linear->start(), QSizeF())); json["end"] = rectToJson(QRectF(linear->finalStop(), QSizeF()));
    } else if (gradient->type() == QGradient::RadialGradient) {
        const auto *radial = static_cast<const QRadialGradient *>(gradient); json["center"] = rectToJson(QRectF(radial->center(), QSizeF())); json["focal"] = rectToJson(QRectF(radial->focalPoint(), QSizeF())); json["radius"] = radial->radius();
    }
    return json;
}

QBrush brushFromJson(const QJsonObject &json, const QColor &fallback = Qt::transparent)
{
    if (json.isEmpty()) return QBrush(fallback);
    const auto gradientType = static_cast<QGradient::Type>(json["gradientType"].toInt(static_cast<int>(QGradient::NoGradient)));
    QGradientStops stops; for (const QJsonValue &value : json["stops"].toArray()) { const QJsonObject stop = value.toObject(); stops.append({stop["at"].toDouble(), QColor(stop["color"].toString())}); }
    QBrush brush;
    if (gradientType == QGradient::LinearGradient) {
        QLinearGradient gradient(rectFromJson(json["start"].toObject()).topLeft(), rectFromJson(json["end"].toObject()).topLeft()); gradient.setStops(stops); gradient.setSpread(static_cast<QGradient::Spread>(json["spread"].toInt())); gradient.setCoordinateMode(static_cast<QGradient::CoordinateMode>(json["coordinateMode"].toInt())); brush = QBrush(gradient);
    } else if (gradientType == QGradient::RadialGradient) {
        QRadialGradient gradient(rectFromJson(json["center"].toObject()).topLeft(), json["radius"].toDouble(), rectFromJson(json["focal"].toObject()).topLeft()); gradient.setStops(stops); gradient.setSpread(static_cast<QGradient::Spread>(json["spread"].toInt())); gradient.setCoordinateMode(static_cast<QGradient::CoordinateMode>(json["coordinateMode"].toInt())); brush = QBrush(gradient);
    } else {
        brush = QBrush(QColor(json["color"].toString(fallback.name(QColor::HexArgb)))); brush.setStyle(static_cast<Qt::BrushStyle>(json["style"].toInt(static_cast<int>(Qt::SolidPattern))));
    }
    brush.setTransform(transformFromJson(json["transform"].toObject())); return brush;
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

QJsonArray pathToJson(const QPainterPath &path)
{
    QJsonArray elements;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        elements.append(QJsonObject {{"x", e.x}, {"y", e.y}, {"type", static_cast<int>(e.type)}});
    }
    return elements;
}

QPainterPath pathFromJson(const QJsonArray &elements)
{
    QPainterPath path;
    for (int i = 0; i < elements.size(); ++i) {
        const QJsonObject object = elements[i].toObject();
        const QPointF point(object["x"].toDouble(), object["y"].toDouble());
        const int type = object["type"].toInt(static_cast<int>(QPainterPath::LineToElement));
        if (type == QPainterPath::MoveToElement) path.moveTo(point);
        else if (type == QPainterPath::LineToElement) path.lineTo(point);
        else if (type == QPainterPath::CurveToElement && i + 2 < elements.size()) {
            const QJsonObject c2 = elements[++i].toObject(); const QJsonObject end = elements[++i].toObject();
            path.cubicTo(point, QPointF(c2["x"].toDouble(), c2["y"].toDouble()), QPointF(end["x"].toDouble(), end["y"].toDouble()));
        }
    }
    return path;
}

QJsonObject baseItemJson(QGraphicsItem *item)
{
    return {{"kind", item->data(KindRole).toString()}, {"name", item->data(NameRole).toString()},
            {"layer", item->data(LayerRole).toString()}, {"locked", item->data(LockedRole).toBool()},
            {"visible", !item->data(VisibleRole).isValid() || item->data(VisibleRole).toBool()},
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
        json["fill"] = shape->brush().color().name(QColor::HexArgb); json["brush"] = brushToJson(shape->brush());
    } else if (kind == "ellipse") {
        const auto *shape = static_cast<QGraphicsEllipseItem *>(item);
        json["rect"] = rectToJson(shape->rect()); json["pen"] = penToJson(shape->pen());
        json["fill"] = shape->brush().color().name(QColor::HexArgb); json["brush"] = brushToJson(shape->brush());
    } else if (kind == "line") {
        const auto *shape = static_cast<QGraphicsLineItem *>(item);
        const QLineF line = shape->line();
        json["line"] = QJsonArray {line.x1(), line.y1(), line.x2(), line.y2()}; json["pen"] = penToJson(shape->pen());
    } else if (kind == "path" || kind == "clip") {
        const auto *shape = static_cast<QGraphicsPathItem *>(item);
        json["path"] = pathToJson(shape->path()); json["pen"] = penToJson(shape->pen());
        json["fill"] = shape->brush().color().name(QColor::HexArgb); json["brush"] = brushToJson(shape->brush());
        json["fillStyle"] = static_cast<int>(shape->brush().style());
        if (kind == "clip") json["children"] = DocumentIO::serializeItems(item->childItems());
    } else if (kind == "text") {
        const auto *text = static_cast<QGraphicsTextItem *>(item);
        json["text"] = text->toPlainText(); json["color"] = text->defaultTextColor().name(QColor::HexArgb);
        json["font"] = QJsonObject {{"family", text->font().family()}, {"size", text->font().pointSizeF()},
                                     {"bold", text->font().bold()}, {"italic", text->font().italic()}};
        json["textWidth"] = text->textWidth(); json["textBoxHeight"] = item->data(TextBoxHeightRole).toDouble(); json["paragraph"] = item->data(ParagraphRole).toBool(); json["alignment"] = static_cast<int>(text->document()->defaultTextOption().alignment());
    } else if (kind == "group") {
        json["children"] = DocumentIO::serializeItems(item->childItems());
    }
    return json;
}

void applyBase(QGraphicsItem *item, const QJsonObject &json, const QPointF &offset)
{
    item->setData(KindRole, json["kind"].toString());
    item->setData(NameRole, json["name"].toString());
    item->setData(LayerRole, json["layer"].toString("图层 1"));
    item->setData(LockedRole, json["locked"].toBool()); item->setData(VisibleRole, json["visible"].toBool(true));
    item->setPos(json["x"].toDouble() + offset.x(), json["y"].toDouble() + offset.y());
    item->setRotation(json["rotation"].toDouble()); item->setZValue(json["z"].toDouble());
    item->setOpacity(json["opacity"].toDouble(1.0)); item->setTransform(transformFromJson(json["transform"].toObject()));
    item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    item->setVisible(json["visible"].toBool(true));
    if (json["locked"].toBool()) item->setFlags(QGraphicsItem::ItemIsFocusable);
}

QGraphicsItem *restoreOne(QGraphicsScene *scene, const QJsonObject &json, const QPointF &offset)
{
    const QString kind = json["kind"].toString();
    QGraphicsItem *item = nullptr;
    if (kind == "rectangle") {
        auto *shape = new QGraphicsRectItem(rectFromJson(json["rect"].toObject()));
        shape->setPen(penFromJson(json["pen"].toObject())); shape->setBrush(json.contains("brush") ? brushFromJson(json["brush"].toObject()) : QBrush(QColor(json["fill"].toString()))); item = shape;
    } else if (kind == "ellipse") {
        auto *shape = new QGraphicsEllipseItem(rectFromJson(json["rect"].toObject()));
        shape->setPen(penFromJson(json["pen"].toObject())); shape->setBrush(json.contains("brush") ? brushFromJson(json["brush"].toObject()) : QBrush(QColor(json["fill"].toString()))); item = shape;
    } else if (kind == "line") {
        const QJsonArray a = json["line"].toArray(); if (a.size() != 4) return nullptr;
        auto *shape = new QGraphicsLineItem(QLineF(a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble()));
        shape->setPen(penFromJson(json["pen"].toObject())); item = shape;
    } else if (kind == "path" || kind == "clip") {
        auto *shape = new QGraphicsPathItem(pathFromJson(json["path"].toArray()));
        shape->setPen(penFromJson(json["pen"].toObject()));
        QBrush brush = json.contains("brush") ? brushFromJson(json["brush"].toObject()) : QBrush(QColor(json["fill"].toString("#00000000")));
        if (!json.contains("brush")) brush.setStyle(static_cast<Qt::BrushStyle>(json["fillStyle"].toInt(static_cast<int>(Qt::NoBrush))));
        shape->setBrush(brush); item = shape;
    } else if (kind == "text") {
        auto *text = new QGraphicsTextItem(json["text"].toString()); const QJsonObject f = json["font"].toObject();
        QFont font(f["family"].toString("Microsoft YaHei")); font.setPointSizeF(f["size"].toDouble(24.0)); font.setBold(f["bold"].toBool()); font.setItalic(f["italic"].toBool());
        text->setFont(font); text->setDefaultTextColor(QColor(json["color"].toString("#ff222222"))); text->setTextWidth(json["textWidth"].toDouble(-1.0)); QTextOption option = text->document()->defaultTextOption(); option.setAlignment(static_cast<Qt::AlignmentFlag>(json["alignment"].toInt(static_cast<int>(Qt::AlignLeft)))); text->document()->setDefaultTextOption(option); text->setData(TextBoxHeightRole, json["textBoxHeight"].toDouble()); text->setData(ParagraphRole, json["paragraph"].toBool()); item = text;
    } else if (kind == "group") {
        auto *group = new QGraphicsItemGroup(); scene->addItem(group); applyBase(group, json, offset);
        const auto children = DocumentIO::restoreItems(scene, json["children"].toArray());
        for (QGraphicsItem *child : children) group->addToGroup(child);
        return group;
    }
    if (!item) return nullptr;
    scene->addItem(item); applyBase(item, json, offset);
    if (kind == "clip") {
        item->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
        const auto children = DocumentIO::restoreItems(scene, json["children"].toArray());
        for (QGraphicsItem *child : children) child->setParentItem(item);
    }
    return item;
}
}

QJsonObject DocumentIO::serializeDocument(QGraphicsScene *scene, const QRectF &pageRect)
{
    QList<QGraphicsItem *> roots;
    for (QGraphicsItem *item : scene->items(Qt::AscendingOrder)) if (!item->parentItem()) roots.append(item);
    return {{"format", "JiangxinVectorDocument"}, {"version", 3}, {"page", rectToJson(pageRect)},
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
    QJsonArray array; for (QGraphicsItem *item : items) if (!item->data(KindRole).toString().isEmpty()) array.append(serializeOne(item)); return array;
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
