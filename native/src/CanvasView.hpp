#pragma once

#include <QColor>
#include <QGraphicsView>
#include <QPainterPath>

class QGraphicsItem;
class QGraphicsPathItem;

class CanvasView final : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Tool { Select, Node, Freehand, Rectangle, Ellipse, Line, Text, Zoom, Pan };

    explicit CanvasView(QWidget *parent = nullptr);

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    QRectF pageRect() const { return m_pageRect; }
    void setPageRect(const QRectF &rect);
    void setGridVisible(bool visible);
    void setSnapEnabled(bool enabled);
    void setFillColor(const QColor &color) { m_fillColor = color; }
    void setStrokeColor(const QColor &color) { m_strokeColor = color; }
    void setStrokeWidth(qreal width) { m_strokeWidth = width; }
    void zoomToFit();
    void zoomBy(qreal factor);
    void deleteSelection();
    void selectAllObjects();
    void notifyDocumentChanged();

Q_SIGNALS:
    void documentCommitted(const QString &reason);
    void viewChanged();
    void cursorScenePositionChanged(const QPointF &position);
    void toolChanged(CanvasView::Tool tool);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointF snapped(const QPointF &point) const;
    void prepareItem(QGraphicsItem *item, const QString &kind, const QString &name);
    void finishDrawing(const QString &reason);

    Tool m_tool = Tool::Select;
    QRectF m_pageRect {100.0, 100.0, 800.0, 600.0};
    bool m_gridVisible = false;
    bool m_snapEnabled = true;
    qreal m_gridStep = 20.0;
    QColor m_fillColor {244, 197, 66};
    QColor m_strokeColor {34, 34, 34};
    qreal m_strokeWidth = 2.0;
    QPointF m_startPoint;
    QGraphicsItem *m_drawingItem = nullptr;
    QGraphicsPathItem *m_pathItem = nullptr;
    QPainterPath m_path;
};

