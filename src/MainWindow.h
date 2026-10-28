#pragma once
#include "Extensions.h"
#include "FileNode.h"
#include <QMainWindow>
#include <QStringList>
#include <memory>

class QAction;
class QActionGroup;
class QLabel;
class QLineEdit;
class QProgressBar;
class QSortFilterProxyModel;
class QSplitter;
class QTabWidget;
class QTableView;
class QTreeView;
class BreadcrumbBar;
class DirTreeModel;
class ExtensionModel;
class LargestFilesModel;
class Scanner;
class TreemapWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void startup(const QStringList& args);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    enum class ScanMode { None, Full, Refresh };

    void createActions();
    void createMenus();
    void createToolBar();
    void createWidgets();
    void createStatusBar();
    void loadSettings();
    void saveSettings();

    void openScanDialog();
    void startScan(const QStringList& paths);
    void rescanAll();
    void refreshSelected();
    void stopScan();
    void onScanProgress(qint64 files, qint64 dirs, qint64 bytes, const QString& path);
    void onScanFinished(bool cancelled);

    void selectNode(FileNode* node);
    FileNode* currentNode() const;
    QList<FileNode*> selectedNodes() const;
    void showContextMenu(const QPoint& globalPos);
    void deleteSelection(bool toRecycleBin);
    void updateSpecialNodes();
    void afterRefresh();
    void refreshCrumbs();

    void updateActions();
    void updateStatus();
    void updateTitle();
    void applyTreemapOptions();

    void copyPath();
    void openItem();
    void showInExplorer();
    void openTerminal();
    void showProperties();
    void zoomHere();
    void exportCsv();
    void restartAsAdmin();
    void about();

    // data
    std::unique_ptr<FileNode> m_tree;
    ExtensionTable m_exts;
    Scanner* m_scanner = nullptr;
    ScanMode m_scanMode = ScanMode::None;
    FileNode* m_refreshTarget = nullptr;
    QStringList m_scanPaths;
    QString m_lastFolder;
    qint64 m_expectedBytes = 0;
    int m_threads = 8;
    bool m_syncing = false;
    QString m_summary;

    // models / views
    DirTreeModel* m_model = nullptr;
    ExtensionModel* m_extModel = nullptr;
    QSortFilterProxyModel* m_extProxy = nullptr;
    LargestFilesModel* m_largestModel = nullptr;
    QSortFilterProxyModel* m_largestProxy = nullptr;
    QTreeView* m_treeView = nullptr;
    QTableView* m_extView = nullptr;
    QTableView* m_largestView = nullptr;
    QLineEdit* m_largestFilter = nullptr;
    QTabWidget* m_sideTabs = nullptr;
    TreemapWidget* m_treemap = nullptr;
    BreadcrumbBar* m_crumbs = nullptr;
    QWidget* m_treemapPane = nullptr;
    QSplitter* m_mainSplit = nullptr;
    QSplitter* m_topSplit = nullptr;

    // status bar
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hoverLabel = nullptr;
    QLabel* m_adminLabel = nullptr;
    QProgressBar* m_progress = nullptr;

    // actions
    QAction* m_actScan = nullptr;
    QAction* m_actRescan = nullptr;
    QAction* m_actRefreshSel = nullptr;
    QAction* m_actStop = nullptr;
    QAction* m_actExport = nullptr;
    QAction* m_actAdmin = nullptr;
    QAction* m_actExit = nullptr;
    QAction* m_actOpen = nullptr;
    QAction* m_actExplorer = nullptr;
    QAction* m_actTerminal = nullptr;
    QAction* m_actCopyPath = nullptr;
    QAction* m_actProperties = nullptr;
    QAction* m_actRecycle = nullptr;
    QAction* m_actDelete = nullptr;
    QAction* m_actZoomIn = nullptr;
    QAction* m_actZoomOut = nullptr;
    QAction* m_actZoomReset = nullptr;
    QAction* m_actZoomHere = nullptr;
    QAction* m_actCushion = nullptr;
    QAction* m_actGrid = nullptr;
    QAction* m_actLabels = nullptr;
    QAction* m_actShowFree = nullptr;
    QAction* m_actShowUnknown = nullptr;
    QAction* m_actShowSide = nullptr;
    QAction* m_actShowTreemap = nullptr;
    QAction* m_actAbout = nullptr;
    QActionGroup* m_threadGroup = nullptr;
};
