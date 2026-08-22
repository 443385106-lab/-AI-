#include "MainWindow.hpp"
#include "CanvasView.hpp"
#include "DocumentIO.hpp"
#include "RulerWidget.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QAbstractGraphicsShapeItem>
#include <QGraphicsItemGroup>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPathStroker>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPdfWriter>
#include <QPushButton>
#include <QSet>
#include <QStatusBar>
#include <QSvgGenerator>
#include <QToolBar>
#include <QToolButton>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;
constexpr int LayerRole = 2;
constexpr int LockedRole = 3;
constexpr int VisibleRole = 4;
constexpr int TextBoxHeightRole = 5;
constexpr int ParagraphRole = 6;

QString toolName(CanvasView::Tool tool)
{
    switch (tool) {
    case CanvasView::Tool::Select: return QStringLiteral("选择工具");
    case CanvasView::Tool::Node: return QStringLiteral("节点工具");
    case CanvasView::Tool::Bezier: return QStringLiteral("贝塞尔钢笔");
    case CanvasView::Tool::Freehand: return QStringLiteral("自由手绘");
    case CanvasView::Tool::Rectangle: return QStringLiteral("矩形工具");
    case CanvasView::Tool::Ellipse: return QStringLiteral("椭圆工具");
    case CanvasView::Tool::Line: return QStringLiteral("直线工具");
    case CanvasView::Tool::Text: return QStringLiteral("文字工具");
    case CanvasView::Tool::ParagraphText: return QStringLiteral("段落文本工具");
    case CanvasView::Tool::Zoom: return QStringLiteral("缩放工具");
    case CanvasView::Tool::Pan: return QStringLiteral("平移工具");
    }
    return {};
}

QString itemIcon(const QString &kind)
{
    if (kind == "rectangle") return QStringLiteral("□");
    if (kind == "ellipse") return QStringLiteral("○");
    if (kind == "line") return QStringLiteral("╱");
    if (kind == "path") return QStringLiteral("✎");
    if (kind == "text") return QStringLiteral("字");
    if (kind == "group") return QStringLiteral("▣");
    if (kind == "clip") return QStringLiteral("▧");
    return QStringLiteral("◇");
}

QString colorButtonStyle(const QColor &color)
{
    return QStringLiteral("QToolButton{background:%1;border:1px solid #8f969e;min-width:30px;}").arg(color.name());
}

QPainterPath localItemPath(QGraphicsItem *item)
{
    if (auto *rect = dynamic_cast<QGraphicsRectItem *>(item)) {
        QPainterPath path; path.addRect(rect->rect()); return path;
    }
    if (auto *ellipse = dynamic_cast<QGraphicsEllipseItem *>(item)) {
        QPainterPath path; path.addEllipse(ellipse->rect()); return path;
    }
    if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item)) return pathItem->path();
    if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) {
        QPainterPath path; path.moveTo(line->line().p1()); path.lineTo(line->line().p2());
        QPainterPathStroker stroker; stroker.setWidth(qMax(1.0, line->pen().widthF())); return stroker.createStroke(path);
    }
    if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) {
        QPainterPath path; path.addText(QPointF(0.0, text->font().pointSizeF()), text->font(), text->toPlainText()); return path;
    }
    QPainterPath combined;
    for (QGraphicsItem *child : item->childItems()) combined = combined.united(item->mapFromItem(child, localItemPath(child)));
    return combined;
}

QPainterPath sceneItemPath(QGraphicsItem *item)
{
    return item->sceneTransform().map(localItemPath(item));
}

QBrush itemBrush(QGraphicsItem *item)
{
    if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) return shape->brush();
    if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) return QBrush(text->defaultTextColor());
    return QBrush(QColor(244, 197, 66));
}

QPen itemPen(QGraphicsItem *item)
{
    if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) return shape->pen();
    if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) return line->pen();
    return QPen(QColor(34, 34, 34), 2.0);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setMinimumSize(1100, 720);
    resize(1500, 920);
    buildInterface();
    connectSignals();
    newDocument();
}

