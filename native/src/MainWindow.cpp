#include "MainWindow.hpp"
#include "CanvasView.hpp"
#include "DocumentIO.hpp"
#include "RulerWidget.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFontComboBox>
#include <QAbstractGraphicsShapeItem>
#include <QGraphicsItemGroup>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsSvgItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
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
#include <QProgressDialog>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QToolBar>
#include <QToolButton>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr int KindRole = 0;
constexpr int NameRole = 1;
constexpr int LayerRole = 2;
constexpr int LockedRole = 3;
constexpr int VisibleRole = 4;
constexpr int TextBoxHeightRole = 5;
constexpr int ParagraphRole = 6;
constexpr int SvgDataRole = 7;

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
    if (kind == "bitmap") return QStringLiteral("▦");
    if (kind == "svg") return QStringLiteral("◆");
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
    if (auto *bitmap = dynamic_cast<QGraphicsPixmapItem *>(item)) {
        QPainterPath path; path.addRect(bitmap->boundingRect()); return path;
    }
    if (auto *svg = dynamic_cast<QGraphicsSvgItem *>(item)) {
        QPainterPath path; path.addRect(svg->boundingRect()); return path;
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

struct BoardTheme {
    QString name;
    QColor primary;
    QColor accent;
    QColor background;
};

QList<BoardTheme> boardThemes()
{
    return {
        {QStringLiteral("政务蓝"), QColor("#145DA0"), QColor("#5CA9E6"), QColor("#F4F9FD")},
        {QStringLiteral("安全红"), QColor("#B3262D"), QColor("#E9A3A6"), QColor("#FFF7F5")},
        {QStringLiteral("食品绿"), QColor("#237A4B"), QColor("#78BE91"), QColor("#F3FAF5")},
        {QStringLiteral("车间橙"), QColor("#C95F18"), QColor("#F0B16C"), QColor("#FFF8ED")},
        {QStringLiteral("医疗蓝"), QColor("#117F9F"), QColor("#70C4D5"), QColor("#F1FBFD")},
        {QStringLiteral("简约金"), QColor("#8C6317"), QColor("#D6B565"), QColor("#FFFBF0")}
    };
}

QStringList boardSizeLabels()
{
    return {QStringLiteral("40×60厘米 竖版"), QStringLiteral("50×70厘米 竖版"), QStringLiteral("60×80厘米 竖版"),
            QStringLiteral("60×90厘米 竖版"), QStringLiteral("80×120厘米 竖版"), QStringLiteral("60×40厘米 横版"),
            QStringLiteral("70×50厘米 横版"), QStringLiteral("80×60厘米 横版")};
}

QSizeF boardSizeMillimeters(const QString &label)
{
    if (label.startsWith(QStringLiteral("50×70"))) return {500, 700};
    if (label.startsWith(QStringLiteral("60×80"))) return {600, 800};
    if (label.startsWith(QStringLiteral("60×90"))) return {600, 900};
    if (label.startsWith(QStringLiteral("80×120"))) return {800, 1200};
    if (label.startsWith(QStringLiteral("60×40"))) return {600, 400};
    if (label.startsWith(QStringLiteral("70×50"))) return {700, 500};
    if (label.startsWith(QStringLiteral("80×60"))) return {800, 600};
    return {400, 600};
}

QString policyBodyForTitle(const QString &title)
{
    if (title.contains(QStringLiteral("消防")) || title.contains(QStringLiteral("防火"))) return QStringLiteral(
        "一、严格落实消防安全责任制，明确责任人、管理人和各岗位职责。\n"
        "二、保持疏散通道、安全出口和消防车通道畅通，严禁占用、堵塞或锁闭。\n"
        "三、按规定配置灭火器、消火栓、应急照明和疏散标志，定期检查并做好记录。\n"
        "四、严禁违规动火、私拉乱接电线和超负荷用电，重点部位实行专人管理。\n"
        "五、每日开展防火巡查，发现隐患立即整改；不能立即整改的，应采取防范措施并上报。\n"
        "六、定期组织消防培训和应急演练，使员工掌握报警、灭火、疏散和逃生方法。\n"
        "七、发生火情立即报警，启动预案，组织人员有序疏散，严禁贪恋财物或盲目施救。\n"
        "八、消防检查、培训、演练和隐患整改资料应真实完整，按规定归档备查。");
    if (title.contains(QStringLiteral("食品")) || title.contains(QStringLiteral("卫生"))) return QStringLiteral(
        "一、严格执行食品安全法律法规和操作规范，落实食品安全主体责任。\n"
        "二、从业人员须持有效健康证明上岗，保持个人卫生，按要求穿戴清洁工作衣帽。\n"
        "三、采购食品及原料应查验供货者资质、合格证明和票据，建立完整进货记录。\n"
        "四、食品分类、分架、离墙、离地存放，生熟分开，防止交叉污染。\n"
        "五、严格控制加工温度、时间和储存条件，不得使用过期、腐败变质或来源不明食品。\n"
        "六、场所、设备、工具和餐饮具及时清洗消毒，防鼠、防蝇、防虫设施保持有效。\n"
        "七、每日开展食品安全自查，发现问题立即停止相关操作并采取整改措施。\n"
        "八、发生疑似食品安全事故时，立即封存相关食品和记录，及时报告并配合调查。");
    if (title.contains(QStringLiteral("仓库")) || title.contains(QStringLiteral("库房")) || title.contains(QStringLiteral("出入库"))) return QStringLiteral(
        "一、物资入库必须核对名称、规格、数量、质量和凭证，验收合格后方可入账。\n"
        "二、库内物资分类分区、定置标识，做到帐、卡、物相符，严禁混放和超高堆码。\n"
        "三、执行先进先出和保质期管理，定期盘点，发现差异及时查明原因并上报。\n"
        "四、仓库保持通风、干燥、整洁，落实防火、防潮、防盗、防虫和防污染措施。\n"
        "五、易燃、易爆、危险化学品按性质专区存放，设置警示标识并落实专人管理。\n"
        "六、物资出库凭有效手续办理，复核品名、规格和数量，未经批准不得擅自领用。\n"
        "七、消防通道和安全出口保持畅通，库区严禁烟火，电气设备使用后及时关闭。\n"
        "八、出入库、盘点、报损和异常处置记录应真实完整，按规定保存备查。");
    if (title.contains(QStringLiteral("岗位职责")) || title.contains(QStringLiteral("职责"))) return QStringLiteral(
        "一、遵守国家法律法规和单位各项规章制度，服从工作安排，认真履行岗位职责。\n"
        "二、熟悉本岗位工作流程、质量要求和安全风险，按标准完成各项任务。\n"
        "三、上岗前检查工作环境、设备、工具和防护用品，发现异常及时报告。\n"
        "四、严格执行操作规程，不违章指挥、不违章作业，有权制止不安全行为。\n"
        "五、做好工作记录、交接班和资料保管，确保信息真实、准确、完整。\n"
        "六、维护现场秩序和环境卫生，做到物品定置、区域整洁、通道畅通。\n"
        "七、发生突发情况立即采取力所能及的措施并逐级报告，配合应急处置。\n"
        "八、主动参加培训和考核，持续改进工作质量，完成上级交办的其他任务。");
    if (title.contains(QStringLiteral("操作规程")) || title.contains(QStringLiteral("设备"))) return QStringLiteral(
        "一、操作人员须经培训考核合格，熟悉设备性能、风险和应急处置方法后方可上岗。\n"
        "二、开机前检查电源、防护装置、急停按钮、工具和作业区域，确认正常后启动。\n"
        "三、按规定穿戴劳动防护用品，严禁佩戴可能卷入设备的饰物，长发应盘入帽内。\n"
        "四、严格按工艺参数和操作顺序作业，设备运行时不得离岗、拆卸防护或徒手排障。\n"
        "五、发现异响、异味、振动或温升异常时立即停机断电，悬挂警示标识并报告。\n"
        "六、清理、调整、维修和更换部件必须执行停机、断电、挂牌等能量隔离措施。\n"
        "七、作业结束后关闭设备和电源，清理现场，工具归位，填写运行及交接记录。\n"
        "八、未经许可不得擅自改变设备结构、保护装置、控制程序和工艺参数。");
    if (title.contains(QStringLiteral("应急"))) return QStringLiteral(
        "一、坚持统一指挥、快速响应、以人为本、科学处置的原则，最大限度减少损失。\n"
        "二、明确应急负责人、联络人员、疏散引导、现场警戒、救护和后勤保障职责。\n"
        "三、发现险情立即报告，说明地点、类型、人员伤亡和现场情况，必要时拨打紧急电话。\n"
        "四、立即启动相应预案，切断危险能源，划定警戒区域，组织无关人员有序撤离。\n"
        "五、救援人员应正确佩戴防护用品，在确保自身安全的前提下实施初期处置。\n"
        "六、保持疏散通道畅通，到达安全区域后清点人数，不得擅自返回危险区域。\n"
        "七、保护事故现场和相关证据，配合专业救援及调查，不得迟报、漏报、瞒报。\n"
        "八、事后及时总结评估，补充应急物资，整改暴露问题，并按计划组织培训演练。");
    if (title.contains(QStringLiteral("质量")) || title.contains(QStringLiteral("检验"))) return QStringLiteral(
        "一、坚持质量第一、预防为主、全员参与、持续改进，严格执行技术标准和工艺文件。\n"
        "二、原辅材料入场应按规定检验，未经确认或检验不合格的物料不得投入使用。\n"
        "三、落实首件确认、过程巡检和成品检验，关键工序实行重点控制并保留记录。\n"
        "四、作业人员做好自检、互检，不接收、不制造、不流转不合格品。\n"
        "五、不合格品应及时标识、隔离、评审和处置，严禁与合格品混放或擅自放行。\n"
        "六、仪器设备按期检定校准，保持状态有效；检验数据必须真实、准确、可追溯。\n"
        "七、发生质量异常立即停止相关工序，查明原因，制定并验证纠正预防措施。\n"
        "八、定期分析质量数据和客户反馈，持续改进工艺、管理和产品质量。");
    return QStringLiteral(
        "一、严格遵守国家有关法律法规、行业标准和单位各项规章制度。\n"
        "二、按照职责分工落实责任，明确工作要求、办理流程和完成时限。\n"
        "三、工作人员应经培训后上岗，熟悉岗位风险、操作要求和应急处置方法。\n"
        "四、工作前认真检查环境、设施、设备和相关资料，确认符合要求后开展作业。\n"
        "五、工作中严格执行规定程序，做好沟通、复核和记录，严禁弄虚作假。\n"
        "六、定期开展检查和隐患排查，发现问题立即整改；不能立即整改的及时上报。\n"
        "七、发生异常或突发事件时，立即采取有效措施，保护人员安全并按程序报告。\n"
        "八、各类记录和档案应真实、完整、清晰，按规定保存，持续改进管理工作。");
}

void prepareBoardItem(QGraphicsItem *item, const QString &kind, const QString &name, QGraphicsScene *scene, qreal z)
{
    item->setData(KindRole, kind); item->setData(NameRole, name); item->setData(LayerRole, QStringLiteral("智能展板")); item->setData(VisibleRole, true); item->setZValue(z);
    item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable); scene->addItem(item);
}

