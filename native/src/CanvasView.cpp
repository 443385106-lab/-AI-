#include "CanvasView.hpp"
#include "PathNodeHandle.hpp"

#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;
constexpr int LayerRole = 2;
constexpr int VisibleRole = 4;
constexpr int TextBoxHeightRole = 5;
constexpr int ParagraphRole = 6;
}

CanvasView::CanvasView(QWidget *parent) : QGraphicsView(parent)
{
    auto *graphicsScene = new QGraphicsScene(this);
    graphicsScene->setSceneRect(-300.0, -300.0, 1600.0, 1400.0);
    setScene(graphicsScene);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::RubberBandDrag);
    setBackgroundBrush(QColor(214, 218, 222));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &CanvasView::viewChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CanvasView::viewChanged);
    connect(graphicsScene, &QGraphicsScene::selectionChanged, this, [this] {
        if (m_tool == Tool::Node) rebuildNodeHandles();
        if (m_tool == Tool::Select) rebuildTextFrameHandles();
    });
}

void CanvasView::setTool(Tool tool)
{
    if (m_tool == Tool::Bezier && tool != Tool::Bezier && m_bezierItem) finishBezier(false);
    m_tool = tool;
    setDragMode(tool == Tool::Select ? QGraphicsView::RubberBandDrag
                                    : tool == Tool::Pan ? QGraphicsView::ScrollHandDrag
                                                        : QGraphicsView::NoDrag);
    viewport()->setCursor(tool == Tool::Text || tool == Tool::ParagraphText ? Qt::IBeamCursor
                          : tool == Tool::Zoom ? Qt::PointingHandCursor
                          : tool == Tool::Pan ? Qt::OpenHandCursor
                          : tool == Tool::Select || tool == Tool::Node ? Qt::ArrowCursor
                                                                    : Qt::CrossCursor);
    if (tool == Tool::Node) rebuildNodeHandles(); else clearNodeHandles();
    if (tool == Tool::Select) rebuildTextFrameHandles(); else clearTextFrameHandles();
    Q_EMIT toolChanged(tool);
}

void CanvasView::setPageRect(const QRectF &rect)
{
    if (rect.width() < 10.0 || rect.height() < 10.0) return;
    m_pageRect = rect;
    scene()->setSceneRect(rect.adjusted(-400.0, -400.0, 400.0, 400.0));
    viewport()->update();
    Q_EMIT viewChanged();
}

void CanvasView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    viewport()->update();
}

void CanvasView::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
}