void MainWindow::buildInterface()
{
    setDockNestingEnabled(true);
    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#dfe3e8;} QMenuBar,QToolBar{background:#f4f5f6;border-color:#c4c9cf;}"
        "QToolBar{spacing:3px;padding:3px;} QDockWidget::title{background:#edf0f3;padding:6px;font-weight:600;}"
        "QListWidget{background:white;border:1px solid #c8cdd3;} QStatusBar{background:#f5f6f7;}"));
    buildCentralCanvas();
    buildMenus();
    buildToolbox();
    buildPropertyBar();
    buildTextAndColorBar();
    buildColorPalette();
    buildDockers();

    m_statusLabel = new QLabel(QStringLiteral("就绪"));
    m_selectionLabel = new QLabel(QStringLiteral("未选择对象"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_cursorLabel = new QLabel(QStringLiteral("X: 0  Y: 0"));
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_selectionLabel);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("RGB工作区 / CMYK输出预留")));
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(m_cursorLabel);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(QStringLiteral("新建(&N)"), QKeySequence::New, this, &MainWindow::newDocument);
    fileMenu->addAction(QStringLiteral("打开…(&O)"), QKeySequence::Open, this, &MainWindow::openDocument);
    fileMenu->addAction(QStringLiteral("保存(&S)"), QKeySequence::Save, this, [this] { saveDocument(false); });
    fileMenu->addAction(QStringLiteral("另存为…"), QKeySequence::SaveAs, this, [this] { saveDocument(true); });
    fileMenu->addSeparator();
    auto *exportMenu = fileMenu->addMenu(QStringLiteral("导出"));
    exportMenu->addAction(QStringLiteral("SVG矢量图…"), this, &MainWindow::exportSvg);
    exportMenu->addAction(QStringLiteral("PDF文件…"), this, &MainWindow::exportPdf);
    exportMenu->addAction(QStringLiteral("PNG高清图…"), this, &MainWindow::exportPng);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), QKeySequence::Quit, this, &QWidget::close);

    auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    editMenu->addAction(QStringLiteral("撤销"), QKeySequence::Undo, this, [this] { if (m_historyIndex > 0) restoreHistory(m_historyIndex - 1); });
    editMenu->addAction(QStringLiteral("重做"), QKeySequence::Redo, this, [this] { if (m_historyIndex + 1 < m_history.size()) restoreHistory(m_historyIndex + 1); });
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("复制"), QKeySequence::Copy, this, &MainWindow::copySelection);
    editMenu->addAction(QStringLiteral("粘贴"), QKeySequence::Paste, this, &MainWindow::pasteSelection);
    editMenu->addAction(QStringLiteral("再制"), QKeySequence(Qt::CTRL | Qt::Key_D), this, &MainWindow::duplicateSelection);
    editMenu->addAction(QStringLiteral("删除"), QKeySequence::Delete, m_canvas, &CanvasView::deleteSelection);
    editMenu->addAction(QStringLiteral("全选"), QKeySequence::SelectAll, m_canvas, &CanvasView::selectAllObjects);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("查看(&V)"));
    auto *gridAction = viewMenu->addAction(QStringLiteral("显示网格")); gridAction->setCheckable(true);
    connect(gridAction, &QAction::toggled, this, [this](bool on) { m_canvas->setGridVisible(on); });
    auto *snapAction = viewMenu->addAction(QStringLiteral("智能吸附")); snapAction->setCheckable(true); snapAction->setChecked(true);
    connect(snapAction, &QAction::toggled, this, [this](bool on) { m_canvas->setSnapEnabled(on); });
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("放大"), QKeySequence::ZoomIn, this, [this] { m_canvas->zoomBy(1.2); });
    viewMenu->addAction(QStringLiteral("缩小"), QKeySequence::ZoomOut, this, [this] { m_canvas->zoomBy(1.0 / 1.2); });
    viewMenu->addAction(QStringLiteral("适合页面"), QKeySequence(Qt::Key_5), m_canvas, &CanvasView::zoomToFit);

    auto *objectMenu = menuBar()->addMenu(QStringLiteral("对象(&O)"));
    objectMenu->addAction(QStringLiteral("组合"), QKeySequence(Qt::CTRL | Qt::Key_G), this, &MainWindow::groupSelection);
    objectMenu->addAction(QStringLiteral("取消组合"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G), this, &MainWindow::ungroupSelection);
    objectMenu->addAction(QStringLiteral("转换为曲线"), QKeySequence(Qt::CTRL | Qt::Key_Q), this, &MainWindow::convertSelectionToPath);
    auto *shapeMenu = objectMenu->addMenu(QStringLiteral("布尔造型"));
    shapeMenu->addAction(QStringLiteral("焊接"), this, [this] { booleanSelection(0); });
    shapeMenu->addAction(QStringLiteral("修剪"), this, [this] { booleanSelection(1); });
    shapeMenu->addAction(QStringLiteral("相交"), this, [this] { booleanSelection(2); });
    shapeMenu->addAction(QStringLiteral("简化"), this, [this] { booleanSelection(3); });
    shapeMenu->addAction(QStringLiteral("前减后"), this, [this] { booleanSelection(4); });
    shapeMenu->addAction(QStringLiteral("后减前"), this, [this] { booleanSelection(5); });
    auto *clipMenu = objectMenu->addMenu(QStringLiteral("图框精确裁剪"));
    clipMenu->addAction(QStringLiteral("置于图框内部"), this, &MainWindow::clipSelection);
    clipMenu->addAction(QStringLiteral("提取图框内容"), this, &MainWindow::releaseClip);
    objectMenu->addSeparator();
    objectMenu->addAction(QStringLiteral("置于顶层"), this, [this] { arrangeSelection(2); });
    objectMenu->addAction(QStringLiteral("上移一层"), this, [this] { arrangeSelection(1); });
    objectMenu->addAction(QStringLiteral("下移一层"), this, [this] { arrangeSelection(-1); });
    objectMenu->addAction(QStringLiteral("置于底层"), this, [this] { arrangeSelection(-2); });
    objectMenu->addSeparator();
    objectMenu->addAction(QStringLiteral("水平镜像"), this, [this] { transformSelection(0); });
    objectMenu->addAction(QStringLiteral("垂直镜像"), this, [this] { transformSelection(1); });
    objectMenu->addAction(QStringLiteral("顺时针旋转90°"), this, [this] { transformSelection(2); });
    objectMenu->addAction(QStringLiteral("逆时针旋转90°"), this, [this] { transformSelection(3); });

    auto *layoutMenu = menuBar()->addMenu(QStringLiteral("布局(&L)"));
    layoutMenu->addAction(QStringLiteral("左对齐"), this, [this] { alignSelection(Qt::AlignLeft); });
    layoutMenu->addAction(QStringLiteral("水平居中"), this, [this] { alignSelection(Qt::AlignHCenter); });
    layoutMenu->addAction(QStringLiteral("右对齐"), this, [this] { alignSelection(Qt::AlignRight); });
    layoutMenu->addSeparator();
    layoutMenu->addAction(QStringLiteral("顶端对齐"), this, [this] { alignSelection(Qt::AlignTop); });
    layoutMenu->addAction(QStringLiteral("垂直居中"), this, [this] { alignSelection(Qt::AlignVCenter); });
    layoutMenu->addAction(QStringLiteral("底端对齐"), this, [this] { alignSelection(Qt::AlignBottom); });
    layoutMenu->addSeparator();
    layoutMenu->addAction(QStringLiteral("水平等距分布"), this, [this] { distributeSelection(true); });
    layoutMenu->addAction(QStringLiteral("垂直等距分布"), this, [this] { distributeSelection(false); });

    auto *effectsMenu = menuBar()->addMenu(QStringLiteral("效果(&C)"));
    effectsMenu->addAction(QStringLiteral("线性渐变填充"), this, [this] { m_fillModeCombo->setCurrentIndex(1); applyInspector(); });
    effectsMenu->addAction(QStringLiteral("径向渐变填充"), this, [this] { m_fillModeCombo->setCurrentIndex(2); applyInspector(); });
    effectsMenu->addAction(QStringLiteral("高级封套/调和模块将在1.5版本启用"));
    menuBar()->addMenu(QStringLiteral("位图(&B)"))->addAction(QStringLiteral("AI描摹模块将在1.5版本启用"));
    auto *textMenu = menuBar()->addMenu(QStringLiteral("文字(&T)"));
    textMenu->addAction(QStringLiteral("添加美术字"), this, [this] { setCurrentTool(CanvasView::Tool::Text); });
    textMenu->addAction(QStringLiteral("添加段落文本"), this, [this] { setCurrentTool(CanvasView::Tool::ParagraphText); });
    textMenu->addAction(QStringLiteral("编辑文字内容…"), this, &MainWindow::editSelectedText);
    textMenu->addAction(QStringLiteral("文本框自动缩字"), this, &MainWindow::autoFitSelectedText);
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于匠心矢量设计"), this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"), QStringLiteral("匠心矢量设计 1.2 Native\n第三阶段：专业文字排版、渐变填充、透明度与快捷色板。\n不包含任何CorelDRAW专有代码或文件规范。"));
    });
}

QAction *MainWindow::addToolAction(const QString &text, const QString &shortcut, CanvasView::Tool tool)
{
    auto *action = new QAction(text, this); action->setCheckable(true); action->setData(static_cast<int>(tool));
    if (!shortcut.isEmpty()) action->setShortcut(QKeySequence(shortcut));
    m_toolActions->addAction(action);
    connect(action, &QAction::triggered, this, [this, tool] { setCurrentTool(tool); });
    return action;
}

void MainWindow::buildToolbox()
{
    m_toolActions = new QActionGroup(this); m_toolActions->setExclusive(true);
    auto *tools = new QToolBar(QStringLiteral("绘图工具"), this); tools->setOrientation(Qt::Vertical); tools->setMovable(false); tools->setIconSize(QSize(28, 28));
    addToolBar(Qt::LeftToolBarArea, tools);
    tools->addAction(addToolAction(QStringLiteral("➤"), QStringLiteral("V"), CanvasView::Tool::Select));
    tools->addAction(addToolAction(QStringLiteral("⌁"), QStringLiteral("N"), CanvasView::Tool::Node));
    tools->addAction(addToolAction(QStringLiteral("⌁P"), QStringLiteral("P"), CanvasView::Tool::Bezier));
    tools->addAction(addToolAction(QStringLiteral("✎"), QStringLiteral("F"), CanvasView::Tool::Freehand));
    tools->addAction(addToolAction(QStringLiteral("□"), QStringLiteral("R"), CanvasView::Tool::Rectangle));
    tools->addAction(addToolAction(QStringLiteral("○"), QStringLiteral("E"), CanvasView::Tool::Ellipse));
    tools->addAction(addToolAction(QStringLiteral("╱"), QStringLiteral("L"), CanvasView::Tool::Line));
    tools->addAction(addToolAction(QStringLiteral("字"), QStringLiteral("T"), CanvasView::Tool::Text));
    tools->addAction(addToolAction(QStringLiteral("¶"), QStringLiteral("A"), CanvasView::Tool::ParagraphText));
    tools->addSeparator();
    tools->addAction(addToolAction(QStringLiteral("⌕"), QStringLiteral("Z"), CanvasView::Tool::Zoom));
    tools->addAction(addToolAction(QStringLiteral("✋"), QStringLiteral("H"), CanvasView::Tool::Pan));
}

