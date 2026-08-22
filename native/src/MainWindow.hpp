#pragma once

#include "CanvasView.hpp"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QVector>

class QAction;
class QActionGroup;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QComboBox;
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
    void exportSvg();
    void exportPdf();
    void exportPng();
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
    void chooseFillColor();
    void chooseStrokeColor();
    void renderForExport(QPainter *painter, const QRectF &target);

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
    QToolButton *m_fillButton = nullptr;
    QToolButton *m_strokeButton = nullptr;
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
};