void addBoardDesign(QGraphicsScene *scene, const QRectF &page, const QString &title, const QString &body, const QString &footer, const BoardTheme &theme, qreal headerRatio = 0.125, qreal marginRatio = 0.045)
{
    const qreal shortSide = qMin(page.width(), page.height()); const qreal margin = shortSide * qBound(0.025, marginRatio, 0.09); const qreal borderWidth = qMax(8.0, shortSide * 0.0045);
    auto *background = new QGraphicsRectItem(page); background->setPen(Qt::NoPen); background->setBrush(theme.background); prepareBoardItem(background, QStringLiteral("rectangle"), QStringLiteral("展板底色"), scene, -10.0);
    auto *outer = new QGraphicsRectItem(page.adjusted(margin * 0.48, margin * 0.48, -margin * 0.48, -margin * 0.48)); outer->setBrush(Qt::NoBrush); outer->setPen(QPen(theme.primary, borderWidth)); prepareBoardItem(outer, QStringLiteral("rectangle"), QStringLiteral("外边框"), scene, -5.0);
    auto *inner = new QGraphicsRectItem(page.adjusted(margin, margin, -margin, -margin)); inner->setBrush(Qt::NoBrush); inner->setPen(QPen(theme.accent, borderWidth * 0.42)); prepareBoardItem(inner, QStringLiteral("rectangle"), QStringLiteral("内边框"), scene, -4.0);
    const qreal headerHeight = page.height() * qBound(0.09, headerRatio, 0.24); const QRectF header(page.left() + margin, page.top() + margin, page.width() - margin * 2.0, headerHeight);
    auto *bar = new QGraphicsRectItem(header); bar->setPen(Qt::NoPen); bar->setBrush(theme.primary); prepareBoardItem(bar, QStringLiteral("rectangle"), QStringLiteral("标题栏"), scene, -2.0);
    auto *titleItem = new QGraphicsTextItem(title); QFont titleFont(QStringLiteral("Microsoft YaHei")); titleFont.setBold(true); titleFont.setPointSizeF(qMax(32.0, shortSide * (title.size() > 14 ? 0.024 : 0.031))); titleItem->setFont(titleFont); titleItem->setDefaultTextColor(Qt::white); titleItem->setTextWidth(header.width() - margin * 0.8); titleItem->document()->setDocumentMargin(0); QTextOption titleOption = titleItem->document()->defaultTextOption(); titleOption.setAlignment(Qt::AlignHCenter); titleItem->document()->setDefaultTextOption(titleOption); titleItem->setPos(header.left() + margin * 0.4, header.center().y() - titleItem->boundingRect().height() / 2.0); prepareBoardItem(titleItem, QStringLiteral("text"), QStringLiteral("制度标题"), scene, 2.0);
    const qreal bodyTop = header.bottom() + margin * 0.75; const qreal footerHeight = qMax(100.0, page.height() * 0.055); const qreal bodyHeight = page.bottom() - margin * 1.8 - footerHeight - bodyTop;
    auto *bodyItem = new QGraphicsTextItem(body); QFont bodyFont(QStringLiteral("Microsoft YaHei")); bodyFont.setPointSizeF(qMax(18.0, shortSide * 0.0175)); bodyItem->setFont(bodyFont); bodyItem->setDefaultTextColor(QColor("#20252A")); bodyItem->setTextWidth(page.width() - margin * 3.0); bodyItem->document()->setDocumentMargin(0); QTextOption bodyOption = bodyItem->document()->defaultTextOption(); bodyOption.setAlignment(Qt::AlignJustify); bodyOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); bodyItem->document()->setDefaultTextOption(bodyOption); bodyItem->setPos(page.left() + margin * 1.5, bodyTop); bodyItem->setData(TextBoxHeightRole, bodyHeight); bodyItem->setData(ParagraphRole, true);
    while (bodyItem->boundingRect().height() > bodyHeight && bodyFont.pointSizeF() > 14.0) { bodyFont.setPointSizeF(bodyFont.pointSizeF() - 1.0); bodyItem->setFont(bodyFont); }
    prepareBoardItem(bodyItem, QStringLiteral("text"), QStringLiteral("制度正文"), scene, 1.0);
    if (!footer.trimmed().isEmpty()) {
        auto *footerItem = new QGraphicsTextItem(footer.trimmed()); QFont footerFont(QStringLiteral("Microsoft YaHei")); footerFont.setBold(true); footerFont.setPointSizeF(qMax(16.0, shortSide * 0.014)); footerItem->setFont(footerFont); footerItem->setDefaultTextColor(theme.primary); footerItem->setTextWidth(page.width() - margin * 3.0); footerItem->document()->setDocumentMargin(0); QTextOption option = footerItem->document()->defaultTextOption(); option.setAlignment(Qt::AlignRight); footerItem->document()->setDefaultTextOption(option); footerItem->setPos(page.left() + margin * 1.5, page.bottom() - margin * 1.25 - footerItem->boundingRect().height()); prepareBoardItem(footerItem, QStringLiteral("text"), QStringLiteral("落款"), scene, 1.0);
    }
}

QString templateDirectoryPath()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/templates"); QDir().mkpath(path); return path;
}

QString safeFileStem(QString name)
{
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), QStringLiteral("_")); name = name.trimmed().left(80); return name.isEmpty() ? QStringLiteral("未命名模板") : name;
}

void ensureBuiltinTemplates()
{
    const QString directory = templateDirectoryPath(); const QList<BoardTheme> themes = boardThemes();
    const QStringList categories {QStringLiteral("政务"), QStringLiteral("安全生产"), QStringLiteral("食品"), QStringLiteral("制造车间"), QStringLiteral("医疗"), QStringLiteral("通用")};
    for (int index = 0; index < themes.size(); ++index) {
        const QString path = QDir(directory).filePath(QStringLiteral("builtin-%1.jxvt").arg(index)); if (QFileInfo::exists(path)) continue;
        QGraphicsScene scene; const QRectF page(0, 0, 4000, 6000); const QString title = index == 2 ? QStringLiteral("食品安全管理制度") : index == 3 ? QStringLiteral("安全生产管理制度") : index == 4 ? QStringLiteral("岗位工作制度") : QStringLiteral("管理制度");
        addBoardDesign(&scene, page, title, policyBodyForTitle(title), QStringLiteral("单位名称"), themes[index]);
        QJsonObject root {{"format", "JiangxinBoardTemplate"}, {"name", themes[index].name + QStringLiteral("标准制度牌")}, {"category", categories.value(index, QStringLiteral("通用"))}, {"favorite", false}, {"builtin", true}, {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"document", DocumentIO::serializeDocument(&scene, page)}};
        QString error; DocumentIO::saveFile(path, root, &error);
    }
}

QColor averageImageRegion(const QImage &image, const QRect &area)
{
    const QRect clipped = area.intersected(image.rect()); if (clipped.isEmpty()) return Qt::white; qint64 r = 0, g = 0, b = 0, count = 0;
    const int step = qMax(1, qMin(clipped.width(), clipped.height()) / 24);
    for (int y = clipped.top(); y <= clipped.bottom(); y += step) for (int x = clipped.left(); x <= clipped.right(); x += step) { const QColor color = image.pixelColor(x, y); if (color.alpha() < 32) continue; r += color.red(); g += color.green(); b += color.blue(); ++count; }
    return count > 0 ? QColor(r / count, g / count, b / count) : QColor(Qt::white);
}

int colorDistance(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}