void MainWindow::buildPropertyBar()
{
    auto *bar = addToolBar(QStringLiteral("属性栏")); bar->setMovable(false); bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->addWidget(new QLabel(QStringLiteral("  X ")));
    auto makeSpin = [bar](qreal minimum, qreal maximum) { auto *s = new QDoubleSpinBox; s->setRange(minimum, maximum); s->setDecimals(1); s->setMaximumWidth(82); bar->addWidget(s); return s; };
    m_xSpin = makeSpin(-100000, 100000); bar->addWidget(new QLabel(QStringLiteral(" Y "))); m_ySpin = makeSpin(-100000, 100000);
    bar->addSeparator(); bar->addWidget(new QLabel(QStringLiteral(" 宽 "))); m_wSpin = makeSpin(0.1, 100000);
    bar->addWidget(new QLabel(QStringLiteral(" 高 "))); m_hSpin = makeSpin(0.1, 100000);
    bar->addWidget(new QLabel(QStringLiteral(" 旋转 "))); m_rotationSpin = makeSpin(-3600, 3600);
    bar->addSeparator(); bar->addWidget(new QLabel(QStringLiteral(" 填充 ")));
    m_fillButton = new QToolButton; m_fillButton->setStyleSheet(colorButtonStyle(m_fillColor)); m_fillButton->setText(QStringLiteral("   ")); bar->addWidget(m_fillButton);
    bar->addWidget(new QLabel(QStringLiteral(" 轮廓 "))); m_strokeButton = new QToolButton; m_strokeButton->setStyleSheet(colorButtonStyle(m_strokeColor)); m_strokeButton->setText(QStringLiteral("   ")); bar->addWidget(m_strokeButton);
    bar->addWidget(new QLabel(QStringLiteral(" 线宽 "))); m_strokeWidthSpin = makeSpin(0, 100); m_strokeWidthSpin->setValue(2.0);
    bar->addAction(QStringLiteral("应用"), this, &MainWindow::applyInspector);
}

void MainWindow::buildTextAndColorBar()
{
    auto *bar = addToolBar(QStringLiteral("文字与色彩"));
    bar->setMovable(false); bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->addWidget(new QLabel(QStringLiteral(" 字体 ")));
    m_fontCombo = new QFontComboBox; m_fontCombo->setMaximumWidth(155); m_fontCombo->setCurrentFont(QFont(QStringLiteral("Microsoft YaHei"))); bar->addWidget(m_fontCombo);
    bar->addWidget(new QLabel(QStringLiteral(" 字号 ")));
    m_fontSizeSpin = new QDoubleSpinBox; m_fontSizeSpin->setRange(6, 500); m_fontSizeSpin->setDecimals(1); m_fontSizeSpin->setValue(24); m_fontSizeSpin->setMaximumWidth(70); bar->addWidget(m_fontSizeSpin);
    m_boldButton = new QToolButton; m_boldButton->setText(QStringLiteral("B")); m_boldButton->setCheckable(true); m_boldButton->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold)); bar->addWidget(m_boldButton);
    m_italicButton = new QToolButton; m_italicButton->setText(QStringLiteral("I")); m_italicButton->setCheckable(true); QFont italicFont(QStringLiteral("Arial"), 9); italicFont.setItalic(true); m_italicButton->setFont(italicFont); bar->addWidget(m_italicButton);
    m_alignmentCombo = new QComboBox; m_alignmentCombo->addItems({QStringLiteral("左对齐"), QStringLiteral("居中"), QStringLiteral("右对齐"), QStringLiteral("两端对齐")}); m_alignmentCombo->setMaximumWidth(92); bar->addWidget(m_alignmentCombo);
    bar->addWidget(new QLabel(QStringLiteral(" 文本宽 ")));
    m_textWidthSpin = new QDoubleSpinBox; m_textWidthSpin->setRange(0, 100000); m_textWidthSpin->setValue(360); m_textWidthSpin->setMaximumWidth(76); bar->addWidget(m_textWidthSpin);
    bar->addWidget(new QLabel(QStringLiteral(" 高 ")));
    m_textHeightSpin = new QDoubleSpinBox; m_textHeightSpin->setRange(20, 100000); m_textHeightSpin->setValue(260); m_textHeightSpin->setMaximumWidth(70); bar->addWidget(m_textHeightSpin);
    bar->addSeparator(); bar->addWidget(new QLabel(QStringLiteral(" 填充模式 ")));
    m_fillModeCombo = new QComboBox; m_fillModeCombo->addItems({QStringLiteral("纯色"), QStringLiteral("线性渐变"), QStringLiteral("径向渐变"), QStringLiteral("无填充")}); m_fillModeCombo->setMaximumWidth(96); bar->addWidget(m_fillModeCombo);
    m_secondFillButton = new QToolButton; m_secondFillButton->setText(QStringLiteral("渐变色")); m_secondFillButton->setStyleSheet(colorButtonStyle(m_secondFillColor)); bar->addWidget(m_secondFillButton);
    m_outlineStyleCombo = new QComboBox; m_outlineStyleCombo->addItems({QStringLiteral("实线"), QStringLiteral("虚线"), QStringLiteral("点线"), QStringLiteral("点划线")}); m_outlineStyleCombo->setMaximumWidth(82); bar->addWidget(m_outlineStyleCombo);
    bar->addWidget(new QLabel(QStringLiteral(" 透明度 ")));
    m_opacitySpin = new QDoubleSpinBox; m_opacitySpin->setRange(0, 100); m_opacitySpin->setSuffix(QStringLiteral("%")); m_opacitySpin->setValue(100); m_opacitySpin->setMaximumWidth(72); bar->addWidget(m_opacitySpin);
    bar->addAction(QStringLiteral("应用文字/色彩"), this, &MainWindow::applyInspector);
    connect(m_secondFillButton, &QToolButton::clicked, this, &MainWindow::chooseSecondFillColor);
}

void MainWindow::buildColorPalette()
{
    auto *palette = new QToolBar(QStringLiteral("快捷色板"), this); palette->setMovable(false); palette->setIconSize(QSize(18, 18));
    addToolBar(Qt::BottomToolBarArea, palette);
    const QList<QColor> colors {
        QColor("#000000"), QColor("#ffffff"), QColor("#e53935"), QColor("#ff7a00"), QColor("#f5c542"),
        QColor("#43a047"), QColor("#00a7a7"), QColor("#1976d2"), QColor("#3949ab"), QColor("#7b1fa2"),
        QColor("#d81b60"), QColor("#795548"), QColor("#607d8b"), QColor("#cfd8dc"), QColor("#0b4f8a"), QColor("#a50f15")
    };
    palette->addWidget(new QLabel(QStringLiteral(" 快捷填充 ")));
    for (const QColor &color : colors) {
        auto *button = new QToolButton; button->setText(QStringLiteral("  ")); button->setToolTip(color.name().toUpper()); button->setStyleSheet(QStringLiteral("QToolButton{background:%1;border:1px solid #687078;min-width:22px;max-width:22px;min-height:20px;}").arg(color.name()));
        connect(button, &QToolButton::clicked, this, [this, color] { applyQuickColor(color); }); palette->addWidget(button);
    }
}

