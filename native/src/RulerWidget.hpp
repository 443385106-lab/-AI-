#pragma once

#include <QWidget>

class CanvasView;

class RulerWidget final : public QWidget
{
    Q_OBJECT

public:
    enum class Orientation { Horizontal, Vertical };
    RulerWidget(Orientation orientation, CanvasView *view, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Orientation m_orientation;
    CanvasView *m_view;
};