BoardTheme analyzeImageTheme(const QImage &source, qreal *headerRatio, qreal *marginRatio, bool *logoCandidate)
{
    const QImage image = source.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    const int blockW = qMax(4, image.width() / 10), blockH = qMax(4, image.height() / 10);
    const QList<QColor> corners {averageImageRegion(image, QRect(0, 0, blockW, blockH)), averageImageRegion(image, QRect(image.width() - blockW, 0, blockW, blockH)), averageImageRegion(image, QRect(0, image.height() - blockH, blockW, blockH)), averageImageRegion(image, QRect(image.width() - blockW, image.height() - blockH, blockW, blockH))};
    QColor background((corners[0].red() + corners[1].red() + corners[2].red() + corners[3].red()) / 4, (corners[0].green() + corners[1].green() + corners[2].green() + corners[3].green()) / 4, (corners[0].blue() + corners[1].blue() + corners[2].blue() + corners[3].blue()) / 4);
    QHash<int, int> bins; for (int y = 0; y < image.height(); y += 2) for (int x = 0; x < image.width(); x += 2) { const QColor c = image.pixelColor(x, y); if (c.alpha() < 32 || colorDistance(c, background) < 55) continue; const int key = (c.red() / 24) * 121 + (c.green() / 24) * 11 + c.blue() / 24; bins[key] += 1 + c.hsvSaturation() / 32; }
    int bestKey = -1, bestScore = -1; for (auto it = bins.cbegin(); it != bins.cend(); ++it) if (it.value() > bestScore) { bestKey = it.key(); bestScore = it.value(); }
    QColor primary = bestKey >= 0 ? QColor(((bestKey / 121) % 11) * 24 + 12, ((bestKey / 11) % 11) * 24 + 12, (bestKey % 11) * 24 + 12) : QColor("#145DA0");
    if (primary.lightness() > 205) primary = primary.darker(180); QColor accent = primary.lighter(155); background = background.lightness() < 175 ? background.lighter(210) : background.lighter(105);
    int lastStrongRow = -1; for (int y = 0; y < qRound(image.height() * 0.34); ++y) { const QColor row = averageImageRegion(image, QRect(0, y, image.width(), 1)); if (colorDistance(row, background) > 75) lastStrongRow = y; }
    *headerRatio = lastStrongRow > 0 ? qBound(0.09, (lastStrongRow + 1.0) / image.height(), 0.22) : 0.125;
    int inset = 0; const int centerY = image.height() / 2; for (int x = 0; x < image.width() / 5; ++x) if (colorDistance(image.pixelColor(x, centerY), background) > 65) { inset = x; break; }
    *marginRatio = inset > 0 ? qBound(0.025, inset / static_cast<qreal>(qMin(image.width(), image.height())), 0.085) : 0.045;
    const QRect logoArea(0, 0, image.width() / 3, image.height() / 4); int different = 0, sampled = 0; for (int y = logoArea.top(); y < logoArea.bottom(); y += 3) for (int x = logoArea.left(); x < logoArea.right(); x += 3) { ++sampled; if (colorDistance(image.pixelColor(x, y), background) > 95) ++different; }
    *logoCandidate = sampled > 0 && different > sampled / 7;
    return {QStringLiteral("样图分析"), primary, accent, background};
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
    fileMenu->addAction(QStringLiteral("导入SVG矢量图…"), this, &MainWindow::importSvg);
    fileMenu->addSeparator();
    auto *exportMenu = fileMenu->addMenu(QStringLiteral("导出"));
    exportMenu->addAction(QStringLiteral("SVG矢量图…"), this, &MainWindow::exportSvg);
    exportMenu->addAction(QStringLiteral("PDF文件…"), this, &MainWindow::exportPdf);
    exportMenu->addAction(QStringLiteral("PNG高清图…"), this, &MainWindow::exportPng);
    exportMenu->addAction(QStringLiteral("JPG高清图…"), this, [this] { exportImage(QStringLiteral("JPG")); });
    exportMenu->addAction(QStringLiteral("TIFF印刷图…"), this, [this] { exportImage(QStringLiteral("TIFF")); });
    exportMenu->addSeparator();
    exportMenu->addAction(QStringLiteral("印刷PDF（出血与裁切线）…"), this, &MainWindow::exportPrintPdf);
    exportMenu->addAction(QStringLiteral("批量导出JXV文件夹…"), this, &MainWindow::batchExport);
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
    layoutMenu->addAction(QStringLiteral("页面尺寸设置…"), this, &MainWindow::pageSetup);
    layoutMenu->addSeparator();
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

    auto *prepressMenu = menuBar()->addMenu(QStringLiteral("印前(&P)"));
    prepressMenu->addAction(QStringLiteral("出血与裁切线设置…"), this, &MainWindow::configurePrintSettings);
    prepressMenu->addAction(QStringLiteral("印前预检"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), this, &MainWindow::preflightDocument);
    prepressMenu->addAction(QStringLiteral("导出印刷PDF…"), this, &MainWindow::exportPrintPdf);

    auto *smartMenu = menuBar()->addMenu(QStringLiteral("智能展板(&I)"));
    smartMenu->addAction(QStringLiteral("输入制度名称自动生成…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this, &MainWindow::generateSmartBoard);
    smartMenu->addAction(QStringLiteral("批量生成独立制度牌…"), this, &MainWindow::batchGenerateBoards);
    smartMenu->addAction(QStringLiteral("上传样图分析并仿制版式…"), this, &MainWindow::analyzeSampleLayout);

    auto *templateMenu = menuBar()->addMenu(QStringLiteral("模板库(&M)"));
    templateMenu->addAction(QStringLiteral("打开本地模板库…"), this, &MainWindow::openTemplateLibrary);
    templateMenu->addAction(QStringLiteral("将当前设计保存为模板…"), this, &MainWindow::saveCurrentAsTemplate);

    auto *effectsMenu = menuBar()->addMenu(QStringLiteral("效果(&C)"));
    effectsMenu->addAction(QStringLiteral("线性渐变填充"), this, [this] { m_fillModeCombo->setCurrentIndex(1); applyInspector(); });
    effectsMenu->addAction(QStringLiteral("径向渐变填充"), this, [this] { m_fillModeCombo->setCurrentIndex(2); applyInspector(); });
    effectsMenu->addSeparator();
    effectsMenu->addAction(QStringLiteral("生成轮廓图…"), this, &MainWindow::createContour);
    effectsMenu->addAction(QStringLiteral("矢量阴影…"), this, &MainWindow::addVectorShadow);
    effectsMenu->addAction(QStringLiteral("对象调和…"), this, &MainWindow::createBlend);
    auto *envelopeMenu = effectsMenu->addMenu(QStringLiteral("封套变形"));
    envelopeMenu->addAction(QStringLiteral("上窄下宽"), this, [this] { applyEnvelope(0); });
    envelopeMenu->addAction(QStringLiteral("上宽下窄"), this, [this] { applyEnvelope(1); });
    envelopeMenu->addAction(QStringLiteral("拱形"), this, [this] { applyEnvelope(2); });
    envelopeMenu->addAction(QStringLiteral("旗形"), this, [this] { applyEnvelope(3); });
    auto *bitmapMenu = menuBar()->addMenu(QStringLiteral("位图(&B)"));
    bitmapMenu->addAction(QStringLiteral("导入图片…"), this, &MainWindow::importBitmap);
    bitmapMenu->addSeparator();
    bitmapMenu->addAction(QStringLiteral("转换为灰度"), this, [this] { adjustBitmap(0); });
    bitmapMenu->addAction(QStringLiteral("黑白阈值"), this, [this] { adjustBitmap(1); });
    bitmapMenu->addAction(QStringLiteral("提亮"), this, [this] { adjustBitmap(2); });
    bitmapMenu->addSeparator();
    bitmapMenu->addAction(QStringLiteral("基础位图转矢量"), this, &MainWindow::traceBitmap);
    auto *textMenu = menuBar()->addMenu(QStringLiteral("文字(&T)"));
    textMenu->addAction(QStringLiteral("添加美术字"), this, [this] { setCurrentTool(CanvasView::Tool::Text); });
    textMenu->addAction(QStringLiteral("添加段落文本"), this, [this] { setCurrentTool(CanvasView::Tool::ParagraphText); });
    textMenu->addAction(QStringLiteral("编辑文字内容…"), this, &MainWindow::editSelectedText);
    textMenu->addAction(QStringLiteral("文本框自动缩字"), this, &MainWindow::autoFitSelectedText);
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于匠心矢量设计"), this, [this] {
        QMessageBox::about(this, QStringLiteral("关于"), QStringLiteral("匠心矢量设计 1.6 Native\n第七阶段：样图颜色与结构分析、可编辑仿制版式、本地模板收藏、搜索和行业分类。\n不包含任何CorelDRAW专有代码或文件规范。"));
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
    auto *svgButton = new QPushButton(QStringLiteral("导出 SVG 矢量图")); auto *pdfButton = new QPushButton(QStringLiteral("导出普通 PDF")); auto *pngButton = new QPushButton(QStringLiteral("导出 PNG 高清图"));
    auto *smartButton = new QPushButton(QStringLiteral("智能生成制度展板")); auto *sampleButton = new QPushButton(QStringLiteral("上传样图仿制版式")); auto *templateButton = new QPushButton(QStringLiteral("本地模板库")); auto *printButton = new QPushButton(QStringLiteral("印前预检并导出印刷 PDF")); auto *batchButton = new QPushButton(QStringLiteral("批量生成独立制度牌"));
    layout->addWidget(new QLabel(QStringLiteral("自主文档格式：JXV\n交付格式：SVG / PDF / PNG / JPG / TIFF\n印刷输出：300dpi、出血与裁切线。")));
    layout->addWidget(smartButton); layout->addWidget(sampleButton); layout->addWidget(templateButton); layout->addWidget(newButton); layout->addWidget(saveButton); layout->addWidget(svgButton); layout->addWidget(pdfButton); layout->addWidget(pngButton); layout->addWidget(printButton); layout->addWidget(batchButton); layout->addStretch();
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newDocument); connect(saveButton, &QPushButton::clicked, this, [this] { saveDocument(false); });
    connect(svgButton, &QPushButton::clicked, this, &MainWindow::exportSvg); connect(pdfButton, &QPushButton::clicked, this, &MainWindow::exportPdf); connect(pngButton, &QPushButton::clicked, this, &MainWindow::exportPng);
    connect(smartButton, &QPushButton::clicked, this, &MainWindow::generateSmartBoard); connect(sampleButton, &QPushButton::clicked, this, &MainWindow::analyzeSampleLayout); connect(templateButton, &QPushButton::clicked, this, &MainWindow::openTemplateLibrary); connect(printButton, &QPushButton::clicked, this, [this] { preflightDocument(); exportPrintPdf(); }); connect(batchButton, &QPushButton::clicked, this, &MainWindow::batchGenerateBoards);
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

void MainWindow::pageSetup()
{
    QStringList choices = boardSizeLabels(); choices.append(QStringLiteral("自定义尺寸")); bool ok = false;
    const QString choice = QInputDialog::getItem(this, QStringLiteral("页面尺寸"), QStringLiteral("选择成品尺寸"), choices, 0, false, &ok); if (!ok) return;
    QSizeF millimeters;
    if (choice == QStringLiteral("自定义尺寸")) {
        const qreal width = QInputDialog::getDouble(this, QStringLiteral("自定义尺寸"), QStringLiteral("宽度（毫米）"), m_canvas->pageRect().width() / 10.0, 20.0, 3000.0, 1, &ok); if (!ok) return;
        const qreal height = QInputDialog::getDouble(this, QStringLiteral("自定义尺寸"), QStringLiteral("高度（毫米）"), m_canvas->pageRect().height() / 10.0, 20.0, 3000.0, 1, &ok); if (!ok) return; millimeters = {width, height};
    } else millimeters = boardSizeMillimeters(choice);
    const QPointF topLeft = m_canvas->pageRect().topLeft(); m_canvas->setPageRect(QRectF(topLeft, millimeters * 10.0)); m_canvas->zoomToFit();
    markModified(QStringLiteral("页面尺寸已设置为 %1×%2 mm").arg(millimeters.width()).arg(millimeters.height()));
}

void MainWindow::generateSmartBoard()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("智能生成制度展板")); dialog.resize(620, 560); QFormLayout layout(&dialog);
    auto *titleEdit = new QLineEdit; titleEdit->setPlaceholderText(QStringLiteral("例如：消防安全管理制度"));
    auto *bodyEdit = new QPlainTextEdit; bodyEdit->setPlaceholderText(QStringLiteral("正文可留空，由本地规则根据制度名称生成；也可以粘贴自己的正文。"));
    auto *footerEdit = new QLineEdit; footerEdit->setPlaceholderText(QStringLiteral("公司名称或落款（可留空）"));
    auto *themeCombo = new QComboBox; for (const BoardTheme &theme : boardThemes()) themeCombo->addItem(theme.name);
    auto *sizeCombo = new QComboBox; sizeCombo->addItems(boardSizeLabels());
    auto *localContent = new QCheckBox(QStringLiteral("正文为空时，根据标题生成本地制度草稿")); localContent->setChecked(true);
    auto *notice = new QLabel(QStringLiteral("提示：自动内容是排版草稿，涉及法规、医疗、消防、食品和安全生产时必须由专业人员审核。")); notice->setWordWrap(true); notice->setStyleSheet(QStringLiteral("color:#a34721;padding:6px;background:#fff5e9;"));
    layout.addRow(QStringLiteral("制度名称"), titleEdit); layout.addRow(QStringLiteral("正文内容"), bodyEdit); layout.addRow(QStringLiteral("落款"), footerEdit); layout.addRow(QStringLiteral("行业版式"), themeCombo); layout.addRow(QStringLiteral("成品尺寸"), sizeCombo); layout.addRow(localContent); layout.addRow(notice);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成展板")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout.addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return; const QString title = titleEdit->text().trimmed(); if (title.isEmpty()) { QMessageBox::warning(this, QStringLiteral("缺少名称"), QStringLiteral("请输入制度名称")); return; }
    QString body = bodyEdit->toPlainText().trimmed(); if (body.isEmpty() && localContent->isChecked()) body = policyBodyForTitle(title); if (body.isEmpty()) { QMessageBox::warning(this, QStringLiteral("缺少正文"), QStringLiteral("请输入正文，或启用本地内容生成")); return; }
    if (!maybeSave()) return; const QList<BoardTheme> themes = boardThemes(); const QSizeF sizeMm = boardSizeMillimeters(sizeCombo->currentText()); const QRectF page(100, 100, sizeMm.width() * 10.0, sizeMm.height() * 10.0);
    m_restoring = true; m_canvas->scene()->clear(); m_canvas->setPageRect(page); m_fileName.clear(); m_history.clear(); m_historyIndex = -1; m_currentLayer = QStringLiteral("智能展板"); m_canvas->setActiveLayer(m_currentLayer);
    addBoardDesign(m_canvas->scene(), page, title, body, footerEdit->text(), themes.value(themeCombo->currentIndex(), themes.first()));
    m_restoring = false; m_modified = true; recordHistory(QStringLiteral("智能生成制度展板")); m_canvas->zoomToFit(); updateObjectList(); updateWindowTitle(); setStatus(QStringLiteral("制度展板已自动生成：") + title);
}