void MainWindow::buildCentralCanvas()
{
    m_canvas = new CanvasView;
    auto *host = new QWidget; auto *layout = new QGridLayout(host); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *corner = new QWidget; corner->setFixedSize(20, 20); corner->setStyleSheet(QStringLiteral("background:#eef0f2;border-right:1px solid #aeb4bb;border-bottom:1px solid #aeb4bb;"));
    layout->addWidget(corner, 0, 0); layout->addWidget(new RulerWidget(RulerWidget::Orientation::Horizontal, m_canvas), 0, 1);
    layout->addWidget(new RulerWidget(RulerWidget::Orientation::Vertical, m_canvas), 1, 0); layout->addWidget(m_canvas, 1, 1);
    setCentralWidget(host);
}

void MainWindow::buildDockers()
{
    auto *objectsDock = new QDockWidget(QStringLiteral("对象与图层"), this);
    auto *objectPanel = new QWidget;
    auto *objectLayout = new QVBoxLayout(objectPanel);
    objectLayout->setContentsMargins(6, 6, 6, 6);
    objectLayout->addWidget(new QLabel(QStringLiteral("当前图层")));
    m_layerList = new QListWidget;
    m_layerList->setMaximumHeight(150);
    objectLayout->addWidget(m_layerList);
    auto *layerButtons = new QHBoxLayout;
    auto *addLayerButton = new QPushButton(QStringLiteral("+"));
    auto *renameLayerButton = new QPushButton(QStringLiteral("改名"));
    auto *visibleLayerButton = new QPushButton(QStringLiteral("显隐"));
    auto *lockLayerButton = new QPushButton(QStringLiteral("锁定"));
    layerButtons->addWidget(addLayerButton); layerButtons->addWidget(renameLayerButton);
    layerButtons->addWidget(visibleLayerButton); layerButtons->addWidget(lockLayerButton);
    objectLayout->addLayout(layerButtons);
    objectLayout->addWidget(new QLabel(QStringLiteral("对象列表")));
    m_objectList = new QListWidget;
    objectLayout->addWidget(m_objectList, 1);
    connect(addLayerButton, &QPushButton::clicked, this, &MainWindow::addLayer);
    connect(renameLayerButton, &QPushButton::clicked, this, &MainWindow::renameLayer);
    connect(visibleLayerButton, &QPushButton::clicked, this, &MainWindow::toggleLayerVisible);
    connect(lockLayerButton, &QPushButton::clicked, this, &MainWindow::toggleLayerLocked);
    objectsDock->setWidget(objectPanel); addDockWidget(Qt::RightDockWidgetArea, objectsDock);
    objectsDock->setMinimumWidth(270);

    auto *productionDock = new QDockWidget(QStringLiteral("生产与导出"), this); auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel);
    auto *newButton = new QPushButton(QStringLiteral("新建设计")); auto *saveButton = new QPushButton(QStringLiteral("保存工程文件 .jxv"));
    auto *svgButton = new QPushButton(QStringLiteral("导出 SVG 矢量图")); auto *pdfButton = new QPushButton(QStringLiteral("导出 PDF")); auto *pngButton = new QPushButton(QStringLiteral("导出 PNG 高清图"));
    layout->addWidget(new QLabel(QStringLiteral("自主文档格式：JXV\n通用交付格式：SVG / PDF / PNG\nCDR通过已安装CorelDRAW联动生成。")));
    layout->addWidget(newButton); layout->addWidget(saveButton); layout->addWidget(svgButton); layout->addWidget(pdfButton); layout->addWidget(pngButton); layout->addStretch();
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newDocument); connect(saveButton, &QPushButton::clicked, this, [this] { saveDocument(false); });
    connect(svgButton, &QPushButton::clicked, this, &MainWindow::exportSvg); connect(pdfButton, &QPushButton::clicked, this, &MainWindow::exportPdf); connect(pngButton, &QPushButton::clicked, this, &MainWindow::exportPng);
    productionDock->setWidget(panel); addDockWidget(Qt::RightDockWidgetArea, productionDock); tabifyDockWidget(objectsDock, productionDock); objectsDock->raise();
}

void MainWindow::connectSignals()
{
    connect(m_canvas, &CanvasView::documentCommitted, this, &MainWindow::markModified);
    connect(m_canvas->scene(), &QGraphicsScene::selectionChanged, this, &MainWindow::updateInspector);
    connect(m_canvas, &CanvasView::viewChanged, this, [this] { if (m_zoomLabel) m_zoomLabel->setText(QString::number(qRound(m_canvas->transform().m11() * 100.0)) + "%"); });
    connect(m_canvas, &CanvasView::cursorScenePositionChanged, this, [this](const QPointF &p) { if (m_cursorLabel) m_cursorLabel->setText(QStringLiteral("X: %1  Y: %2").arg(qRound(p.x())).arg(qRound(p.y()))); });
    connect(m_canvas, &CanvasView::toolChanged, this, [this](CanvasView::Tool tool) { setStatus(toolName(tool)); });
    connect(m_fillButton, &QToolButton::clicked, this, &MainWindow::chooseFillColor); connect(m_strokeButton, &QToolButton::clicked, this, &MainWindow::chooseStrokeColor);
    connect(m_objectList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return; QList<QGraphicsItem *> roots; for (QGraphicsItem *item : m_canvas->scene()->items(Qt::DescendingOrder)) if (!item->parentItem() && !item->data(KindRole).toString().isEmpty() && (item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString()) == m_currentLayer) roots.append(item); if (row >= roots.size()) return;
        m_canvas->scene()->clearSelection(); roots[row]->setSelected(true); m_canvas->centerOn(roots[row]);
    });
    connect(m_layerList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (!current) return;
        m_currentLayer = current->data(Qt::UserRole).toString();
        m_canvas->setActiveLayer(m_currentLayer);
        updateObjectList();
    });
}

void MainWindow::setCurrentTool(CanvasView::Tool tool)
{
    m_canvas->setTool(tool);
    for (QAction *action : m_toolActions->actions()) if (action->data().toInt() == static_cast<int>(tool)) action->setChecked(true);
}

void MainWindow::updateObjectList()
{
    m_objectList->blockSignals(true); m_objectList->clear();
    for (QGraphicsItem *item : m_canvas->scene()->items(Qt::DescendingOrder)) {
        if (item->parentItem()) continue; const QString kind = item->data(KindRole).toString(); if (kind.isEmpty()) continue;
        const QString layer = item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString();
        if (layer != m_currentLayer) continue;
        const QString state = item->data(LockedRole).toBool() ? QStringLiteral("🔒 ") : QString();
        auto *row = new QListWidgetItem(state + itemIcon(kind) + QStringLiteral("  ") + item->data(NameRole).toString()); row->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(item))); m_objectList->addItem(row);
        if (item->isSelected()) m_objectList->setCurrentItem(row);
    }
    m_objectList->blockSignals(false);
    updateLayerList();
}

