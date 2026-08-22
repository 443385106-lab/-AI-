#pragma once

#include "CanvasView.hpp"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QVector>

class QAction;
class QActionGroup;
class QBrush;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QComboBox;
class QFontComboBox;
class QToolButton;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildInterface();
    void buildMenus();
    void buildToolbox();
    void buildPropertyBar();
    void buildTextAndColorBar();
    void buildColorPalette();
    void buildCentralCanvas();
    void buildDockers();
    void connectSignals();
    QAction *addToolAction(const QString &text, const QString &shortcut, CanvasView::Tool tool);
    void setCurrentTool(CanvasView::Tool tool);
    void updateObjectList();
    void updateInspector();
    void updateWindowTitle();
    void setStatus(const QString &message);
    void markModified(const QString &reason);
    void recordHistory(const QString &reason);
    void restoreHistory(int index);
    bool maybeSave();
    bool saveDocument(bool saveAs);
    void newDocument();
    void openDocument();
    void pageSetup();
    void generateSmartBoard();
    void batchGenerateBoards();
    void ocrSampleImage();
    void analyzeSampleLayout();
    void openTemplateLibrary();
    void saveCurrentAsTemplate();
    void configureBrandProfile();
    void applyBrandProfile();
    void batchApplyBrandProfile();
    void findReplaceDocumentText();
    void importSvg();
    void exportSvg();
    void exportPdf();
    void exportPng();
    void exportImage(const QString &format);
    void exportPrintPdf();
    void configurePrintSettings();
    void preflightDocument();
    void batchExport();
    void copySelection();
    void pasteSelection();
    void duplicateSelection();
    void groupSelection();
    void ungroupSelection();
    void convertSelectionToPath();
    void booleanSelection(int operation);
    void transformSelection(int operation);
    void clipSelection();
    void releaseClip();
    void addVectorShadow();
    void createContour();
    void createBlend();
    void applyEnvelope(int preset);
    void importBitmap();
    void adjustBitmap(int operation);
    void traceBitmap();
    void arrangeSelection(int direction);
    void alignSelection(Qt::Alignment alignment);
    void distributeSelection(bool horizontal);
    void updateLayerList();
    void addLayer();
    void renameLayer();
    void toggleLayerVisible();
    void toggleLayerLocked();
    void applyLayerState();
    void applyInspector();
    void editSelectedText();
    void autoFitSelectedText();
    void chooseSecondFillColor();
    QBrush selectedFillBrush(const QRectF &bounds) const;
    void applyQuickColor(const QColor &color);
    void chooseFillColor();
    void chooseStrokeColor();
    void renderForExport(QPainter *painter, const QRectF &target);
    void renderPrintOutput(QPainter *painter, const QRectF &target);
    void drawCropMarks(QPainter *painter, const QRectF &target, const QRectF &source);

    CanvasView *m_canvas = nullptr;
    QListWidget *m_objectList = nullptr;
    QListWidget *m_layerList = nullptr;
    QLabel *m_selectionLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QDoubleSpinBox *m_xSpin = nullptr;
    QDoubleSpinBox *m_ySpin = nullptr;
    QDoubleSpinBox *m_wSpin = nullptr;
    QDoubleSpinBox *m_hSpin = nullptr;
    QDoubleSpinBox *m_rotationSpin = nullptr;
    QDoubleSpinBox *m_strokeWidthSpin = nullptr;
    QDoubleSpinBox *m_fontSizeSpin = nullptr;
    QDoubleSpinBox *m_textWidthSpin = nullptr;
    QDoubleSpinBox *m_textHeightSpin = nullptr;
    QDoubleSpinBox *m_opacitySpin = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QComboBox *m_alignmentCombo = nullptr;
    QComboBox *m_fillModeCombo = nullptr;
    QComboBox *m_outlineStyleCombo = nullptr;
    QToolButton *m_boldButton = nullptr;
    QToolButton *m_italicButton = nullptr;
    QToolButton *m_fillButton = nullptr;
    QToolButton *m_strokeButton = nullptr;
    QToolButton *m_secondFillButton = nullptr;
    QActionGroup *m_toolActions = nullptr;
    QJsonArray m_clipboard;
    QVector<QJsonObject> m_history;
    int m_historyIndex = -1;
    bool m_restoring = false;
    bool m_modified = false;
    QString m_fileName;
    QString m_currentLayer = QStringLiteral("图层 1");
    QColor m_fillColor {244, 197, 66};
    QColor m_strokeColor {34, 34, 34};
    QColor m_secondFillColor {235, 75, 75};
    qreal m_bleedMm = 3.0;
    bool m_cropMarks = true;
};