void CanvasView::zoomToFit()
{
    const qreal margin = qMax(30.0, m_bleedUnits + 20.0);
    fitInView(m_pageRect.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
    Q_EMIT viewChanged();
}

void CanvasView::zoomBy(qreal factor)
{
    const qreal next = transform().m11() * factor;
    if (next < 0.08 || next > 16.0) return;
    scale(factor, factor);
    Q_EMIT viewChanged();
}

void CanvasView::deleteSelection()
{
    const auto selected = scene()->selectedItems();
    for (QGraphicsItem *item : selected) {
        if (item->parentItem() && item->parentItem()->data(KindRole).toString() == QStringLiteral("group")) continue;
        scene()->removeItem(item);
        delete item;
    }
    if (!selected.isEmpty()) Q_EMIT documentCommitted(QStringLiteral("删除对象"));
}

void CanvasView::selectAllObjects()
{
    for (QGraphicsItem *item : scene()->items()) {
        if (!item->parentItem()) item->setSelected(true);
    }
}

void CanvasView::notifyDocumentChanged()
{
    scene()->update();
    viewport()->update();
    if (m_tool == Tool::Select) rebuildTextFrameHandles();
}

void CanvasView::refreshTextFrameHandles()
{
    if (m_tool == Tool::Select) rebuildTextFrameHandles();
}

void CanvasView::clearTextFrameOverlays()
{
    m_resizingText = nullptr; m_resizeHandle = -1; clearTextFrameHandles();
}

void CanvasView::clearTextFrameHandles()
{
    const auto handles = m_textFrameHandles; m_textFrameHandles.clear();
    for (QGraphicsRectItem *handle : handles) { if (handle->scene()) handle->scene()->removeItem(handle); delete handle; }
    if (m_textFrameOutline) { if (m_textFrameOutline->scene()) m_textFrameOutline->scene()->removeItem(m_textFrameOutline); delete m_textFrameOutline; m_textFrameOutline = nullptr; }
}

void CanvasView::rebuildTextFrameHandles()
{
    clearTextFrameHandles();
    if (m_tool != Tool::Select || m_resizingText) return;
    const auto selected = scene()->selectedItems(); if (selected.size() != 1) return;
    auto *text = dynamic_cast<QGraphicsTextItem *>(selected.first());
    if (!text || !text->data(ParagraphRole).toBool() || text->data(3).toBool()) return;
    const qreal width = text->textWidth() > 0.0 ? text->textWidth() : text->boundingRect().width();
    const qreal height = text->data(TextBoxHeightRole).toDouble() > 0.0 ? text->data(TextBoxHeightRole).toDouble() : text->boundingRect().height();
    const QRectF frame = text->sceneTransform().mapRect(QRectF(0.0, 0.0, qMax(40.0, width), qMax(30.0, height)));
    const bool overflow = text->boundingRect().height() > height + 0.5;
    m_textFrameOutline = new QGraphicsRectItem(frame); QPen outline(overflow ? QColor(QStringLiteral("#E53935")) : QColor(QStringLiteral("#1479D1")), 1.2, overflow ? Qt::DashLine : Qt::SolidLine); outline.setCosmetic(true); m_textFrameOutline->setPen(outline); m_textFrameOutline->setBrush(Qt::NoBrush); m_textFrameOutline->setZValue(100000.0); scene()->addItem(m_textFrameOutline);
    const QList<QPointF> positions {frame.topLeft(), QPointF(frame.center().x(), frame.top()), frame.topRight(), QPointF(frame.right(), frame.center().y()), frame.bottomRight(), QPointF(frame.center().x(), frame.bottom()), frame.bottomLeft(), QPointF(frame.left(), frame.center().y())};
    for (int index = 0; index < positions.size(); ++index) {
        auto *handle = new QGraphicsRectItem(QRectF(-4.5, -4.5, 9.0, 9.0)); handle->setPos(positions[index]); handle->setPen(QPen(Qt::white, 1.0)); handle->setBrush(overflow ? QColor(QStringLiteral("#E53935")) : QColor(QStringLiteral("#1479D1"))); handle->setFlag(QGraphicsItem::ItemIgnoresTransformations); handle->setZValue(100001.0); handle->setData(0, index); scene()->addItem(handle); m_textFrameHandles.append(handle);
    }
}

void CanvasView::resizeTextFrame(const QPointF &scenePoint, Qt::KeyboardModifiers modifiers)
{
    if (!m_resizingText || m_resizeHandle < 0) return;
    QRectF frame = m_originalTextFrame; const QPointF delta = scenePoint - m_resizeStartPoint;
    if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7) frame.setLeft(frame.left() + delta.x());
    if (m_resizeHandle == 2 || m_resizeHandle == 3 || m_resizeHandle == 4) frame.setRight(frame.right() + delta.x());
    if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2) frame.setTop(frame.top() + delta.y());
    if (m_resizeHandle == 4 || m_resizeHandle == 5 || m_resizeHandle == 6) frame.setBottom(frame.bottom() + delta.y());
    const qreal ratio = m_originalTextFrame.width() / qMax(1.0, m_originalTextFrame.height());
    if (modifiers.testFlag(Qt::ShiftModifier) && m_resizeHandle % 2 == 0) {
        if (qAbs(frame.width() - m_originalTextFrame.width()) >= qAbs(frame.height() - m_originalTextFrame.height()) * ratio) frame.setHeight(qAbs(frame.width()) / ratio);
        else frame.setWidth(qAbs(frame.height()) * ratio);
        if (m_resizeHandle == 0 || m_resizeHandle == 2) frame.moveBottom(m_originalTextFrame.bottom());
        if (m_resizeHandle == 0 || m_resizeHandle == 6) frame.moveRight(m_originalTextFrame.right());
    }
    if (frame.width() < 40.0) { if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7) frame.setLeft(frame.right() - 40.0); else frame.setRight(frame.left() + 40.0); }
    if (frame.height() < 30.0) { if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2) frame.setTop(frame.bottom() - 30.0); else frame.setBottom(frame.top() + 30.0); }
    m_resizingText->setPos(frame.topLeft()); m_resizingText->setTextWidth(frame.width()); m_resizingText->setData(TextBoxHeightRole, frame.height()); m_resizingText->setData(ParagraphRole, true);
    if (modifiers.testFlag(Qt::ControlModifier)) { QFont font = m_originalTextFont; const qreal factor = qMin(frame.width() / qMax(1.0, m_originalTextFrame.width()), frame.height() / qMax(1.0, m_originalTextFrame.height())); font.setPointSizeF(qBound(6.0, m_originalTextFont.pointSizeF() * factor, 500.0)); m_resizingText->setFont(font); }
    if (m_textFrameOutline) m_textFrameOutline->setRect(frame);
    const QList<QPointF> positions {frame.topLeft(), QPointF(frame.center().x(), frame.top()), frame.topRight(), QPointF(frame.right(), frame.center().y()), frame.bottomRight(), QPointF(frame.center().x(), frame.bottom()), frame.bottomLeft(), QPointF(frame.left(), frame.center().y())};
    for (int index = 0; index < m_textFrameHandles.size(); ++index) m_textFrameHandles[index]->setPos(positions[index]);
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, QColor(214, 218, 222));
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::white);
    painter->drawRect(m_pageRect);

    if (m_gridVisible) {
        painter->save();
        painter->setClipRect(m_pageRect);
        QPen gridPen(QColor(225, 228, 232));
        gridPen.setCosmetic(true);
        painter->setPen(gridPen);
        const qreal left = std::floor(m_pageRect.left() / m_gridStep) * m_gridStep;
        const qreal top = std::floor(m_pageRect.top() / m_gridStep) * m_gridStep;
        for (qreal x = left; x <= m_pageRect.right(); x += m_gridStep)
            painter->drawLine(QPointF(x, m_pageRect.top()), QPointF(x, m_pageRect.bottom()));
        for (qreal y = top; y <= m_pageRect.bottom(); y += m_gridStep)
            painter->drawLine(QPointF(m_pageRect.left(), y), QPointF(m_pageRect.right(), y));
        painter->restore();
    }

    QPen edgePen(QColor(125, 132, 140));
    edgePen.setCosmetic(true);
    painter->setPen(edgePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_pageRect);

    if (m_bleedUnits > 0.0) {
        QPen bleedPen(QColor(220, 55, 55)); bleedPen.setStyle(Qt::DashLine); bleedPen.setCosmetic(true);
        painter->setPen(bleedPen);
        painter->drawRect(m_pageRect.adjusted(-m_bleedUnits, -m_bleedUnits, m_bleedUnits, m_bleedUnits));
    }
}