void MainWindow::updateInspector()
{
    updateObjectList(); const auto selected = m_canvas->scene()->selectedItems(); const bool enabled = !selected.isEmpty();
    for (QDoubleSpinBox *spin : {m_xSpin, m_ySpin, m_wSpin, m_hSpin, m_rotationSpin, m_strokeWidthSpin}) spin->setEnabled(enabled);
    m_fillButton->setEnabled(enabled); m_strokeButton->setEnabled(enabled);
    if (!enabled) { if (m_selectionLabel) m_selectionLabel->setText(QStringLiteral("未选择对象")); return; }
    QGraphicsItem *item = selected.first(); const QRectF bounds = item->sceneBoundingRect();
    for (QDoubleSpinBox *spin : {m_xSpin, m_ySpin, m_wSpin, m_hSpin, m_rotationSpin, m_strokeWidthSpin}) spin->blockSignals(true);
    m_xSpin->setValue(bounds.x()); m_ySpin->setValue(bounds.y()); m_wSpin->setValue(bounds.width()); m_hSpin->setValue(bounds.height()); m_rotationSpin->setValue(item->rotation());
    m_opacitySpin->setValue(item->opacity() * 100.0);
    if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) {
        const QBrush brush = shape->brush(); m_fillColor = brush.color(); m_strokeColor = shape->pen().color(); m_strokeWidthSpin->setValue(shape->pen().widthF());
        if (const QGradient *gradient = brush.gradient()) {
            m_fillModeCombo->setCurrentIndex(gradient->type() == QGradient::RadialGradient ? 2 : 1);
            const auto stops = gradient->stops(); if (!stops.isEmpty()) { m_fillColor = stops.first().second; m_secondFillColor = stops.last().second; }
        } else m_fillModeCombo->setCurrentIndex(brush.style() == Qt::NoBrush ? 3 : 0);
        const Qt::PenStyle style = shape->pen().style(); m_outlineStyleCombo->setCurrentIndex(style == Qt::DashLine ? 1 : style == Qt::DotLine ? 2 : style == Qt::DashDotLine ? 3 : 0);
    }
    else if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) { m_strokeColor = line->pen().color(); m_strokeWidthSpin->setValue(line->pen().widthF()); }
    else if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) {
        m_fillColor = text->defaultTextColor(); const QFont font = text->font(); m_fontCombo->setCurrentFont(font); m_fontSizeSpin->setValue(font.pointSizeF() > 0 ? font.pointSizeF() : 24.0); m_boldButton->setChecked(font.bold()); m_italicButton->setChecked(font.italic());
        m_textWidthSpin->setValue(text->textWidth() > 0 ? text->textWidth() : 0); m_textHeightSpin->setValue(item->data(TextBoxHeightRole).toDouble() > 0 ? item->data(TextBoxHeightRole).toDouble() : qMax(20.0, text->boundingRect().height()));
        const Qt::Alignment alignment = text->document()->defaultTextOption().alignment(); m_alignmentCombo->setCurrentIndex(alignment.testFlag(Qt::AlignJustify) ? 3 : alignment.testFlag(Qt::AlignRight) ? 2 : alignment.testFlag(Qt::AlignHCenter) ? 1 : 0);
    }
    m_fillButton->setStyleSheet(colorButtonStyle(m_fillColor)); m_strokeButton->setStyleSheet(colorButtonStyle(m_strokeColor));
    m_secondFillButton->setStyleSheet(colorButtonStyle(m_secondFillColor));
    for (QDoubleSpinBox *spin : {m_xSpin, m_ySpin, m_wSpin, m_hSpin, m_rotationSpin, m_strokeWidthSpin}) spin->blockSignals(false);
    if (m_selectionLabel) m_selectionLabel->setText(QStringLiteral("已选择 %1 个对象").arg(selected.size()));
}

void MainWindow::markModified(const QString &reason)
{
    if (m_restoring) return; m_modified = true; recordHistory(reason); updateObjectList(); updateWindowTitle(); setStatus(reason);
}

void MainWindow::recordHistory(const QString &)
{
    if (m_restoring) return; const QJsonObject state = DocumentIO::serializeDocument(m_canvas->scene(), m_canvas->pageRect());
    if (m_historyIndex >= 0 && QJsonDocument(m_history[m_historyIndex]).toJson(QJsonDocument::Compact) == QJsonDocument(state).toJson(QJsonDocument::Compact)) return;
    m_history.resize(m_historyIndex + 1); m_history.append(state); if (m_history.size() > 50) m_history.removeFirst(); m_historyIndex = m_history.size() - 1;
}

void MainWindow::restoreHistory(int index)
{
    if (index < 0 || index >= m_history.size()) return; m_restoring = true; QRectF page; QString error;
    if (DocumentIO::restoreDocument(m_canvas->scene(), m_history[index], &page, &error)) { m_canvas->setPageRect(page); m_historyIndex = index; m_modified = true; applyLayerState(); updateWindowTitle(); setStatus(QStringLiteral("历史记录已恢复")); }
    m_restoring = false;
}

void MainWindow::newDocument()
{
    if (!maybeSave()) return; m_restoring = true; m_canvas->scene()->clear(); m_canvas->setPageRect({100, 100, 800, 600}); m_fileName.clear(); m_modified = false; m_history.clear(); m_historyIndex = -1; m_currentLayer = QStringLiteral("图层 1"); m_canvas->setActiveLayer(m_currentLayer); m_restoring = false;
    recordHistory(QStringLiteral("新建文档")); setCurrentTool(CanvasView::Tool::Select); m_canvas->zoomToFit(); updateObjectList(); updateWindowTitle(); setStatus(QStringLiteral("已新建800×600页面"));
}

bool MainWindow::maybeSave()
{
    if (!m_modified) return true; const auto answer = QMessageBox::question(this, QStringLiteral("保存设计"), QStringLiteral("当前设计已修改，是否保存？"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false; if (answer == QMessageBox::Save) return saveDocument(false); return true;
}

bool MainWindow::saveDocument(bool saveAs)
{
    QString fileName = m_fileName; if (saveAs || fileName.isEmpty()) fileName = QFileDialog::getSaveFileName(this, QStringLiteral("保存匠心矢量文档"), fileName.isEmpty() ? QStringLiteral("未命名.jxv") : fileName, QStringLiteral("匠心矢量文档 (*.jxv)"));
    if (fileName.isEmpty()) return false; if (!fileName.endsWith(".jxv", Qt::CaseInsensitive)) fileName += ".jxv";
    QString error; if (!DocumentIO::saveFile(fileName, DocumentIO::serializeDocument(m_canvas->scene(), m_canvas->pageRect()), &error)) { QMessageBox::critical(this, QStringLiteral("保存失败"), error); return false; }
    m_fileName = fileName; m_modified = false; updateWindowTitle(); setStatus(QStringLiteral("文档已保存")); return true;
}

void MainWindow::openDocument()
{
    if (!maybeSave()) return; const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("打开匠心矢量文档"), {}, QStringLiteral("匠心矢量文档 (*.jxv)")); if (fileName.isEmpty()) return;
    QString error; const QJsonObject document = DocumentIO::loadFile(fileName, &error); QRectF page; if (document.isEmpty() || !DocumentIO::restoreDocument(m_canvas->scene(), document, &page, &error)) { QMessageBox::critical(this, QStringLiteral("打开失败"), error); return; }
    m_canvas->setPageRect(page); m_fileName = fileName; m_modified = false; m_history.clear(); m_historyIndex = -1; recordHistory(QStringLiteral("打开文档")); m_canvas->zoomToFit(); applyLayerState(); updateWindowTitle();
}

void MainWindow::renderForExport(QPainter *painter, const QRectF &target)
{
    const auto selected = m_canvas->scene()->selectedItems(); m_canvas->scene()->clearSelection();
    m_canvas->scene()->render(painter, target, m_canvas->pageRect(), Qt::IgnoreAspectRatio);
    for (QGraphicsItem *item : selected) item->setSelected(true);
}

void MainWindow::exportSvg()
{
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出SVG"), QStringLiteral("设计.svg"), QStringLiteral("SVG矢量图 (*.svg)")); if (fileName.isEmpty()) return; if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) fileName += ".svg";
    QSvgGenerator generator; generator.setFileName(fileName); generator.setSize(m_canvas->pageRect().size().toSize()); generator.setViewBox(QRect(QPoint(), m_canvas->pageRect().size().toSize())); generator.setTitle(QStringLiteral("匠心矢量设计导出"));
    QPainter painter(&generator); renderForExport(&painter, QRectF(QPointF(), m_canvas->pageRect().size())); painter.end(); setStatus(QStringLiteral("SVG导出完成"));
}

void MainWindow::exportPdf()
{
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出PDF"), QStringLiteral("设计.pdf"), QStringLiteral("PDF文件 (*.pdf)")); if (fileName.isEmpty()) return; if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) fileName += ".pdf";
    QPdfWriter writer(fileName); writer.setResolution(300); writer.setPageSize(QPageSize(m_canvas->pageRect().size() / 10.0, QPageSize::Millimeter, QStringLiteral("自定义页面"), QPageSize::ExactMatch)); writer.setPageMargins(QMarginsF(), QPageLayout::Millimeter);
    QPainter painter(&writer); renderForExport(&painter, QRectF(0, 0, writer.width(), writer.height())); painter.end(); setStatus(QStringLiteral("PDF导出完成"));
}

void MainWindow::exportPng()
{
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出PNG"), QStringLiteral("设计.png"), QStringLiteral("PNG高清图 (*.png)")); if (fileName.isEmpty()) return; if (!fileName.endsWith(".png", Qt::CaseInsensitive)) fileName += ".png";
    const QSize size = (m_canvas->pageRect().size() * 3.0).toSize(); QImage image(size, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white); image.setDotsPerMeterX(11811); image.setDotsPerMeterY(11811);
    QPainter painter(&image); painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); renderForExport(&painter, QRectF(QPointF(), size)); painter.end();
    if (!image.save(fileName)) QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法写入PNG文件")); else setStatus(QStringLiteral("300dpi PNG导出完成"));
}

