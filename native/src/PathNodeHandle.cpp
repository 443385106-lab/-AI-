#include "PathNodeHandle.hpp"

#include <QBrush>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPen>

PathNodeHandle::PathNodeHandle(QGraphicsPathItem *pathItem, int elementIndex, bool controlPoint)
    : QGraphicsEllipseItem(pathItem), m_pathItem(pathItem), m_elementIndex(elementIndex)
{
    setRect(-4.5, -4.5, 9.0, 9.0);
    setBrush(controlPoint ? QColor(255, 181, 46) : QColor(39, 188, 208));
    setPen(QPen(QColor(24, 63, 75), 1.0));
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIgnoresTransformations);
    setCursor(Qt::SizeAllCursor);
    setZValue(100000.0);
    const auto element = pathItem->path().elementAt(elementIndex);
    m_updating = true;
    setPos(element.x, element.y);
    m_updating = false;
}

QVariant PathNodeHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged && !m_updating && m_pathItem && m_elementIndex >= 0) {
        QPainterPath path = m_pathItem->path();
        if (m_elementIndex < path.elementCount()) {
            const QPointF point = value.toPointF();
            path.setElementPositionAt(m_elementIndex, point.x(), point.y());
            m_pathItem->setPath(path);
        }
    }
    return QGraphicsEllipseItem::itemChange(change, value);
}
