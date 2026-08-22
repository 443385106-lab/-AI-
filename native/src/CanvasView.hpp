#pragma once

#include <QColor>
#include <QFont>
#include <QGraphicsView>
#include <QList>
#include <QPainterPath>

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsTextItem;

class CanvasView final : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Tool { Select, Node, Bezier, Freehand, Rectangle, Ellipse, Line, Text, ParagraphText, Zoom, Pan };

    explicit CanvasView(QWidget *parent = nullptr);

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    QRectF pageRect() const { return m_pageRect; }
    void setPageRect(const QRectF &rect);
    void setBleed(qreal units) { m_bleedUnits = qMax(0.0, units); viewport()->update(); }
    void setGridVisible(bool visible);
    void setSnapEnabled(bool enabled);
    void setFillColor(const QColor &color) { m_fillColor = color; }
    void setStrokeColor(const QColor &color) { m_strokeColor = color; }
    void setStrokeWidth(qreal width) { m_strokeWidth = width; }
    void setActiveLayer(const QString &layer) { m_activeLayer = layer; }
    QString activeLayer() const { return m_activeLayer; }
    void zoomToFit();
    void zoomBy(qreal factor);
    void deleteSelection();
    void selectAllObjects();
    void notifyDocumentChanged();
    void refreshTextFrameHandles();
    void clearTextFrameOverlays();

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
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointF snapped(const QPointF &point) const;
    void prepareItem(QGraphicsItem *item, const QString &kind, const QString &name);
    void finishDrawing(const QString &reason);
    void finishBezier(bool closePath, bool commit = true);
    void rebuildNodeHandles();
    void clearNodeHandles();
    void rebuildTextFrameHandles();
    void clearTextFrameHandles();
    void resizeTextFrame(const QPointF &scenePoint, Qt::KeyboardModifiers modifiers);

    Tool m_tool = Tool::Select;
    QRectF m_pageRect {100.0, 100.0, 800.0, 600.0};
    qreal m_bleedUnits = 30.0;
    bool m_gridVisible = false;
    bool m_snapEnabled = true;
    qreal m_gridStep = 20.0;
    QColor m_fillColor {244, 197, 66};
    QColor m_strokeColor {34, 34, 34};
    qreal m_strokeWidth = 2.0;
    QString m_activeLayer = QStringLiteral("图层 1");
    QPointF m_startPoint;
    QGraphicsItem *m_drawingItem = nullptr;
    QGraphicsPathItem *m_pathItem = nullptr;
    QPainterPath m_path;
    QGraphicsPathItem *m_bezierItem = nullptr;
    QPainterPath m_bezierPath;
    int m_bezierAnchorCount = 0;
    QList<QGraphicsItem *> m_nodeHandles;
    QList<QGraphicsRectItem *> m_textFrameHandles;
    QGraphicsRectItem *m_textFrameOutline = nullptr;
    QGraphicsTextItem *m_resizingText = nullptr;
    int m_resizeHandle = -1;
    QRectF m_originalTextFrame;
    QPointF m_resizeStartPoint;
    QFont m_originalTextFont;
};