void MainWindow::copySelection()
{
    QList<QGraphicsItem *> roots; for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) if (!item->parentItem()) roots.append(item); m_clipboard = DocumentIO::serializeItems(roots); setStatus(QStringLiteral("已复制 %1 个对象").arg(roots.size()));
}

void MainWindow::pasteSelection()
{
    if (m_clipboard.isEmpty()) return; m_canvas->scene()->clearSelection(); const auto items = DocumentIO::restoreItems(m_canvas->scene(), m_clipboard, {20, 20}); for (QGraphicsItem *item : items) { item->setData(LayerRole, m_currentLayer); item->setSelected(true); } markModified(QStringLiteral("粘贴对象"));
}

void MainWindow::duplicateSelection() { copySelection(); pasteSelection(); }

void MainWindow::groupSelection()
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.size() < 2) { setStatus(QStringLiteral("请至少选择两个对象")); return; }
    auto *group = m_canvas->scene()->createItemGroup(selected); group->setData(KindRole, QStringLiteral("group")); group->setData(NameRole, QStringLiteral("组合对象")); group->setData(LayerRole, m_currentLayer); group->setData(VisibleRole, true); group->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable); group->setSelected(true); markModified(QStringLiteral("组合对象"));
}

void MainWindow::ungroupSelection()
{
    const auto selected = m_canvas->scene()->selectedItems(); bool changed = false; for (QGraphicsItem *item : selected) if (auto *group = dynamic_cast<QGraphicsItemGroup *>(item)) { m_canvas->scene()->destroyItemGroup(group); changed = true; } if (changed) markModified(QStringLiteral("取消组合"));
}

void MainWindow::convertSelectionToPath()
{
    QList<QGraphicsItem *> roots;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) if (!item->parentItem()) roots.append(item);
    if (roots.isEmpty()) return;
    m_canvas->scene()->clearSelection();
    for (QGraphicsItem *item : roots) {
        if (item->data(KindRole).toString() == QStringLiteral("path")) { item->setSelected(true); continue; }
        const QPainterPath path = sceneItemPath(item);
        const QBrush brush = itemBrush(item); const QPen pen = itemPen(item);
        const QString layer = item->data(LayerRole).toString().isEmpty() ? m_currentLayer : item->data(LayerRole).toString();
        m_canvas->scene()->removeItem(item); delete item;
        auto *curve = new QGraphicsPathItem(path); curve->setBrush(brush); curve->setPen(pen);
        curve->setData(KindRole, QStringLiteral("path")); curve->setData(NameRole, QStringLiteral("曲线对象"));
        curve->setData(LayerRole, layer); curve->setData(VisibleRole, true);
        curve->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
        m_canvas->scene()->addItem(curve); curve->setSelected(true);
    }
    markModified(QStringLiteral("已转换为曲线，可使用节点工具编辑"));
    setCurrentTool(CanvasView::Tool::Node);
}

void MainWindow::booleanSelection(int operation)
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) if (!item->parentItem()) items.append(item);
    if (items.size() < 2) { setStatus(QStringLiteral("布尔造型需要至少两个对象")); return; }
    std::sort(items.begin(), items.end(), [](QGraphicsItem *a, QGraphicsItem *b) { return a->zValue() > b->zValue(); });
    QPainterPath result;
    if (operation == 0 || operation == 3) {
        result = sceneItemPath(items.first());
        for (int i = 1; i < items.size(); ++i) result = result.united(sceneItemPath(items[i]));
        if (operation == 3) result = result.simplified();
    } else if (operation == 2) {
        result = sceneItemPath(items.first());
        for (int i = 1; i < items.size(); ++i) result = result.intersected(sceneItemPath(items[i]));
    } else {
        QGraphicsItem *base = operation == 4 ? items.first() : items.last();
        QPainterPath cutters;
        for (QGraphicsItem *item : items) if (item != base) cutters = cutters.united(sceneItemPath(item));
        result = sceneItemPath(base).subtracted(cutters);
    }
    if (result.isEmpty()) { setStatus(QStringLiteral("造型结果为空，请检查对象重叠区域")); return; }
    QGraphicsItem *styleSource = items.first(); const QBrush brush = itemBrush(styleSource); const QPen pen = itemPen(styleSource);
    const QString layer = styleSource->data(LayerRole).toString().isEmpty() ? m_currentLayer : styleSource->data(LayerRole).toString();
    for (QGraphicsItem *item : items) { m_canvas->scene()->removeItem(item); delete item; }
    auto *shape = new QGraphicsPathItem(result); shape->setBrush(brush); shape->setPen(pen);
    shape->setData(KindRole, QStringLiteral("path")); shape->setData(NameRole, QStringLiteral("布尔造型结果"));
    shape->setData(LayerRole, layer); shape->setData(VisibleRole, true);
    shape->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    m_canvas->scene()->addItem(shape); shape->setSelected(true);
    static const QStringList names {QStringLiteral("焊接"), QStringLiteral("修剪"), QStringLiteral("相交"), QStringLiteral("简化"), QStringLiteral("前减后"), QStringLiteral("后减前")};
    markModified(QStringLiteral("布尔造型：") + names.value(operation));
}

void MainWindow::transformSelection(int operation)
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.isEmpty()) return;
    for (QGraphicsItem *item : selected) {
        const QPointF center = item->boundingRect().center();
        QTransform transform; transform.translate(center.x(), center.y());
        if (operation == 0) transform.scale(-1.0, 1.0);
        else if (operation == 1) transform.scale(1.0, -1.0);
        else transform.rotate(operation == 2 ? 90.0 : -90.0);
        transform.translate(-center.x(), -center.y()); item->setTransform(transform, true);
    }
    markModified(QStringLiteral("对象变换完成"));
}

