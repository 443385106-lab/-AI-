#pragma once

#include <QGraphicsEllipseItem>

class QGraphicsPathItem;

class PathNodeHandle final : public QGraphicsEllipseItem
{
public:
    PathNodeHandle(QGraphicsPathItem *pathItem, int elementIndex, bool controlPoint);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QGraphicsPathItem *m_pathItem = nullptr;
    int m_elementIndex = -1;
    bool m_updating = false;
};