void MainWindow::batchGenerateBoards()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("批量生成独立制度牌")); dialog.resize(620, 520); QFormLayout layout(&dialog);
    auto *titlesEdit = new QPlainTextEdit; titlesEdit->setPlaceholderText(QStringLiteral("每行一个制度名称，最多20个\n例如：\n安全生产管理制度\n消防安全管理制度\n仓库管理制度"));
    auto *footerEdit = new QLineEdit; footerEdit->setPlaceholderText(QStringLiteral("统一公司名称或落款（可留空）"));
    auto *themeCombo = new QComboBox; for (const BoardTheme &theme : boardThemes()) themeCombo->addItem(theme.name);
    auto *sizeCombo = new QComboBox; sizeCombo->addItems(boardSizeLabels());
    auto *outputCombo = new QComboBox; outputCombo->addItems({QStringLiteral("JXV工程 + PDF印刷文件（推荐）"), QStringLiteral("JXV + PDF + PNG 300dpi"), QStringLiteral("仅JXV工程")});
    auto *notice = new QLabel(QStringLiteral("每个名称会生成独立文件，并自动匹配消防、食品、仓库、职责、设备、应急、质量等本地内容规则。自动草稿必须人工审核。")); notice->setWordWrap(true);
    layout.addRow(QStringLiteral("制度名称"), titlesEdit); layout.addRow(QStringLiteral("统一落款"), footerEdit); layout.addRow(QStringLiteral("行业版式"), themeCombo); layout.addRow(QStringLiteral("成品尺寸"), sizeCombo); layout.addRow(QStringLiteral("输出文件"), outputCombo); layout.addRow(notice);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("选择文件夹并生成")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout.addRow(buttons); connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return; QStringList titles; for (const QString &line : titlesEdit->toPlainText().split('\n', Qt::SkipEmptyParts)) if (!line.trimmed().isEmpty()) titles.append(line.trimmed());
    if (titles.isEmpty()) { QMessageBox::warning(this, QStringLiteral("缺少名称"), QStringLiteral("请至少输入一个制度名称")); return; } if (titles.size() > 20) titles = titles.mid(0, 20);
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择批量生成文件夹")); if (directory.isEmpty()) return;
    const QList<BoardTheme> themes = boardThemes(); const BoardTheme theme = themes.value(themeCombo->currentIndex(), themes.first()); const QSizeF sizeMm = boardSizeMillimeters(sizeCombo->currentText()); const QRectF page(0, 0, sizeMm.width() * 10.0, sizeMm.height() * 10.0); const bool makePdf = outputCombo->currentIndex() < 2; const bool makePng = outputCombo->currentIndex() == 1;
    QProgressDialog progress(QStringLiteral("正在生成独立制度牌…"), QStringLiteral("取消"), 0, titles.size(), this); progress.setWindowModality(Qt::WindowModal); int succeeded = 0; QStringList failures;
    for (int index = 0; index < titles.size(); ++index) {
        progress.setValue(index); progress.setLabelText(titles[index]); if (progress.wasCanceled()) break; QGraphicsScene scene; addBoardDesign(&scene, page, titles[index], policyBodyForTitle(titles[index]), footerEdit->text(), theme);
        QString safeName = titles[index]; safeName.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), QStringLiteral("_")); safeName = safeName.left(80); const QString base = QDir(directory).filePath(QStringLiteral("%1-%2").arg(index + 1, 2, 10, QLatin1Char('0')).arg(safeName)); QString error;
        bool saved = DocumentIO::saveFile(base + QStringLiteral(".jxv"), DocumentIO::serializeDocument(&scene, page), &error);
        if (saved && makePdf) { QPdfWriter writer(base + QStringLiteral(".pdf")); writer.setResolution(300); writer.setPageSize(QPageSize(sizeMm, QPageSize::Millimeter, QStringLiteral("制度展板"), QPageSize::ExactMatch)); writer.setPageMargins(QMarginsF(), QPageLayout::Millimeter); QPainter painter(&writer); scene.render(&painter, QRectF(0, 0, writer.width(), writer.height()), page, Qt::IgnoreAspectRatio); painter.end(); }
        if (saved && makePng) { const qreal scale = 300.0 / 254.0; const QSize pixels = (page.size() * scale).toSize(); QImage image(pixels, QImage::Format_ARGB32_Premultiplied); if (image.isNull()) saved = false; else { image.fill(Qt::white); image.setDotsPerMeterX(11811); image.setDotsPerMeterY(11811); QPainter painter(&image); painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); scene.render(&painter, QRectF(QPointF(), pixels), page, Qt::IgnoreAspectRatio); painter.end(); saved = image.save(base + QStringLiteral(".png"), "PNG"); } }
        if (saved) ++succeeded; else failures.append(titles[index] + QStringLiteral("：") + (error.isEmpty() ? QStringLiteral("输出失败") : error));
    }
    progress.setValue(titles.size()); QString result = QStringLiteral("已生成 %1/%2 张独立制度牌。\n输出文件夹：%3\n\n自动生成内容属于排版草稿，上墙或交付前请逐张审核。").arg(succeeded).arg(titles.size()).arg(QDir::toNativeSeparators(directory)); if (!failures.isEmpty()) result += QStringLiteral("\n\n失败：\n") + failures.join('\n'); QMessageBox::information(this, QStringLiteral("批量生成完成"), result); setStatus(QStringLiteral("批量生成完成：%1张").arg(succeeded));
}