void MainWindow::clipSelection()
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) if (!item->parentItem()) items.append(item);
    if (items.size() < 2) { setStatus(QStringLiteral("请同时选择图框和至少一个内容对象")); return; }
    std::sort(items.begin(), items.end(), [](QGraphicsItem *a, QGraphicsItem *b) { return a->zValue() < b->zValue(); });
    QGraphicsItem *frame = items.takeFirst();
    const QPainterPath framePath = sceneItemPath(frame); const QBrush brush = itemBrush(frame); const QPen pen = itemPen(frame);
    const QString layer = frame->data(LayerRole).toString().isEmpty() ? m_currentLayer : frame->data(LayerRole).toString();
    m_canvas->scene()->removeItem(frame); delete frame;
    auto *clip = new QGraphicsPathItem(framePath); clip->setBrush(brush); clip->setPen(pen);
    clip->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
    clip->setFlags(clip->flags() | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    clip->setData(KindRole, QStringLiteral("clip")); clip->setData(NameRole, QStringLiteral("图框精确裁剪")); clip->setData(LayerRole, layer); clip->setData(VisibleRole, true);
    m_canvas->scene()->addItem(clip);
    for (QGraphicsItem *content : items) {
        const QTransform sceneTransform = content->sceneTransform();
        content->setParentItem(clip); content->setPos(0, 0); content->setRotation(0); content->setTransform(sceneTransform);
    }
    m_canvas->scene()->clearSelection(); clip->setSelected(true); markModified(QStringLiteral("内容已置于图框内部"));
}

void MainWindow::releaseClip()
{
    const auto selected = m_canvas->scene()->selectedItems(); bool changed = false;
    for (QGraphicsItem *item : selected) {
        auto *clip = dynamic_cast<QGraphicsPathItem *>(item);
        if (!clip || item->data(KindRole).toString() != QStringLiteral("clip")) continue;
        const auto children = clip->childItems();
        for (QGraphicsItem *child : children) {
            const QTransform sceneTransform = child->sceneTransform();
            child->setParentItem(nullptr); child->setPos(0, 0); child->setRotation(0); child->setTransform(sceneTransform);
            child->setData(LayerRole, item->data(LayerRole));
        }
        clip->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false); clip->setData(KindRole, QStringLiteral("path")); clip->setData(NameRole, QStringLiteral("图框曲线"));
        changed = true;
    }
    if (changed) markModified(QStringLiteral("已提取图框内容"));
}

void MainWindow::arrangeSelection(int direction)
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.isEmpty()) return; qreal minZ = 0, maxZ = 0; for (QGraphicsItem *item : m_canvas->scene()->items()) { minZ = qMin(minZ, item->zValue()); maxZ = qMax(maxZ, item->zValue()); }
    for (QGraphicsItem *item : selected) item->setZValue(direction == 2 ? maxZ + 1 : direction == -2 ? minZ - 1 : item->zValue() + direction); markModified(QStringLiteral("调整对象层级"));
}

void MainWindow::alignSelection(Qt::Alignment alignment)
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.isEmpty()) return; const QRectF target = selected.size() == 1 ? m_canvas->pageRect() : selected.last()->sceneBoundingRect();
    for (QGraphicsItem *item : selected) { const QRectF b = item->sceneBoundingRect(); qreal dx = 0, dy = 0; if (alignment.testFlag(Qt::AlignLeft)) dx = target.left() - b.left(); else if (alignment.testFlag(Qt::AlignHCenter)) dx = target.center().x() - b.center().x(); else if (alignment.testFlag(Qt::AlignRight)) dx = target.right() - b.right(); if (alignment.testFlag(Qt::AlignTop)) dy = target.top() - b.top(); else if (alignment.testFlag(Qt::AlignVCenter)) dy = target.center().y() - b.center().y(); else if (alignment.testFlag(Qt::AlignBottom)) dy = target.bottom() - b.bottom(); item->moveBy(dx, dy); }
    markModified(QStringLiteral("对象对齐完成"));
}

void MainWindow::distributeSelection(bool horizontal)
{
    QList<QGraphicsItem *> items = m_canvas->scene()->selectedItems();
    if (items.size() < 3) { setStatus(QStringLiteral("等距分布需要至少三个对象")); return; }
    std::sort(items.begin(), items.end(), [horizontal](QGraphicsItem *a, QGraphicsItem *b) {
        return horizontal ? a->sceneBoundingRect().center().x() < b->sceneBoundingRect().center().x()
                          : a->sceneBoundingRect().center().y() < b->sceneBoundingRect().center().y();
    });
    const qreal start = horizontal ? items.first()->sceneBoundingRect().center().x() : items.first()->sceneBoundingRect().center().y();
    const qreal end = horizontal ? items.last()->sceneBoundingRect().center().x() : items.last()->sceneBoundingRect().center().y();
    const qreal step = (end - start) / (items.size() - 1);
    for (int i = 1; i + 1 < items.size(); ++i) {
        const QRectF bounds = items[i]->sceneBoundingRect(); const qreal current = horizontal ? bounds.center().x() : bounds.center().y();
        if (horizontal) items[i]->moveBy(start + step * i - current, 0); else items[i]->moveBy(0, start + step * i - current);
    }
    markModified(horizontal ? QStringLiteral("水平等距分布完成") : QStringLiteral("垂直等距分布完成"));
}

void MainWindow::updateLayerList()
{
    if (!m_layerList) return;
    QSet<QString> layers; layers.insert(QStringLiteral("图层 1")); layers.insert(m_currentLayer);
    for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && !item->data(KindRole).toString().isEmpty()) layers.insert(item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString());
    QStringList names = layers.values(); names.sort(Qt::CaseInsensitive);
    m_layerList->blockSignals(true); m_layerList->clear();
    for (const QString &name : names) {
        bool visible = true, locked = false;
        for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && (item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString()) == name) { visible = !item->data(VisibleRole).isValid() || item->data(VisibleRole).toBool(); locked = item->data(LockedRole).toBool(); break; }
        auto *row = new QListWidgetItem(QStringLiteral("%1 %2 %3").arg(visible ? QStringLiteral("👁") : QStringLiteral("○"), locked ? QStringLiteral("🔒") : QStringLiteral("  "), name));
        row->setData(Qt::UserRole, name); row->setData(Qt::UserRole + 1, visible); row->setData(Qt::UserRole + 2, locked); m_layerList->addItem(row);
        if (name == m_currentLayer) m_layerList->setCurrentItem(row);
    }
    m_layerList->blockSignals(false);
}

void MainWindow::addLayer()
{
    bool ok = false; const QString suggested = QStringLiteral("图层 %1").arg(m_layerList->count() + 1);
    const QString name = QInputDialog::getText(this, QStringLiteral("新建图层"), QStringLiteral("图层名称"), QLineEdit::Normal, suggested, &ok).trimmed();
    if (!ok || name.isEmpty()) return; m_currentLayer = name; m_canvas->setActiveLayer(name); updateLayerList(); setStatus(QStringLiteral("已新建图层：") + name);
}

void MainWindow::renameLayer()
{
    if (!m_layerList->currentItem()) return; const QString oldName = m_currentLayer; bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名图层"), QStringLiteral("新名称"), QLineEdit::Normal, oldName, &ok).trimmed();
    if (!ok || name.isEmpty() || name == oldName) return;
    for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && (item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString()) == oldName) item->setData(LayerRole, name);
    m_currentLayer = name; m_canvas->setActiveLayer(name); markModified(QStringLiteral("图层已重命名"));
}

void MainWindow::toggleLayerVisible()
{
    if (!m_layerList->currentItem()) return; const bool visible = !m_layerList->currentItem()->data(Qt::UserRole + 1).toBool();
    for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && (item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString()) == m_currentLayer) item->setData(VisibleRole, visible);
    applyLayerState(); markModified(visible ? QStringLiteral("图层已显示") : QStringLiteral("图层已隐藏"));
}

