#include "MainWindow.hpp"
#include "CanvasView.hpp"
#include "DocumentIO.hpp"
#include "RulerWidget.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsShapeItem>
#include <QGraphicsTextItem>
#include <QGridLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QStatusBar>
#include <QSvgGenerator>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;

QString toolName(CanvasView::Tool tool)
{
    switch (tool) {
    case CanvasView::Tool::Select: return QStringLiteral("选择工具");
    case CanvasView::Tool::Node: return QStringLiteral("节点工具");
    case CanvasView::Tool::Freehand: return QStringLiteral("手绘/钢笔");
    case CanvasView::Tool::Rectangle: return QStringLiteral("矩形工具");
    case CanvasView::Tool::Ellipse: return QStringLiteral("椭圆工具");
    case CanvasView::Tool::Line: return QStringLiteral("直线工具");
    case CanvasView::Tool::Text: return QStringLiteral("文字工具");
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
    return QStringLiteral("◇");
}

QString colorButtonStyle(const QColor &color)
{
    return QStringLiteral("QToolButton{background:%1;border:1px solid #8f969e;min-width:30px;}").arg(color.name());
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
    objectMenu->addSeparator();
    objectMenu->addAction(QStringLiteral("置于顶层"), this, [this] { arrangeSelection(2); });
    objectMenu->addAction(QStringLiteral("上移一层"), this, [this] { arrangeSelection(1); });
    objectMenu->addAction(QStringLiteral("下移一层"), this, [this] { arrangeSelection(-1); });
    objectMenu->addAction(QStringLiteral("置于底层"), this, [this] { arrangeSelection(-2); });

    auto *layoutMenu = menuBar()->addMenu(QStringLiteral("布局(&L)"));
    layoutMenu->addAction(QStringLiteral("左对齐"), this, [this] { alignSelection(Qt::AlignLeft); });
    layoutMenu->addAction(QStringLiteral("水平居中"), this, [this] { alignSelection(Qt::AlignHCenter); });
    layoutMenu->addAction(QStringLiteral("右对齐"), this, [this] { alignSelection(Qt::AlignRight); });
    layoutMenu->addSeparator();
    layoutMenu->addAction(QStringLiteral("顶端对齐"), this, [this] { alignSelection(Qt::AlignTop); });
    layoutMenu->addAction(QStringLiteral("垂直居中"), this, [this] { alignSelection(Qt::AlignVCenter); });
    layoutMenu->addAction(QStringLiteral("底端对齐"), this, [this] { alignSelection(Qt::AlignBottom); });

    menuBar()->addMenu(QStringLiteral("效果(&C)"))->addAction(QStringLiteral("高级效果模块将在1.5版本启用"));
    menuBar()->addMenu(QStringLiteral("位图(&B)"))->addAction(QStringLiteral("AI描摹模块将在1.5版本启用"));
    menuBar()->addMenu(QStringLiteral("文字(&T)"))->addAction(QStringLiteral("添加美术字"), this, [this] { setCurrentTool(CanvasView::Tool::Text); });
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于匠心矢量设计"), this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"), QStringLiteral("匠心矢量设计 1.0 Native\n兼容专业图文设计工作流程的国产矢量编辑器。\n不包含任何CorelDRAW专有代码或文件规范。"));
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
    tools->addAction(addToolAction(QStringLiteral("✎"), QStringLiteral("P"), CanvasView::Tool::Freehand));
    tools->addAction(addToolAction(QStringLiteral("□"), QStringLiteral("R"), CanvasView::Tool::Rectangle));
    tools->addAction(addToolAction(QStringLiteral("○"), QStringLiteral("E"), CanvasView::Tool::Ellipse));
    tools->addAction(addToolAction(QStringLiteral("╱"), QStringLiteral("L"), CanvasView::Tool::Line));
    tools->addAction(addToolAction(QStringLiteral("字"), QStringLiteral("T"), CanvasView::Tool::Text));
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
    auto *objectsDock = new QDockWidget(QStringLiteral("对象与图层"), this); m_objectList = new QListWidget; objectsDock->setWidget(m_objectList); addDockWidget(Qt::RightDockWidgetArea, objectsDock);
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
        if (row < 0) return; QList<QGraphicsItem *> roots; for (QGraphicsItem *item : m_canvas->scene()->items(Qt::DescendingOrder)) if (!item->parentItem() && !item->data(KindRole).toString().isEmpty()) roots.append(item); if (row >= roots.size()) return;
        m_canvas->scene()->clearSelection(); roots[row]->setSelected(true); m_canvas->centerOn(roots[row]);
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
        auto *row = new QListWidgetItem(itemIcon(kind) + QStringLiteral("  ") + item->data(NameRole).toString()); row->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(item))); m_objectList->addItem(row);
        if (item->isSelected()) m_objectList->setCurrentItem(row);
    }
    m_objectList->blockSignals(false);
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
    if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) { m_fillColor = shape->brush().color(); m_strokeColor = shape->pen().color(); m_strokeWidthSpin->setValue(shape->pen().widthF()); }
    else if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) { m_strokeColor = line->pen().color(); m_strokeWidthSpin->setValue(line->pen().widthF()); }
    else if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) m_fillColor = text->defaultTextColor();
    m_fillButton->setStyleSheet(colorButtonStyle(m_fillColor)); m_strokeButton->setStyleSheet(colorButtonStyle(m_strokeColor));
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
    if (DocumentIO::restoreDocument(m_canvas->scene(), m_history[index], &page, &error)) { m_canvas->setPageRect(page); m_historyIndex = index; m_modified = true; updateObjectList(); updateWindowTitle(); setStatus(QStringLiteral("历史记录已恢复")); }
    m_restoring = false;
}