void MainWindow::analyzeSampleLayout()
{
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("选择需要分析的样图"), {}, QStringLiteral("样图 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)")); if (fileName.isEmpty()) return;
    const QImage image(fileName); if (image.isNull()) { QMessageBox::warning(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选样图")); return; }
    qreal headerRatio = 0.125, marginRatio = 0.045; bool logoCandidate = false; const BoardTheme theme = analyzeImageTheme(image, &headerRatio, &marginRatio, &logoCandidate);
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("样图版式分析结果")); dialog.resize(620, 560); QFormLayout layout(&dialog);
    auto *preview = new QLabel(QStringLiteral("底色　　　主色　　　辅色")); preview->setAlignment(Qt::AlignCenter); preview->setMinimumHeight(54); preview->setStyleSheet(QStringLiteral("QLabel{color:%1;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:0.34 %2,stop:0.35 %1,stop:0.67 %1,stop:0.68 %3,stop:1 %3);border:1px solid #777;font-weight:bold;}").arg(theme.primary.name(), theme.background.name(), theme.accent.name()));
    auto *analysis = new QLabel(QStringLiteral("识别结果：标题区约占 %1%　边框内缩约 %2%　LOGO候选：%3").arg(qRound(headerRatio * 100)).arg(qRound(marginRatio * 100)).arg(logoCandidate ? QStringLiteral("有") : QStringLiteral("未发现"))); analysis->setWordWrap(true);
    auto *titleEdit = new QLineEdit; titleEdit->setPlaceholderText(QStringLiteral("输入新制度名称")); auto *bodyEdit = new QPlainTextEdit; bodyEdit->setPlaceholderText(QStringLiteral("正文可留空，由本地规则根据标题生成")); auto *footerEdit = new QLineEdit; footerEdit->setPlaceholderText(QStringLiteral("公司名称或落款（可留空）")); auto *sizeCombo = new QComboBox; sizeCombo->addItems(boardSizeLabels()); auto *logoBox = new QCheckBox(QStringLiteral("在检测位置保留LOGO占位框")); logoBox->setChecked(logoCandidate);
    layout.addRow(preview); layout.addRow(analysis); layout.addRow(QStringLiteral("制度名称"), titleEdit); layout.addRow(QStringLiteral("正文内容"), bodyEdit); layout.addRow(QStringLiteral("落款"), footerEdit); layout.addRow(QStringLiteral("成品尺寸"), sizeCombo); layout.addRow(logoBox);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成可编辑版式")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout.addRow(buttons); connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return; const QString title = titleEdit->text().trimmed(); if (title.isEmpty()) { QMessageBox::warning(this, QStringLiteral("缺少名称"), QStringLiteral("请输入制度名称")); return; } QString body = bodyEdit->toPlainText().trimmed(); if (body.isEmpty()) body = policyBodyForTitle(title); if (!maybeSave()) return;
    const QSizeF sizeMm = boardSizeMillimeters(sizeCombo->currentText()); const QRectF page(100, 100, sizeMm.width() * 10.0, sizeMm.height() * 10.0); m_restoring = true; m_canvas->scene()->clear(); m_canvas->setPageRect(page); m_fileName.clear(); m_history.clear(); m_historyIndex = -1; m_currentLayer = QStringLiteral("样图仿制"); m_canvas->setActiveLayer(m_currentLayer); addBoardDesign(m_canvas->scene(), page, title, body, footerEdit->text(), theme, headerRatio, marginRatio);
    if (logoBox->isChecked()) { const qreal side = qMin(page.width(), page.height()) * 0.095; const qreal gap = qMin(page.width(), page.height()) * marginRatio * 1.2; auto *logo = new QGraphicsRectItem(QRectF(page.left() + gap, page.top() + gap, side, side)); QPen pen(theme.accent.darker(130), qMax(4.0, side * 0.018), Qt::DashLine); logo->setPen(pen); logo->setBrush(QColor(255, 255, 255, 205)); prepareBoardItem(logo, QStringLiteral("rectangle"), QStringLiteral("LOGO占位框"), m_canvas->scene(), 5.0); auto *label = new QGraphicsTextItem(QStringLiteral("LOGO")); QFont font(QStringLiteral("Arial")); font.setBold(true); font.setPointSizeF(side * 0.09); label->setFont(font); label->setDefaultTextColor(theme.primary); label->setTextWidth(side); QTextOption option = label->document()->defaultTextOption(); option.setAlignment(Qt::AlignCenter); label->document()->setDefaultTextOption(option); label->setPos(logo->rect().left(), logo->rect().center().y() - label->boundingRect().height() / 2.0); prepareBoardItem(label, QStringLiteral("text"), QStringLiteral("LOGO占位文字"), m_canvas->scene(), 6.0); }
    m_restoring = false; m_modified = true; recordHistory(QStringLiteral("样图版式分析生成")); m_canvas->zoomToFit(); updateObjectList(); updateWindowTitle(); setStatus(QStringLiteral("样图版式已分析并生成可编辑对象"));
}

void MainWindow::saveCurrentAsTemplate()
{
    int roots = 0; for (QGraphicsItem *item : m_canvas->scene()->items()) if (!item->parentItem() && !item->data(KindRole).toString().isEmpty()) ++roots; if (roots == 0) { setStatus(QStringLiteral("当前设计为空，无法保存模板")); return; }
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("保存到本地模板库")); QFormLayout layout(&dialog); auto *nameEdit = new QLineEdit(QFileInfo(m_fileName).completeBaseName()); auto *categoryCombo = new QComboBox; categoryCombo->setEditable(true); categoryCombo->addItems({QStringLiteral("自定义"), QStringLiteral("安全生产"), QStringLiteral("食品"), QStringLiteral("医疗"), QStringLiteral("教育"), QStringLiteral("物业"), QStringLiteral("仓库物流"), QStringLiteral("公司管理"), QStringLiteral("通用")}); auto *favorite = new QCheckBox(QStringLiteral("保存后加入收藏")); layout.addRow(QStringLiteral("模板名称"), nameEdit); layout.addRow(QStringLiteral("行业分类"), categoryCombo); layout.addRow(favorite); auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存模板")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout.addRow(buttons); connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return; const QString name = nameEdit->text().trimmed(); if (name.isEmpty()) return; const QString path = QDir(templateDirectoryPath()).filePath(safeFileStem(name) + QStringLiteral(".jxvt")); if (QFileInfo::exists(path) && QMessageBox::question(this, QStringLiteral("覆盖模板"), QStringLiteral("同名模板已存在，是否覆盖？")) != QMessageBox::Yes) return;
    QJsonObject root {{"format", "JiangxinBoardTemplate"}, {"name", name}, {"category", categoryCombo->currentText().trimmed().isEmpty() ? QStringLiteral("自定义") : categoryCombo->currentText().trimmed()}, {"favorite", favorite->isChecked()}, {"builtin", false}, {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"document", DocumentIO::serializeDocument(m_canvas->scene(), m_canvas->pageRect())}}; QString error;
    if (!DocumentIO::saveFile(path, root, &error)) QMessageBox::critical(this, QStringLiteral("保存失败"), error); else setStatus(QStringLiteral("已保存到本地模板库：") + name);
}