void MainWindow::toggleLayerLocked()
{
    if (!m_layerList->currentItem()) return; const bool locked = !m_layerList->currentItem()->data(Qt::UserRole + 2).toBool();
    for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && (item->data(LayerRole).toString().isEmpty() ? QStringLiteral("图层 1") : item->data(LayerRole).toString()) == m_currentLayer) item->setData(LockedRole, locked);
    applyLayerState(); markModified(locked ? QStringLiteral("图层已锁定") : QStringLiteral("图层已解锁"));
}

void MainWindow::applyLayerState()
{
    for (QGraphicsItem *item : m_canvas->scene()->items()) {
        if (item->parentItem() || item->data(KindRole).toString().isEmpty()) continue;
        const bool visible = !item->data(VisibleRole).isValid() || item->data(VisibleRole).toBool(); const bool locked = item->data(LockedRole).toBool();
        item->setVisible(visible); item->setFlag(QGraphicsItem::ItemIsSelectable, !locked); item->setFlag(QGraphicsItem::ItemIsMovable, !locked); if (locked) item->setSelected(false);
    }
    updateObjectList();
}

void MainWindow::applyInspector()
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.isEmpty()) return; QGraphicsItem *first = selected.first(); const QRectF old = first->sceneBoundingRect(); first->moveBy(m_xSpin->value() - old.x(), m_ySpin->value() - old.y()); const QRectF moved = first->sceneBoundingRect(); if (moved.width() > 0.01 && moved.height() > 0.01) first->setTransform(QTransform::fromScale(m_wSpin->value() / moved.width(), m_hSpin->value() / moved.height()), true); first->setRotation(m_rotationSpin->value());
    const QList<Qt::PenStyle> styles {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    for (QGraphicsItem *item : selected) {
        item->setOpacity(m_opacitySpin->value() / 100.0);
        if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) {
            shape->setBrush(selectedFillBrush(shape->boundingRect())); QPen pen = shape->pen(); pen.setColor(m_strokeColor); pen.setWidthF(m_strokeWidthSpin->value()); pen.setStyle(styles.value(m_outlineStyleCombo->currentIndex(), Qt::SolidLine)); shape->setPen(pen);
        } else if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) {
            QPen pen = line->pen(); pen.setColor(m_strokeColor); pen.setWidthF(m_strokeWidthSpin->value()); pen.setStyle(styles.value(m_outlineStyleCombo->currentIndex(), Qt::SolidLine)); line->setPen(pen);
        } else if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) {
            QFont font = m_fontCombo->currentFont(); font.setPointSizeF(m_fontSizeSpin->value()); font.setBold(m_boldButton->isChecked()); font.setItalic(m_italicButton->isChecked()); text->setFont(font); text->setDefaultTextColor(m_fillColor);
            text->setTextWidth(m_textWidthSpin->value() <= 0.1 ? -1.0 : m_textWidthSpin->value()); text->setData(TextBoxHeightRole, m_textHeightSpin->value()); text->setData(ParagraphRole, m_textWidthSpin->value() > 0.1);
            QTextOption option = text->document()->defaultTextOption(); const QList<Qt::Alignment> alignments {Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight, Qt::AlignJustify}; option.setAlignment(alignments.value(m_alignmentCombo->currentIndex(), Qt::AlignLeft)); text->document()->setDefaultTextOption(option);
        }
    }
    markModified(QStringLiteral("文字与色彩属性已应用")); updateInspector();
}

QBrush MainWindow::selectedFillBrush(const QRectF &bounds) const
{
    if (m_fillModeCombo->currentIndex() == 3) return QBrush(Qt::NoBrush);
    if (m_fillModeCombo->currentIndex() == 1) {
        QLinearGradient gradient(bounds.topLeft(), bounds.bottomRight()); gradient.setColorAt(0.0, m_fillColor); gradient.setColorAt(1.0, m_secondFillColor); return QBrush(gradient);
    }
    if (m_fillModeCombo->currentIndex() == 2) {
        QRadialGradient gradient(bounds.center(), qMax(bounds.width(), bounds.height()) / 2.0); gradient.setColorAt(0.0, m_fillColor); gradient.setColorAt(1.0, m_secondFillColor); return QBrush(gradient);
    }
    return QBrush(m_fillColor);
}

void MainWindow::editSelectedText()
{
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) {
        auto *text = dynamic_cast<QGraphicsTextItem *>(item); if (!text) continue; bool ok = false;
        const QString value = QInputDialog::getMultiLineText(this, QStringLiteral("编辑文字内容"), QStringLiteral("文字"), text->toPlainText(), &ok);
        if (ok) { text->setPlainText(value); item->setData(NameRole, item->data(ParagraphRole).toBool() ? QStringLiteral("段落文本") : value.left(20)); markModified(QStringLiteral("文字内容已更新")); }
        return;
    }
    setStatus(QStringLiteral("请先选择一个文字对象"));
}

void MainWindow::autoFitSelectedText()
{
    bool changed = false;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) {
        auto *text = dynamic_cast<QGraphicsTextItem *>(item); if (!text) continue;
        const qreal maximumHeight = item->data(TextBoxHeightRole).toDouble() > 0 ? item->data(TextBoxHeightRole).toDouble() : m_textHeightSpin->value();
        QFont font = text->font(); qreal size = font.pointSizeF() > 0 ? font.pointSizeF() : m_fontSizeSpin->value();
        while (text->boundingRect().height() > maximumHeight && size > 6.0) { size -= 0.5; font.setPointSizeF(size); text->setFont(font); }
        changed = true;
    }
    if (changed) markModified(QStringLiteral("文本溢出检测完成，字号已自动适配")); else setStatus(QStringLiteral("请先选择段落文本"));
}

void MainWindow::applyQuickColor(const QColor &color)
{
    m_fillColor = color; m_fillModeCombo->setCurrentIndex(0); m_fillButton->setStyleSheet(colorButtonStyle(color)); m_canvas->setFillColor(color);
    const auto selected = m_canvas->scene()->selectedItems();
    for (QGraphicsItem *item : selected) {
        if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) shape->setBrush(color);
        else if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) text->setDefaultTextColor(color);
    }
    if (!selected.isEmpty()) markModified(QStringLiteral("快捷填充颜色已应用"));
}

void MainWindow::chooseFillColor()
{
    const QColor color = QColorDialog::getColor(m_fillColor, this, QStringLiteral("选择填充颜色"), QColorDialog::ShowAlphaChannel); if (!color.isValid()) return; m_fillColor = color; m_fillButton->setStyleSheet(colorButtonStyle(color)); m_canvas->setFillColor(color);
}

void MainWindow::chooseStrokeColor()
{
    const QColor color = QColorDialog::getColor(m_strokeColor, this, QStringLiteral("选择轮廓颜色"), QColorDialog::ShowAlphaChannel); if (!color.isValid()) return; m_strokeColor = color; m_strokeButton->setStyleSheet(colorButtonStyle(color)); m_canvas->setStrokeColor(color);
}

void MainWindow::chooseSecondFillColor()
{
    const QColor color = QColorDialog::getColor(m_secondFillColor, this, QStringLiteral("选择渐变结束颜色"), QColorDialog::ShowAlphaChannel); if (!color.isValid()) return; m_secondFillColor = color; m_secondFillButton->setStyleSheet(colorButtonStyle(color));
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_fileName.isEmpty() ? QStringLiteral("未命名.jxv") : QFileInfo(m_fileName).fileName(); setWindowTitle(QStringLiteral("%1%2 — 匠心矢量设计 1.2 Native").arg(m_modified ? "*" : "", name));
}

void MainWindow::setStatus(const QString &message) { if (m_statusLabel) m_statusLabel->setText(message); }

void MainWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