void MainWindow::newDocument()
{
    if (!maybeSave()) return; m_restoring = true; m_canvas->scene()->clear(); m_canvas->setPageRect({100, 100, 800, 600}); m_fileName.clear(); m_modified = false; m_history.clear(); m_historyIndex = -1; m_restoring = false;
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
    m_canvas->setPageRect(page); m_fileName = fileName; m_modified = false; m_history.clear(); m_historyIndex = -1; recordHistory(QStringLiteral("打开文档")); m_canvas->zoomToFit(); updateObjectList(); updateWindowTitle();
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
    if (m_clipboard.isEmpty()) return; m_canvas->scene()->clearSelection(); const auto items = DocumentIO::restoreItems(m_canvas->scene(), m_clipboard, {20, 20}); for (QGraphicsItem *item : items) item->setSelected(true); markModified(QStringLiteral("粘贴对象"));
}

void MainWindow::duplicateSelection() { copySelection(); pasteSelection(); }

void MainWindow::groupSelection()
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.size() < 2) { setStatus(QStringLiteral("请至少选择两个对象")); return; }
    auto *group = m_canvas->scene()->createItemGroup(selected); group->setData(KindRole, QStringLiteral("group")); group->setData(NameRole, QStringLiteral("组合对象")); group->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable); group->setSelected(true); markModified(QStringLiteral("组合对象"));
}

void MainWindow::ungroupSelection()
{
    const auto selected = m_canvas->scene()->selectedItems(); bool changed = false; for (QGraphicsItem *item : selected) if (auto *group = dynamic_cast<QGraphicsItemGroup *>(item)) { m_canvas->scene()->destroyItemGroup(group); changed = true; } if (changed) markModified(QStringLiteral("取消组合"));
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

void MainWindow::applyInspector()
{
    const auto selected = m_canvas->scene()->selectedItems(); if (selected.isEmpty()) return; QGraphicsItem *first = selected.first(); const QRectF old = first->sceneBoundingRect(); first->moveBy(m_xSpin->value() - old.x(), m_ySpin->value() - old.y()); const QRectF moved = first->sceneBoundingRect(); if (moved.width() > 0.01 && moved.height() > 0.01) first->setTransform(QTransform::fromScale(m_wSpin->value() / moved.width(), m_hSpin->value() / moved.height()), true); first->setRotation(m_rotationSpin->value());
    for (QGraphicsItem *item : selected) { if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) { shape->setBrush(m_fillColor); QPen pen = shape->pen(); pen.setColor(m_strokeColor); pen.setWidthF(m_strokeWidthSpin->value()); shape->setPen(pen); } else if (auto *line = dynamic_cast<QGraphicsLineItem *>(item)) { QPen pen = line->pen(); pen.setColor(m_strokeColor); pen.setWidthF(m_strokeWidthSpin->value()); line->setPen(pen); } else if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) text->setDefaultTextColor(m_fillColor); }
    markModified(QStringLiteral("对象属性已应用")); updateInspector();
}

void MainWindow::chooseFillColor()
{
    const QColor color = QColorDialog::getColor(m_fillColor, this, QStringLiteral("选择填充颜色"), QColorDialog::ShowAlphaChannel); if (!color.isValid()) return; m_fillColor = color; m_fillButton->setStyleSheet(colorButtonStyle(color)); m_canvas->setFillColor(color);
}

void MainWindow::chooseStrokeColor()
{
    const QColor color = QColorDialog::getColor(m_strokeColor, this, QStringLiteral("选择轮廓颜色"), QColorDialog::ShowAlphaChannel); if (!color.isValid()) return; m_strokeColor = color; m_strokeButton->setStyleSheet(colorButtonStyle(color)); m_canvas->setStrokeColor(color);
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_fileName.isEmpty() ? QStringLiteral("未命名.jxv") : QFileInfo(m_fileName).fileName(); setWindowTitle(QStringLiteral("%1%2 — 匠心矢量设计 1.0 Native").arg(m_modified ? "*" : "", name));
}

void MainWindow::setStatus(const QString &message) { if (m_statusLabel) m_statusLabel->setText(message); }

void MainWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