QPointF CanvasView::snapped(const QPointF &point) const
{
    if (!m_snapEnabled) return point;
    return {std::round(point.x() / 5.0) * 5.0, std::round(point.y() / 5.0) * 5.0};
}

void CanvasView::prepareItem(QGraphicsItem *item, const QString &kind, const QString &name)
{
    item->setData(KindRole, kind);
    item->setData(NameRole, name);
    item->setData(LayerRole, m_activeLayer);
    item->setData(VisibleRole, true);
    item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    scene()->addItem(item);
    scene()->clearSelection();
    item->setSelected(true);
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    const QPointF rawPoint = mapToScene(event->position().toPoint());
    if (m_tool == Tool::Select) {
        const QList<QGraphicsItem *> underMouse = items(event->position().toPoint());
        for (QGraphicsItem *candidate : underMouse) {
            int index = -1; for (int handleIndex = 0; handleIndex < m_textFrameHandles.size(); ++handleIndex) if (candidate == m_textFrameHandles[handleIndex]) { index = handleIndex; break; }
            if (index >= 0) {
                const auto selected = scene()->selectedItems(); if (selected.size() == 1) m_resizingText = dynamic_cast<QGraphicsTextItem *>(selected.first());
                if (m_resizingText) { m_resizeHandle = index; m_resizeStartPoint = rawPoint; const qreal width = m_resizingText->textWidth() > 0.0 ? m_resizingText->textWidth() : m_resizingText->boundingRect().width(); const qreal height = m_resizingText->data(TextBoxHeightRole).toDouble() > 0.0 ? m_resizingText->data(TextBoxHeightRole).toDouble() : m_resizingText->boundingRect().height(); m_originalTextFrame = QRectF(m_resizingText->pos(), QSizeF(width, height)); m_originalTextFont = m_resizingText->font(); event->accept(); return; }
            }
        }
    }
    const QPointF point = snapped(rawPoint);
    m_startPoint = point;
    if (m_tool == Tool::Select || m_tool == Tool::Node || m_tool == Tool::Pan) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    if (m_tool == Tool::Zoom) {
        zoomBy(event->modifiers().testFlag(Qt::ShiftModifier) ? 0.8 : 1.25);
        return;
    }
    if (m_tool == Tool::Text || m_tool == Tool::ParagraphText) {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(this, QStringLiteral("添加文字"),
                                                            QStringLiteral("文字内容"), QStringLiteral("双击编辑文字"), &ok);
        if (ok && !text.isEmpty()) {
            auto *item = new QGraphicsTextItem(text);
            item->setDefaultTextColor(m_fillColor);
            const bool paragraph = m_tool == Tool::ParagraphText;
            QFont font(QStringLiteral("Microsoft YaHei"), paragraph ? 18 : 24);
            item->setFont(font);
            if (paragraph) {
                item->setTextWidth(360.0);
                item->setData(TextBoxHeightRole, 260.0);
                item->setData(ParagraphRole, true);
            }
            item->setPos(point);
            prepareItem(item, QStringLiteral("text"), paragraph ? QStringLiteral("段落文本") : text.left(20));
            if (paragraph) {
                item->setData(TextBoxHeightRole, 260.0);
                item->setData(ParagraphRole, true);
            }
            Q_EMIT documentCommitted(paragraph ? QStringLiteral("添加段落文本") : QStringLiteral("添加美术字"));
        }
        return;
    }

    if (m_tool == Tool::Bezier) {
        const QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (!m_bezierItem) {
            m_bezierPath = QPainterPath(point);
            m_bezierItem = new QGraphicsPathItem(m_bezierPath);
            m_bezierItem->setPen(pen);
            m_bezierItem->setBrush(Qt::NoBrush);
            prepareItem(m_bezierItem, QStringLiteral("path"), QStringLiteral("贝塞尔曲线"));
            m_bezierAnchorCount = 1;
        } else {
            const QPointF previous = m_bezierPath.currentPosition();
            const QPointF delta = point - previous;
            m_bezierPath.cubicTo(previous + delta / 3.0, point - delta / 3.0, point);
            m_bezierItem->setPath(m_bezierPath);
            ++m_bezierAnchorCount;
        }
        return;
    }

    const QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (m_tool == Tool::Rectangle) {
        auto *item = new QGraphicsRectItem(QRectF(point, QSizeF(1.0, 1.0)));
        item->setBrush(m_fillColor); item->setPen(pen);
        prepareItem(item, QStringLiteral("rectangle"), QStringLiteral("矩形")); m_drawingItem = item;
    } else if (m_tool == Tool::Ellipse) {
        auto *item = new QGraphicsEllipseItem(QRectF(point, QSizeF(1.0, 1.0)));
        item->setBrush(m_fillColor); item->setPen(pen);
        prepareItem(item, QStringLiteral("ellipse"), QStringLiteral("椭圆")); m_drawingItem = item;
    } else if (m_tool == Tool::Line) {
        auto *item = new QGraphicsLineItem(QLineF(point, point));
        item->setPen(pen);
        prepareItem(item, QStringLiteral("line"), QStringLiteral("直线")); m_drawingItem = item;
    } else if (m_tool == Tool::Freehand) {
        m_path = QPainterPath(point);
        m_pathItem = new QGraphicsPathItem(m_path);
        m_pathItem->setPen(pen); m_pathItem->setBrush(Qt::NoBrush);
        prepareItem(m_pathItem, QStringLiteral("path"), QStringLiteral("手绘路径")); m_drawingItem = m_pathItem;
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF rawPoint = mapToScene(event->position().toPoint());
    if (m_resizingText) { resizeTextFrame(rawPoint, event->modifiers()); Q_EMIT cursorScenePositionChanged(rawPoint); event->accept(); return; }
    const QPointF point = snapped(rawPoint);
    Q_EMIT cursorScenePositionChanged(point);
    if (!m_drawingItem) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    const QRectF normalized = QRectF(m_startPoint, point).normalized();
    if (auto *rect = qgraphicsitem_cast<QGraphicsRectItem *>(m_drawingItem)) rect->setRect(normalized);
    else if (auto *ellipse = qgraphicsitem_cast<QGraphicsEllipseItem *>(m_drawingItem)) ellipse->setRect(normalized);
    else if (auto *line = qgraphicsitem_cast<QGraphicsLineItem *>(m_drawingItem)) line->setLine(QLineF(m_startPoint, point));
    else if (m_pathItem) { m_path.lineTo(point); m_pathItem->setPath(m_path); }
}

void CanvasView::finishDrawing(const QString &reason)
{
    if (!m_drawingItem) return;
    if (m_drawingItem->sceneBoundingRect().width() < 2.0 && m_drawingItem->sceneBoundingRect().height() < 2.0) {
        scene()->removeItem(m_drawingItem);
        delete m_drawingItem;
    } else {
        Q_EMIT documentCommitted(reason);
    }
    m_drawingItem = nullptr;
    m_pathItem = nullptr;
    m_path = QPainterPath();
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizingText) { m_resizingText = nullptr; m_resizeHandle = -1; rebuildTextFrameHandles(); Q_EMIT documentCommitted(QStringLiteral("调整文本框尺寸")); event->accept(); return; }
    if (m_drawingItem) {
        finishDrawing(QStringLiteral("绘制对象"));
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (m_tool == Tool::Select || m_tool == Tool::Node) Q_EMIT documentCommitted(QStringLiteral("对象操作"));
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_tool == Tool::Bezier && event->button() == Qt::LeftButton && m_bezierItem) {
        const bool openPath = event->modifiers().testFlag(Qt::ShiftModifier);
        finishBezier(!openPath);
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        zoomBy(event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (m_tool == Tool::Bezier && m_bezierItem) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            finishBezier(false);
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            finishBezier(false, false);
            return;
        }
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelection();
        return;
    }
    const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
    QPointF delta;
    if (event->key() == Qt::Key_Left) delta.setX(-step);
    else if (event->key() == Qt::Key_Right) delta.setX(step);
    else if (event->key() == Qt::Key_Up) delta.setY(-step);
    else if (event->key() == Qt::Key_Down) delta.setY(step);
    if (!delta.isNull()) {
        for (QGraphicsItem *item : scene()->selectedItems()) item->moveBy(delta.x(), delta.y());
        Q_EMIT documentCommitted(QStringLiteral("微移对象"));
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::finishBezier(bool closePath, bool commit)
{
    if (!m_bezierItem) return;
    if (m_bezierAnchorCount < 2 || !commit) {
        scene()->removeItem(m_bezierItem);
        delete m_bezierItem;
    } else {
        if (closePath) {
            m_bezierPath.closeSubpath();
            m_bezierItem->setBrush(m_fillColor);
        }
        m_bezierItem->setPath(m_bezierPath);
        Q_EMIT documentCommitted(QStringLiteral("绘制贝塞尔曲线"));
    }
    m_bezierItem = nullptr;
    m_bezierPath = QPainterPath();
    m_bezierAnchorCount = 0;
}

void CanvasView::clearNodeHandles()
{
    const auto handles = m_nodeHandles;
    m_nodeHandles.clear();
    for (QGraphicsItem *handle : handles) delete handle;
}

void CanvasView::rebuildNodeHandles()
{
    clearNodeHandles();
    if (m_tool != Tool::Node) return;
    for (QGraphicsItem *selected : scene()->selectedItems()) {
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(selected);
        if (!pathItem || selected->data(KindRole).toString() != QStringLiteral("path")) continue;
        const QPainterPath path = pathItem->path();
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto element = path.elementAt(i);
            const bool control = element.type == QPainterPath::CurveToElement ||
                                 element.type == QPainterPath::CurveToDataElement;
            auto *handle = new PathNodeHandle(pathItem, i, control);
            m_nodeHandles.append(handle);
        }
    }
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    Q_EMIT viewChanged();
}