void MainWindow::openTemplateLibrary()
{
    ensureBuiltinTemplates(); QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("本地制度展板模板库")); dialog.resize(760, 560); QVBoxLayout layout(&dialog); auto *filters = new QHBoxLayout; auto *search = new QLineEdit; search->setPlaceholderText(QStringLiteral("搜索模板名称…")); auto *category = new QComboBox; category->addItems({QStringLiteral("全部分类"), QStringLiteral("政务"), QStringLiteral("安全生产"), QStringLiteral("食品"), QStringLiteral("制造车间"), QStringLiteral("医疗"), QStringLiteral("教育"), QStringLiteral("物业"), QStringLiteral("仓库物流"), QStringLiteral("公司管理"), QStringLiteral("通用"), QStringLiteral("自定义")}); filters->addWidget(search, 1); filters->addWidget(category); layout.addLayout(filters); auto *list = new QListWidget; list->setAlternatingRowColors(true); layout.addWidget(list, 1); auto *actions = new QHBoxLayout; auto *applyButton = new QPushButton(QStringLiteral("应用模板")); auto *favoriteButton = new QPushButton(QStringLiteral("收藏/取消收藏")); auto *deleteButton = new QPushButton(QStringLiteral("删除自定义模板")); auto *closeButton = new QPushButton(QStringLiteral("关闭")); actions->addWidget(applyButton); actions->addWidget(favoriteButton); actions->addWidget(deleteButton); actions->addStretch(); actions->addWidget(closeButton); layout.addLayout(actions);
    auto reload = [&] { list->clear(); QDir dir(templateDirectoryPath()); const QString query = search->text().trimmed(); const QString wanted = category->currentText(); for (const QString &file : dir.entryList({QStringLiteral("*.jxvt")}, QDir::Files, QDir::Name)) { QString error; const QString path = dir.filePath(file); const QJsonObject root = DocumentIO::loadFile(path, &error); if (root["format"].toString() != QStringLiteral("JiangxinBoardTemplate")) continue; const QString name = root["name"].toString(); const QString group = root["category"].toString(QStringLiteral("自定义")); if (!query.isEmpty() && !name.contains(query, Qt::CaseInsensitive)) continue; if (wanted != QStringLiteral("全部分类") && wanted != group) continue; auto *row = new QListWidgetItem(QStringLiteral("%1  %2　｜　%3%4").arg(root["favorite"].toBool() ? QStringLiteral("★") : QStringLiteral("☆"), name, group, root["builtin"].toBool() ? QStringLiteral("　内置") : QStringLiteral("　自定义"))); row->setData(Qt::UserRole, path); list->addItem(row); } if (list->count() > 0) list->setCurrentRow(0); };
    connect(search, &QLineEdit::textChanged, &dialog, [&](const QString &) { reload(); }); connect(category, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { reload(); }); connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(favoriteButton, &QPushButton::clicked, &dialog, [&] { if (!list->currentItem()) return; const QString path = list->currentItem()->data(Qt::UserRole).toString(); QString error; QJsonObject root = DocumentIO::loadFile(path, &error); root["favorite"] = !root["favorite"].toBool(); DocumentIO::saveFile(path, root, &error); reload(); });
    connect(deleteButton, &QPushButton::clicked, &dialog, [&] { if (!list->currentItem()) return; const QString path = list->currentItem()->data(Qt::UserRole).toString(); QString error; const QJsonObject root = DocumentIO::loadFile(path, &error); if (root["builtin"].toBool()) { QMessageBox::information(&dialog, QStringLiteral("内置模板"), QStringLiteral("内置模板不能删除，可以取消收藏。")); return; } if (QMessageBox::question(&dialog, QStringLiteral("删除模板"), QStringLiteral("确定删除“%1”？").arg(root["name"].toString())) == QMessageBox::Yes) { QFile::remove(path); reload(); } });
    connect(applyButton, &QPushButton::clicked, &dialog, [&] { if (!list->currentItem()) return; const QString path = list->currentItem()->data(Qt::UserRole).toString(); QString error; const QJsonObject root = DocumentIO::loadFile(path, &error); const QJsonObject document = root["document"].toObject(); if (document.isEmpty()) return; if (!maybeSave()) return; QRectF page; m_restoring = true; if (!DocumentIO::restoreDocument(m_canvas->scene(), document, &page, &error)) { m_restoring = false; QMessageBox::critical(&dialog, QStringLiteral("应用失败"), error); return; } m_canvas->setPageRect(page); m_fileName.clear(); m_history.clear(); m_historyIndex = -1; m_currentLayer = QStringLiteral("图层 1"); m_canvas->setActiveLayer(m_currentLayer); m_restoring = false; m_modified = true; recordHistory(QStringLiteral("应用模板")); m_canvas->zoomToFit(); applyLayerState(); updateWindowTitle(); setStatus(QStringLiteral("已应用模板：") + root["name"].toString()); dialog.accept(); });
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem *) { applyButton->click(); }); reload(); dialog.exec();
}

void MainWindow::importSvg()
{
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("导入SVG矢量图"), {}, QStringLiteral("SVG矢量图 (*.svg)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, QStringLiteral("导入失败"), file.errorString()); return; }
    const QByteArray data = file.readAll();
    auto *svg = new QGraphicsSvgItem; auto *renderer = new QSvgRenderer(data, svg);
    if (!renderer->isValid()) { delete svg; QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("SVG文件无效或包含不支持的内容")); return; }
    svg->setSharedRenderer(renderer); svg->setData(SvgDataRole, data);
    svg->setData(KindRole, QStringLiteral("svg")); svg->setData(NameRole, QFileInfo(fileName).completeBaseName()); svg->setData(LayerRole, m_currentLayer); svg->setData(VisibleRole, true);
    svg->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    const QRectF bounds = svg->boundingRect(); const QRectF page = m_canvas->pageRect();
    if (bounds.width() > page.width() || bounds.height() > page.height()) {
        const qreal scale = qMin(page.width() / qMax(1.0, bounds.width()), page.height() / qMax(1.0, bounds.height())) * 0.9; svg->setScale(scale);
    }
    m_canvas->scene()->clearSelection(); m_canvas->scene()->addItem(svg);
    const QRectF sceneBounds = svg->sceneBoundingRect(); svg->setPos(page.center() - sceneBounds.center()); svg->setSelected(true);
    markModified(QStringLiteral("SVG矢量图已导入并保留矢量渲染"));
}

void MainWindow::renderForExport(QPainter *painter, const QRectF &target)
{
    const auto selected = m_canvas->scene()->selectedItems(); m_canvas->scene()->clearSelection();
    m_canvas->scene()->render(painter, target, m_canvas->pageRect(), Qt::IgnoreAspectRatio);
    for (QGraphicsItem *item : selected) item->setSelected(true);
}

void MainWindow::renderPrintOutput(QPainter *painter, const QRectF &target)
{
    const auto selected = m_canvas->scene()->selectedItems(); m_canvas->scene()->clearSelection();
    const qreal bleed = m_bleedMm * 10.0; const QRectF source = m_canvas->pageRect().adjusted(-bleed, -bleed, bleed, bleed);
    m_canvas->scene()->render(painter, target, source, Qt::IgnoreAspectRatio);
    if (m_cropMarks) drawCropMarks(painter, target, source);
    for (QGraphicsItem *item : selected) item->setSelected(true);
}

void MainWindow::drawCropMarks(QPainter *painter, const QRectF &target, const QRectF &source)
{
    const QRectF page = m_canvas->pageRect();
    const auto mapX = [&](qreal x) { return target.left() + (x - source.left()) * target.width() / source.width(); };
    const auto mapY = [&](qreal y) { return target.top() + (y - source.top()) * target.height() / source.height(); };
    const qreal left = mapX(page.left()), right = mapX(page.right()), top = mapY(page.top()), bottom = mapY(page.bottom());
    const qreal mark = qMax(8.0, qMin((left - target.left()) * 0.72, (top - target.top()) * 0.72));
    painter->save(); QPen pen(Qt::black); pen.setWidthF(0.6); painter->setPen(pen);
    painter->drawLine(QPointF(left - mark, top), QPointF(left - 2.0, top)); painter->drawLine(QPointF(left, top - mark), QPointF(left, top - 2.0));
    painter->drawLine(QPointF(right + 2.0, top), QPointF(right + mark, top)); painter->drawLine(QPointF(right, top - mark), QPointF(right, top - 2.0));
    painter->drawLine(QPointF(left - mark, bottom), QPointF(left - 2.0, bottom)); painter->drawLine(QPointF(left, bottom + 2.0), QPointF(left, bottom + mark));
    painter->drawLine(QPointF(right + 2.0, bottom), QPointF(right + mark, bottom)); painter->drawLine(QPointF(right, bottom + 2.0), QPointF(right, bottom + mark));
    painter->restore();
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
    exportImage(QStringLiteral("PNG"));
}

void MainWindow::exportImage(const QString &format)
{
    const QString extension = format.toLower(); const QString filter = QStringLiteral("%1高清图 (*.%2)").arg(format, extension);
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出") + format, QStringLiteral("设计.") + extension, filter); if (fileName.isEmpty()) return; if (!fileName.endsWith("." + extension, Qt::CaseInsensitive)) fileName += "." + extension;
    const qreal scale = 300.0 / 254.0; const QSize size = (m_canvas->pageRect().size() * scale).toSize(); QImage image(size, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white); image.setDotsPerMeterX(11811); image.setDotsPerMeterY(11811);
    QPainter painter(&image); painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); renderForExport(&painter, QRectF(QPointF(), size)); painter.end();
    if (!image.save(fileName, format.toLatin1().constData(), format == QStringLiteral("JPG") ? 95 : -1)) QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法写入") + format + QStringLiteral("文件")); else setStatus(QStringLiteral("300dpi ") + format + QStringLiteral("导出完成"));
}

void MainWindow::configurePrintSettings()
{
    bool ok = false; const qreal bleed = QInputDialog::getDouble(this, QStringLiteral("印前设置"), QStringLiteral("出血尺寸（毫米）"), m_bleedMm, 0.0, 20.0, 1, &ok); if (!ok) return;
    const auto answer = QMessageBox::question(this, QStringLiteral("印前设置"), QStringLiteral("印刷PDF是否添加裁切标记？"), QMessageBox::Yes | QMessageBox::No, m_cropMarks ? QMessageBox::Yes : QMessageBox::No);
    m_bleedMm = bleed; m_cropMarks = answer == QMessageBox::Yes; m_canvas->setBleed(m_bleedMm * 10.0); m_canvas->zoomToFit();
    setStatus(QStringLiteral("印前设置：出血 %1 mm，裁切线%2").arg(m_bleedMm).arg(m_cropMarks ? QStringLiteral("开启") : QStringLiteral("关闭")));
}

