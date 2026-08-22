#include "RulerWidget.hpp"
#include "CanvasView.hpp"

#include <QPainter>
#include <cmath>

RulerWidget::RulerWidget(Orientation orientation, CanvasView *view, QWidget *parent)
    : QWidget(parent), m_orientation(orientation), m_view(view)
{
    setMinimumSize(20, 20);
    connect(m_view, &CanvasView::viewChanged, this, [this] { update(); });
}

QSize RulerWidget::sizeHint() const
{
    return m_orientation == Orientation::Horizontal ? QSize(300, 20) : QSize(20, 300);
}

void RulerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(246, 247, 248));
    painter.setPen(QColor(145, 151, 159));
    painter.drawLine(m_orientation == Orientation::Horizontal ? rect().bottomLeft() : rect().topRight(),
                     m_orientation == Orientation::Horizontal ? rect().bottomRight() : rect().bottomRight());

    const QRectF page = m_view->pageRect();
    const qreal scale = m_view->transform().m11();
    qreal major = 100.0;
    if (scale > 2.0) major = 50.0;
    if (scale < 0.45) major = 200.0;
    const qreal minor = major / 10.0;
    painter.setFont(QFont(QStringLiteral("Arial"), 7));

    if (m_orientation == Orientation::Horizontal) {
        const qreal first = std::floor(page.left() / minor) * minor;
        for (qreal x = first; x <= page.right() + minor; x += minor) {
            const int px = m_view->mapFromScene(QPointF(x, page.top())).x();
            if (px < 0 || px > width()) continue;
            const bool isMajor = std::abs(std::remainder(x - page.left(), major)) < 0.1;
            painter.drawLine(px, height(), px, height() - (isMajor ? 11 : 5));
            if (isMajor) painter.drawText(px + 2, 9, QString::number(qRound(x - page.left())));
        }
    } else {
        const qreal first = std::floor(page.top() / minor) * minor;
        for (qreal y = first; y <= page.bottom() + minor; y += minor) {
            const int py = m_view->mapFromScene(QPointF(page.left(), y)).y();
            if (py < 0 || py > height()) continue;
            const bool isMajor = std::abs(std::remainder(y - page.top(), major)) < 0.1;
            painter.drawLine(width(), py, width() - (isMajor ? 11 : 5), py);
            if (isMajor) {
                painter.save();
                painter.translate(8, py + 2);
                painter.rotate(-90);
                painter.drawText(0, 0, QString::number(qRound(y - page.top())));
                painter.restore();
            }
        }
    }
}