void MainWindow::preflightDocument()
{
    QStringList warnings; int objectCount = 0, bitmapCount = 0, textCount = 0;
    const QRectF safeArea = m_canvas->pageRect().adjusted(-m_bleedMm * 10.0, -m_bleedMm * 10.0, m_bleedMm * 10.0, m_bleedMm * 10.0);
    const QStringList fonts = QFontDatabase::families();
    for (QGraphicsItem *item : m_canvas->scene()->items()) {
        if (item->parentItem() || item->data(KindRole).toString().isEmpty()) continue; ++objectCount;
        if (!safeArea.contains(item->sceneBoundingRect())) warnings.append(QStringLiteral("• 对象“%1”超出页面与出血范围").arg(item->data(NameRole).toString()));
        if (item->opacity() < 1.0) warnings.append(QStringLiteral("• 对象“%1”使用透明度，印刷前请检查叠印效果").arg(item->data(NameRole).toString()));
        if (auto *text = dynamic_cast<QGraphicsTextItem *>(item)) {
            ++textCount; if (text->toPlainText().trimmed().isEmpty()) warnings.append(QStringLiteral("• 存在空文字对象"));
            if (!fonts.contains(text->font().family(), Qt::CaseInsensitive)) warnings.append(QStringLiteral("• 字体可能缺失：%1").arg(text->font().family()));
            const qreal height = item->data(TextBoxHeightRole).toDouble(); if (height > 0.0 && text->boundingRect().height() > height) warnings.append(QStringLiteral("• 文字“%1”存在溢出").arg(text->toPlainText().left(18)));
        }
        if (auto *bitmap = dynamic_cast<QGraphicsPixmapItem *>(item)) {
            ++bitmapCount; const qreal widthMm = item->sceneBoundingRect().width() / 10.0;
            if (widthMm > 0.01) { const qreal dpi = bitmap->pixmap().width() * 25.4 / widthMm; if (dpi < 150.0) warnings.append(QStringLiteral("• 位图“%1”有效分辨率约 %2 dpi，建议不低于150dpi").arg(item->data(NameRole).toString()).arg(qRound(dpi))); }
        }
    }
    QString report = QStringLiteral("对象：%1　文字：%2　位图：%3\n页面：%4 × %5 mm　出血：%6 mm\n\n")
                         .arg(objectCount).arg(textCount).arg(bitmapCount).arg(m_canvas->pageRect().width() / 10.0).arg(m_canvas->pageRect().height() / 10.0).arg(m_bleedMm);
    if (warnings.isEmpty()) report += QStringLiteral("预检通过：未发现常见版面、字体、溢出或低分辨率问题。\n\n当前输出使用RGB工作区；正式CMYK分色仍需交由CorelDRAW或专业印前软件复核。");
    else report += QStringLiteral("发现 %1 项需要检查：\n%2\n\n当前输出使用RGB工作区；正式CMYK分色仍需专业印前软件复核。").arg(warnings.size()).arg(warnings.join('\n'));
    QMessageBox message(warnings.isEmpty() ? QMessageBox::Information : QMessageBox::Warning, QStringLiteral("印前预检"), report, QMessageBox::Ok, this); message.setTextInteractionFlags(Qt::TextSelectableByMouse); message.exec();
}

void MainWindow::exportPrintPdf()
{
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出印刷PDF"), QStringLiteral("设计-印刷.pdf"), QStringLiteral("PDF印刷文件 (*.pdf)")); if (fileName.isEmpty()) return; if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) fileName += ".pdf";
    const QSizeF pageMm(m_canvas->pageRect().width() / 10.0 + m_bleedMm * 2.0, m_canvas->pageRect().height() / 10.0 + m_bleedMm * 2.0);
    QPdfWriter writer(fileName); writer.setResolution(300); writer.setPageSize(QPageSize(pageMm, QPageSize::Millimeter, QStringLiteral("含出血印刷页面"), QPageSize::ExactMatch)); writer.setPageMargins(QMarginsF(), QPageLayout::Millimeter);
    QPainter painter(&writer); painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); renderPrintOutput(&painter, QRectF(0, 0, writer.width(), writer.height())); painter.end();
    setStatus(QStringLiteral("300dpi印刷PDF导出完成（出血 %1 mm）").arg(m_bleedMm));
}

void MainWindow::batchExport()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择需要批量导出的JXV文件（最多20个）"), {}, QStringLiteral("匠心矢量文档 (*.jxv)"));
    if (files.isEmpty()) return;
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出文件夹")); if (directory.isEmpty()) return;
    bool ok = false; const QString format = QInputDialog::getItem(this, QStringLiteral("批量导出"), QStringLiteral("输出格式"), {QStringLiteral("PDF"), QStringLiteral("SVG"), QStringLiteral("PNG"), QStringLiteral("JPG"), QStringLiteral("TIFF")}, 0, false, &ok); if (!ok) return;
    const int count = qMin(20, files.size()); int succeeded = 0; QStringList errors;
    QProgressDialog progress(QStringLiteral("正在批量导出…"), QStringLiteral("取消"), 0, count, this); progress.setWindowModality(Qt::WindowModal);
    for (int index = 0; index < count; ++index) {
        progress.setValue(index); progress.setLabelText(QFileInfo(files[index]).fileName()); if (progress.wasCanceled()) break;
        QString error; const QJsonObject document = DocumentIO::loadFile(files[index], &error); QGraphicsScene scene; QRectF page;
        if (document.isEmpty() || !DocumentIO::restoreDocument(&scene, document, &page, &error)) { errors.append(QFileInfo(files[index]).fileName() + QStringLiteral("：") + error); continue; }
        const QString base = QDir(directory).filePath(QFileInfo(files[index]).completeBaseName()); bool saved = true;
        if (format == QStringLiteral("SVG")) {
            QSvgGenerator generator; generator.setFileName(base + QStringLiteral(".svg")); generator.setSize(page.size().toSize()); generator.setViewBox(QRect(QPoint(), page.size().toSize())); generator.setTitle(QStringLiteral("匠心矢量设计批量导出"));
            QPainter painter(&generator); scene.render(&painter, QRectF(QPointF(), page.size()), page, Qt::IgnoreAspectRatio); painter.end();
        } else if (format == QStringLiteral("PDF")) {
            QPdfWriter writer(base + QStringLiteral(".pdf")); writer.setResolution(300); writer.setPageSize(QPageSize(page.size() / 10.0, QPageSize::Millimeter, QStringLiteral("自定义页面"), QPageSize::ExactMatch)); writer.setPageMargins(QMarginsF(), QPageLayout::Millimeter);
            QPainter painter(&writer); scene.render(&painter, QRectF(0, 0, writer.width(), writer.height()), page, Qt::IgnoreAspectRatio); painter.end();
        } else {
            const qreal scale = 300.0 / 254.0; const QSize size = (page.size() * scale).toSize(); QImage image(size, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white); image.setDotsPerMeterX(11811); image.setDotsPerMeterY(11811);
            QPainter painter(&image); painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform); scene.render(&painter, QRectF(QPointF(), size), page, Qt::IgnoreAspectRatio); painter.end();
            saved = image.save(base + QStringLiteral(".") + format.toLower(), format.toLatin1().constData(), format == QStringLiteral("JPG") ? 95 : -1);
        }
        if (saved) ++succeeded; else errors.append(QFileInfo(files[index]).fileName() + QStringLiteral("：写入失败"));
    }
    progress.setValue(count);
    QString result = QStringLiteral("批量导出完成：%1/%2 个文件已输出到\n%3").arg(succeeded).arg(count).arg(QDir::toNativeSeparators(directory));
    if (!errors.isEmpty()) result += QStringLiteral("\n\n失败项目：\n") + errors.join('\n');
    QMessageBox::information(this, QStringLiteral("批量导出"), result); setStatus(QStringLiteral("批量导出完成：%1个文件").arg(succeeded));
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

void MainWindow::addVectorShadow()
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems())
        if (!item->parentItem() && item->data(KindRole).toString() != QStringLiteral("bitmap")) items.append(item);
    if (items.isEmpty()) { setStatus(QStringLiteral("请先选择一个矢量或文字对象")); return; }
    bool ok = false;
    const qreal offset = QInputDialog::getDouble(this, QStringLiteral("矢量阴影"), QStringLiteral("偏移距离"), 12.0, -500.0, 500.0, 1, &ok);
    if (!ok) return;
    for (QGraphicsItem *item : items) {
        auto *shadow = new QGraphicsPathItem(sceneItemPath(item).translated(offset, offset));
        shadow->setBrush(QColor(0, 0, 0, 105)); shadow->setPen(Qt::NoPen); shadow->setZValue(item->zValue() - 0.5);
        shadow->setData(KindRole, QStringLiteral("path")); shadow->setData(NameRole, QStringLiteral("矢量阴影"));
        shadow->setData(LayerRole, item->data(LayerRole).toString().isEmpty() ? m_currentLayer : item->data(LayerRole)); shadow->setData(VisibleRole, true);
        shadow->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
        m_canvas->scene()->addItem(shadow);
    }
    markModified(QStringLiteral("已生成可编辑矢量阴影"));
}

void MainWindow::createContour()
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems())
        if (!item->parentItem() && item->data(KindRole).toString() != QStringLiteral("bitmap")) items.append(item);
    if (items.isEmpty()) { setStatus(QStringLiteral("请先选择一个矢量或文字对象")); return; }
    bool ok = false;
    const qreal distance = QInputDialog::getDouble(this, QStringLiteral("轮廓图"), QStringLiteral("每层轮廓距离"), 6.0, 0.5, 200.0, 1, &ok);
    if (!ok) return;
    const int steps = QInputDialog::getInt(this, QStringLiteral("轮廓图"), QStringLiteral("轮廓层数"), 3, 1, 20, 1, &ok);
    if (!ok) return;
    for (QGraphicsItem *item : items) {
        const QPainterPath base = sceneItemPath(item);
        for (int step = steps; step >= 1; --step) {
            QPainterPathStroker stroker; stroker.setWidth(distance * step * 2.0); stroker.setJoinStyle(Qt::RoundJoin);
            auto *contour = new QGraphicsPathItem(stroker.createStroke(base).united(base));
            QColor color = m_secondFillColor; color.setAlpha(70 + (steps - step) * 120 / qMax(1, steps));
            contour->setBrush(color); contour->setPen(Qt::NoPen); contour->setZValue(item->zValue() - 0.1 * step);
            contour->setData(KindRole, QStringLiteral("path")); contour->setData(NameRole, QStringLiteral("轮廓图 %1").arg(step));
            contour->setData(LayerRole, item->data(LayerRole).toString().isEmpty() ? m_currentLayer : item->data(LayerRole)); contour->setData(VisibleRole, true);
            contour->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
            m_canvas->scene()->addItem(contour);
        }
    }
    markModified(QStringLiteral("已生成 %1 层可编辑轮廓图").arg(steps));
}

void MainWindow::createBlend()
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems())
        if (!item->parentItem() && item->data(KindRole).toString() != QStringLiteral("bitmap")) items.append(item);
    if (items.size() != 2) { setStatus(QStringLiteral("对象调和需要恰好选择两个矢量或文字对象")); return; }
    std::sort(items.begin(), items.end(), [](QGraphicsItem *a, QGraphicsItem *b) { return a->zValue() < b->zValue(); });
    bool ok = false;
    const int count = QInputDialog::getInt(this, QStringLiteral("对象调和"), QStringLiteral("中间对象数量"), 5, 1, 50, 1, &ok);
    if (!ok) return;
    const QPainterPath sourcePath = sceneItemPath(items[0]);
    const QRectF sourceBounds = sourcePath.boundingRect(); const QRectF targetBounds = sceneItemPath(items[1]).boundingRect();
    if (sourceBounds.width() < 0.01 || sourceBounds.height() < 0.01) { setStatus(QStringLiteral("源对象尺寸过小，无法调和")); return; }
    const QColor firstColor = itemBrush(items[0]).color(); const QColor lastColor = itemBrush(items[1]).color();
    for (int index = 1; index <= count; ++index) {
        const qreal t = static_cast<qreal>(index) / (count + 1.0);
        const QRectF bounds(sourceBounds.x() + (targetBounds.x() - sourceBounds.x()) * t,
                            sourceBounds.y() + (targetBounds.y() - sourceBounds.y()) * t,
                            sourceBounds.width() + (targetBounds.width() - sourceBounds.width()) * t,
                            sourceBounds.height() + (targetBounds.height() - sourceBounds.height()) * t);
        QTransform transform; transform.translate(bounds.x(), bounds.y()); transform.scale(bounds.width() / sourceBounds.width(), bounds.height() / sourceBounds.height()); transform.translate(-sourceBounds.x(), -sourceBounds.y());
        QColor color(firstColor.red() + qRound((lastColor.red() - firstColor.red()) * t),
                     firstColor.green() + qRound((lastColor.green() - firstColor.green()) * t),
                     firstColor.blue() + qRound((lastColor.blue() - firstColor.blue()) * t),
                     firstColor.alpha() + qRound((lastColor.alpha() - firstColor.alpha()) * t));
        auto *blend = new QGraphicsPathItem(transform.map(sourcePath)); blend->setBrush(color); blend->setPen(itemPen(items[0]));
        blend->setZValue(items[0]->zValue() + (items[1]->zValue() - items[0]->zValue()) * t);
        blend->setData(KindRole, QStringLiteral("path")); blend->setData(NameRole, QStringLiteral("调和对象 %1").arg(index));
        blend->setData(LayerRole, items[0]->data(LayerRole).toString().isEmpty() ? m_currentLayer : items[0]->data(LayerRole)); blend->setData(VisibleRole, true);
        blend->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
        m_canvas->scene()->addItem(blend);
    }
    markModified(QStringLiteral("已生成 %1 个调和对象").arg(count));
}

void MainWindow::applyEnvelope(int preset)
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems())
        if (!item->parentItem() && item->data(KindRole).toString() != QStringLiteral("bitmap")) items.append(item);
    if (items.isEmpty()) { setStatus(QStringLiteral("请先选择一个矢量或文字对象")); return; }
    m_canvas->scene()->clearSelection();
    constexpr qreal Pi = 3.14159265358979323846;
    for (QGraphicsItem *item : items) {
        QPainterPath path = sceneItemPath(item); const QRectF bounds = path.boundingRect();
        if (bounds.width() < 0.01 || bounds.height() < 0.01) { item->setSelected(true); continue; }
        for (int index = 0; index < path.elementCount(); ++index) {
            const auto element = path.elementAt(index); const qreal u = (element.x - bounds.left()) / bounds.width(); const qreal v = (element.y - bounds.top()) / bounds.height();
            qreal x = element.x; qreal y = element.y;
            if (preset == 0 || preset == 1) {
                const qreal inset = (preset == 0 ? 1.0 - v : v) * 0.18;
                x = bounds.left() + (inset + u * (1.0 - 2.0 * inset)) * bounds.width();
            } else if (preset == 2) {
                y -= std::sin(Pi * u) * bounds.height() * 0.14;
            } else {
                y += std::sin(2.0 * Pi * u) * bounds.height() * 0.08;
            }
            path.setElementPositionAt(index, x, y);
        }
        const QBrush brush = itemBrush(item); const QPen pen = itemPen(item); const qreal z = item->zValue(); const qreal opacity = item->opacity();
        const QString layer = item->data(LayerRole).toString().isEmpty() ? m_currentLayer : item->data(LayerRole).toString();
        m_canvas->scene()->removeItem(item); delete item;
        auto *envelope = new QGraphicsPathItem(path); envelope->setBrush(brush); envelope->setPen(pen); envelope->setZValue(z); envelope->setOpacity(opacity);
        envelope->setData(KindRole, QStringLiteral("path")); envelope->setData(NameRole, QStringLiteral("封套变形")); envelope->setData(LayerRole, layer); envelope->setData(VisibleRole, true);
        envelope->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
        m_canvas->scene()->addItem(envelope); envelope->setSelected(true);
    }
    markModified(QStringLiteral("封套预设已应用，对象已转换为曲线"));
}

void MainWindow::importBitmap()
{
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("导入图片"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (fileName.isEmpty()) return;
    QPixmap pixmap(fileName); if (pixmap.isNull()) { QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法读取所选图片")); return; }
    if (pixmap.width() > 1600 || pixmap.height() > 1600) pixmap = pixmap.scaled(1600, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    auto *bitmap = new QGraphicsPixmapItem(pixmap); bitmap->setPos(m_canvas->pageRect().center() - QPointF(pixmap.width() / 2.0, pixmap.height() / 2.0));
    bitmap->setData(KindRole, QStringLiteral("bitmap")); bitmap->setData(NameRole, QFileInfo(fileName).completeBaseName()); bitmap->setData(LayerRole, m_currentLayer); bitmap->setData(VisibleRole, true);
    bitmap->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    m_canvas->scene()->clearSelection(); m_canvas->scene()->addItem(bitmap); bitmap->setSelected(true); markModified(QStringLiteral("图片已导入"));
}

void MainWindow::adjustBitmap(int operation)
{
    bool changed = false;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) {
        auto *bitmap = dynamic_cast<QGraphicsPixmapItem *>(item); if (!bitmap) continue;
        QImage image = bitmap->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y) {
            auto *pixels = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const int alpha = qAlpha(pixels[x]); const int gray = qGray(pixels[x]);
                if (operation == 0) pixels[x] = qRgba(gray, gray, gray, alpha);
                else if (operation == 1) { const int value = gray < 128 ? 0 : 255; pixels[x] = qRgba(value, value, value, alpha); }
                else pixels[x] = qRgba(qMin(255, qRed(pixels[x]) + 30), qMin(255, qGreen(pixels[x]) + 30), qMin(255, qBlue(pixels[x]) + 30), alpha);
            }
        }
        bitmap->setPixmap(QPixmap::fromImage(image)); changed = true;
    }
    if (changed) markModified(operation == 0 ? QStringLiteral("位图已转换为灰度") : operation == 1 ? QStringLiteral("位图黑白阈值已应用") : QStringLiteral("位图已提亮"));
    else setStatus(QStringLiteral("请先选择一个位图对象"));
}

void MainWindow::traceBitmap()
{
    QGraphicsPixmapItem *bitmap = nullptr;
    for (QGraphicsItem *item : m_canvas->scene()->selectedItems()) if ((bitmap = dynamic_cast<QGraphicsPixmapItem *>(item))) break;
    if (!bitmap) { setStatus(QStringLiteral("请先选择一个位图对象")); return; }
    QImage image = bitmap->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    image = image.scaled(360, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainterPath runs;
    for (int y = 0; y < image.height(); ++y) {
        const auto *pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y)); int start = -1;
        for (int x = 0; x <= image.width(); ++x) {
            const bool dark = x < image.width() && qAlpha(pixels[x]) > 24 && qGray(pixels[x]) < 150;
            if (dark && start < 0) start = x;
            if (!dark && start >= 0) { runs.addRect(QRectF(start, y, x - start, 1)); start = -1; }
        }
    }
    if (runs.isEmpty()) { setStatus(QStringLiteral("未检测到可描摹的深色区域")); return; }
    QTransform scale; scale.scale(bitmap->pixmap().width() / static_cast<qreal>(image.width()), bitmap->pixmap().height() / static_cast<qreal>(image.height()));
    const QPainterPath traced = bitmap->sceneTransform().map(scale.map(runs.simplified()));
    auto *vector = new QGraphicsPathItem(traced); vector->setBrush(Qt::black); vector->setPen(Qt::NoPen); vector->setZValue(bitmap->zValue() + 0.1);
    vector->setData(KindRole, QStringLiteral("path")); vector->setData(NameRole, QStringLiteral("基础位图描摹"));
    vector->setData(LayerRole, bitmap->data(LayerRole).toString().isEmpty() ? m_currentLayer : bitmap->data(LayerRole)); vector->setData(VisibleRole, true);
    vector->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsFocusable);
    m_canvas->scene()->clearSelection(); m_canvas->scene()->addItem(vector); vector->setSelected(true);
    markModified(QStringLiteral("基础位图描摹完成：已生成可编辑路径"));
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
    const QString name = m_fileName.isEmpty() ? QStringLiteral("未命名.jxv") : QFileInfo(m_fileName).fileName(); setWindowTitle(QStringLiteral("%1%2 — 匠心矢量设计 1.6 Native").arg(m_modified ? "*" : "", name));
}

void MainWindow::setStatus(const QString &message) { if (m_statusLabel) m_statusLabel->setText(message); }

void MainWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
