#include "MainWindow.h"
#include "BreadcrumbBar.h"
#include "DirTreeModel.h"
#include "DriveDialog.h"
#include "ExtensionModel.h"
#include "Format.h"
#include "Icons.h"
#include "Theme.h"
#include "LargestFilesModel.h"
#include "PercentBarDelegate.h"
#include "Roles.h"
#include "Scanner.h"
#include "TreemapWidget.h"
#include "Platform.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStatusBar>
#include <QStorageInfo>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace {
FileNode* nodeOf(const QModelIndex& idx)
{
    return idx.isValid() ? static_cast<FileNode*>(idx.data(Roles::Node).value<void*>()) : nullptr;
}

QString csvField(const QString& s)
{
    QString t = s;
    t.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + t + QLatin1Char('"');
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_threads = qBound(1, QThread::idealThreadCount(), 8);
    m_scanner = new Scanner(this);
    connect(m_scanner, &Scanner::progress, this, &MainWindow::onScanProgress);
    connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished);

    createActions();
    createWidgets();
    createMenus();
    createToolBar();
    createStatusBar();
    loadSettings();
    applyTreemapOptions();
    setAcceptDrops(true);
    updateActions();
    updateTitle();
}

MainWindow::~MainWindow()
{
    m_scanner->cancel();
    // Views must stop referencing the tree before it is destroyed.
    m_treemap->setTopRoot(nullptr);
    m_largestModel->clear();
    m_model->setRoot(nullptr);
}

void MainWindow::startup(const QStringList& args, int dialogDelayMs)
{
    QStringList paths;
    for (const QString& a : args)
        if (QFileInfo(a).isDir())
            paths << a;
    if (!m_capture.file.isEmpty() && m_capture.duringScan)
        QTimer::singleShot(1500, this, &MainWindow::doCapture);
    if (!paths.isEmpty())
        startScan(paths);
    else
        QTimer::singleShot(dialogDelayMs, this, &MainWindow::openScanDialog);
}

// ------------------------------------------------------------------------------------------------ setup

void MainWindow::createActions()
{
    m_actScan = new QAction(Icons::make(Icons::Scan), tr("&Scan Drive or Folder\u2026"), this);
    m_actScan->setShortcut(QKeySequence::Open);
    m_actScan->setToolTip(tr("Choose drives or a folder to scan (Ctrl+O)"));
    connect(m_actScan, &QAction::triggered, this, &MainWindow::openScanDialog);

    m_actRescan = new QAction(Icons::make(Icons::Rescan), tr("&Rescan All"), this);
    m_actRescan->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5));
    connect(m_actRescan, &QAction::triggered, this, &MainWindow::rescanAll);

    m_actRefreshSel = new QAction(Icons::make(Icons::Refresh), tr("Re&fresh Selected Folder"), this);
    m_actRefreshSel->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_actRefreshSel, &QAction::triggered, this, &MainWindow::refreshSelected);

    m_actStop = new QAction(Icons::make(Icons::Stop), tr("S&top Scan"), this);
    m_actStop->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_actStop, &QAction::triggered, this, &MainWindow::stopScan);

    m_actExport = new QAction(Icons::make(Icons::Export), tr("&Export to CSV\u2026"), this);
    m_actExport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportCsv);

    m_actAdmin = new QAction(Icons::make(Icons::Shield), tr("Restart as &Administrator"), this);
    m_actAdmin->setToolTip(tr("Restart elevated so protected folders can be measured"));
    connect(m_actAdmin, &QAction::triggered, this, &MainWindow::restartAsAdmin);

    m_actExit = new QAction(Icons::make(Icons::Exit), tr("E&xit"), this);
    m_actExit->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);

    m_actOpen = new QAction(Icons::make(Icons::Open), tr("&Open"), this);
    m_actOpen->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::openItem);

    m_actExplorer = new QAction(Icons::make(Icons::Folder), tr("Show in %1").arg(Platform::fileManagerName()), this);
    m_actExplorer->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_actExplorer, &QAction::triggered, this, &MainWindow::showInExplorer);

    m_actTerminal = new QAction(Icons::make(Icons::Terminal), tr("Open &Terminal Here"), this);
    m_actTerminal->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_actTerminal, &QAction::triggered, this, &MainWindow::openTerminal);

    m_actCopyPath = new QAction(Icons::make(Icons::Copy), tr("&Copy Path"), this);
    m_actCopyPath->setShortcut(QKeySequence::Copy);
    connect(m_actCopyPath, &QAction::triggered, this, &MainWindow::copyPath);

    m_actProperties = new QAction(Icons::make(Icons::Info), tr("P&roperties"), this);
    m_actProperties->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    connect(m_actProperties, &QAction::triggered, this, &MainWindow::showProperties);

    m_actRecycle = new QAction(Icons::make(Icons::Recycle), tr("&Delete (to %1)\u2026").arg(Platform::trashName()), this);
    m_actRecycle->setShortcut(QKeySequence::Delete);
    connect(m_actRecycle, &QAction::triggered, this, [this] { deleteSelection(true); });

    m_actDelete = new QAction(Icons::make(Icons::Delete), tr("Delete &Permanently\u2026"), this);
    m_actDelete->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    connect(m_actDelete, &QAction::triggered, this, [this] { deleteSelection(false); });

    m_actZoomIn = new QAction(Icons::make(Icons::ZoomIn), tr("Zoom &In"), this);
    m_actZoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    m_actZoomOut = new QAction(Icons::make(Icons::ZoomOut), tr("Zoom &Out"), this);
    m_actZoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    m_actZoomReset = new QAction(Icons::make(Icons::ZoomReset), tr("&Reset Zoom"), this);
    m_actZoomReset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    m_actZoomHere = new QAction(tr("&Zoom Treemap Here"), this);
    m_actZoomHere->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    connect(m_actZoomHere, &QAction::triggered, this, &MainWindow::zoomHere);

    m_actCushion = new QAction(tr("&Cushion Shading"), this);
    m_actCushion->setCheckable(true);
    m_actCushion->setChecked(true);
    m_actGrid = new QAction(tr("Tile &Gaps"), this);
    m_actGrid->setCheckable(true);
    m_actGrid->setChecked(true);
    m_actLabels = new QAction(tr("Show Folder &Labels"), this);
    m_actLabels->setCheckable(true);
    m_actLabels->setChecked(true);
    for (QAction* a : {m_actCushion, m_actGrid, m_actLabels})
        connect(a, &QAction::toggled, this, &MainWindow::applyTreemapOptions);

    m_actShowFree = new QAction(tr("Show <&Free Space> on Drives"), this);
    m_actShowFree->setCheckable(true);
    m_actShowUnknown = new QAction(tr("Show <&Unknown> Used Space on Drives"), this);
    m_actShowUnknown->setCheckable(true);
    m_actShowUnknown->setChecked(true);
    for (QAction* a : {m_actShowFree, m_actShowUnknown})
        connect(a, &QAction::toggled, this, [this] {
            if (m_tree && !m_scanner->isRunning()) {
                updateSpecialNodes();
                m_treemap->invalidate();
                refreshCrumbs();
                updateStatus();
            }
        });

    m_actShowSide = new QAction(tr("Show File &Types / Largest Files"), this);
    m_actShowSide->setCheckable(true);
    m_actShowSide->setChecked(true);
    m_actShowTreemap = new QAction(tr("Show &Treemap"), this);
    m_actShowTreemap->setCheckable(true);
    m_actShowTreemap->setChecked(true);
    m_actShowTreemap->setShortcut(QKeySequence(Qt::Key_F9));

    m_actSaveImage = new QAction(Icons::make(Icons::Export), tr("Save Treemap as &Image\u2026"), this);
    connect(m_actSaveImage, &QAction::triggered, this, &MainWindow::saveTreemapImage);

    m_actAbout = new QAction(Icons::make(Icons::Info), tr("&About DrDirectory"), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::about);

    m_threadGroup = new QActionGroup(this);
    for (int n : {1, 2, 4, 8, 16, 32}) {
        QAction* a = m_threadGroup->addAction(n == 1 ? tr("1 thread (best for spinning HDDs)") : tr("%1 threads").arg(n));
        a->setCheckable(true);
        a->setData(n);
    }
    connect(m_threadGroup, &QActionGroup::triggered, this, [this](QAction* a) { m_threads = a->data().toInt(); });

    // Make the delete/clipboard shortcuts work from every pane.
    for (QAction* a : {m_actRecycle, m_actDelete, m_actCopyPath, m_actProperties, m_actExplorer, m_actOpen,
                       m_actRefreshSel, m_actZoomHere, m_actTerminal})
        a->setShortcutContext(Qt::WindowShortcut);
}

void MainWindow::createWidgets()
{
    // Directory tree
    m_model = new DirTreeModel(this);
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(DirTreeModel::ColSize, Qt::DescendingOrder);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setItemDelegateForColumn(DirTreeModel::ColPercent, new PercentBarDelegate(m_treeView));
    m_treeView->setExpandsOnDoubleClick(true);
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->setColumnWidth(DirTreeModel::ColName, 320);
    m_treeView->setColumnWidth(DirTreeModel::ColPercent, 120);
    m_treeView->setColumnWidth(DirTreeModel::ColSize, 95);
    m_treeView->setColumnWidth(DirTreeModel::ColItems, 80);
    m_treeView->setColumnWidth(DirTreeModel::ColFiles, 80);
    m_treeView->setColumnWidth(DirTreeModel::ColFolders, 70);
    m_treeView->setColumnWidth(DirTreeModel::ColModified, 125);
    m_treeView->setColumnWidth(DirTreeModel::ColAttributes, 70);

    // File types
    m_extModel = new ExtensionModel(&m_exts, this);
    m_extProxy = new QSortFilterProxyModel(this);
    m_extProxy->setSourceModel(m_extModel);
    m_extProxy->setSortRole(Roles::Sort);
    m_extView = new QTableView(this);
    m_extView->setModel(m_extProxy);
    m_extView->setSortingEnabled(true);
    m_extView->sortByColumn(ExtensionModel::ColBytes, Qt::DescendingOrder);
    m_extView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_extView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_extView->verticalHeader()->hide();
    m_extView->verticalHeader()->setDefaultSectionSize(22);
    m_extView->setShowGrid(false);
    m_extView->setAlternatingRowColors(true);
    m_extView->setWordWrap(false);
    m_extView->setItemDelegateForColumn(ExtensionModel::ColPercent, new PercentBarDelegate(m_extView));
    m_extView->horizontalHeader()->setStretchLastSection(true);
    m_extView->setColumnWidth(ExtensionModel::ColExt, 95);
    m_extView->setColumnWidth(ExtensionModel::ColDesc, 170);
    m_extView->setColumnWidth(ExtensionModel::ColBytes, 85);
    m_extView->setColumnWidth(ExtensionModel::ColPercent, 100);
    m_extView->setToolTip(tr("Select a file type to outline all its files in the treemap"));

    // Largest files
    m_largestModel = new LargestFilesModel(this);
    m_largestProxy = new QSortFilterProxyModel(this);
    m_largestProxy->setSourceModel(m_largestModel);
    m_largestProxy->setSortRole(Roles::Sort);
    m_largestProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_largestProxy->setFilterKeyColumn(-1);
    m_largestView = new QTableView(this);
    m_largestView->setModel(m_largestProxy);
    m_largestView->setSortingEnabled(true);
    m_largestView->sortByColumn(LargestFilesModel::ColSize, Qt::DescendingOrder);
    m_largestView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_largestView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_largestView->verticalHeader()->hide();
    m_largestView->verticalHeader()->setDefaultSectionSize(22);
    m_largestView->setShowGrid(false);
    m_largestView->setAlternatingRowColors(true);
    m_largestView->setWordWrap(false);
    m_largestView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_largestView->horizontalHeader()->setStretchLastSection(true);
    m_largestView->setColumnWidth(LargestFilesModel::ColName, 220);
    m_largestView->setColumnWidth(LargestFilesModel::ColSize, 85);
    m_largestView->setColumnWidth(LargestFilesModel::ColFolder, 260);
    m_largestFilter = new QLineEdit(this);
    m_largestFilter->setPlaceholderText(tr("Filter by name or folder\u2026"));
    m_largestFilter->setClearButtonEnabled(true);
    auto largestPane = new QWidget(this);
    auto ll = new QVBoxLayout(largestPane);
    ll->setContentsMargins(0, 4, 0, 0);
    ll->setSpacing(4);
    ll->addWidget(m_largestFilter);
    ll->addWidget(m_largestView);

    m_sideTabs = new QTabWidget(this);
    m_sideTabs->addTab(m_extView, tr("File Types"));
    m_sideTabs->addTab(largestPane, tr("Largest Files"));
    m_sideTabs->setDocumentMode(true);

    // Treemap
    m_treemap = new TreemapWidget(&m_exts, this);
    m_crumbs = new BreadcrumbBar(this);
    m_treemapPane = new QWidget(this);
    auto tl = new QVBoxLayout(m_treemapPane);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    tl->addWidget(m_crumbs);
    tl->addWidget(m_treemap, 1);

    m_topSplit = new QSplitter(Qt::Horizontal, this);
    m_topSplit->addWidget(m_treeView);
    m_topSplit->addWidget(m_sideTabs);
    m_topSplit->setStretchFactor(0, 3);
    m_topSplit->setStretchFactor(1, 2);

    m_mainSplit = new QSplitter(Qt::Vertical, this);
    m_mainSplit->addWidget(m_topSplit);
    m_mainSplit->addWidget(m_treemapPane);
    m_mainSplit->setStretchFactor(0, 2);
    m_mainSplit->setStretchFactor(1, 3);
    setCentralWidget(m_mainSplit);

    // --- wiring
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& cur) {
                if (!m_syncing)
                    m_treemap->setSelected(m_model->nodeFromIndex(cur));
                updateActions();
                updateStatus();
            });
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        updateActions();
        updateStatus();
    });
    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex& idx) {
        FileNode* n = m_model->nodeFromIndex(idx);
        if (n && n->isContainer() && n->size > 0)
            m_treemap->setZoomRoot(n);
    });
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex idx = m_treeView->indexAt(pos);
        if (!idx.isValid())
            return;
        if (!m_treeView->selectionModel()->isRowSelected(idx.row(), idx.parent()))
            m_treeView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        showContextMenu(m_treeView->viewport()->mapToGlobal(pos));
    });

    connect(m_treemap, &TreemapWidget::nodeClicked, this, &MainWindow::selectNode);
    connect(m_treemap, &TreemapWidget::contextMenuRequested, this,
            [this](FileNode*, const QPoint& gp) { showContextMenu(gp); });
    connect(m_treemap, &TreemapWidget::nodeHovered, this, [this](FileNode* n) {
        if (!n) {
            m_hoverLabel->clear();
            return;
        }
        m_hoverLabel->setText(QStringLiteral("%1  \u2014  %2").arg(n->hasRealPath() ? n->fullPath() : n->name, Fmt::size(n->size)));
    });
    connect(m_treemap, &TreemapWidget::zoomChanged, this, [this](FileNode* z) {
        m_crumbs->setNodes(m_treemap->topRoot(), z);
        updateActions();
    });
    connect(m_crumbs, &BreadcrumbBar::nodeClicked, m_treemap, &TreemapWidget::setZoomRoot);
    connect(m_crumbs, &BreadcrumbBar::zoomOutClicked, m_treemap, &TreemapWidget::zoomOut);
    connect(m_actZoomIn, &QAction::triggered, m_treemap, &TreemapWidget::zoomIn);
    connect(m_actZoomOut, &QAction::triggered, m_treemap, &TreemapWidget::zoomOut);
    connect(m_actZoomReset, &QAction::triggered, m_treemap, &TreemapWidget::zoomReset);

    connect(m_extView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur) {
                m_treemap->setHighlightExtension(cur.isValid() ? cur.data(Roles::ExtId).toInt() : -1);
            });
    connect(m_extModel, &QAbstractItemModel::modelReset, this, [this] { m_treemap->setHighlightExtension(-1); });

    connect(m_largestFilter, &QLineEdit::textChanged, m_largestProxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_largestView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur) {
                if (FileNode* n = nodeOf(cur))
                    selectNode(n);
            });
    connect(m_largestView, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        if (FileNode* n = nodeOf(idx))
            Platform::showInFileManager(n->fullPath());
    });
    connect(m_largestView, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex idx = m_largestView->indexAt(pos);
        if (FileNode* n = nodeOf(idx)) {
            selectNode(n);
            showContextMenu(m_largestView->viewport()->mapToGlobal(pos));
        }
    });

    connect(m_actShowSide, &QAction::toggled, m_sideTabs, &QWidget::setVisible);
    connect(m_actShowTreemap, &QAction::toggled, m_treemapPane, &QWidget::setVisible);
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_actScan);
    file->addAction(m_actRescan);
    file->addAction(m_actRefreshSel);
    file->addAction(m_actStop);
    file->addSeparator();
    QMenu* threads = file->addMenu(tr("Scanner &Threads"));
    threads->addActions(m_threadGroup->actions());
    file->addSeparator();
    file->addAction(m_actExport);
    file->addSeparator();
    if (Platform::supportsElevation()) {
        file->addAction(m_actAdmin);
        file->addSeparator();
    }
    file->addAction(m_actExit);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_actOpen);
    edit->addAction(m_actExplorer);
    edit->addAction(m_actTerminal);
    edit->addAction(m_actCopyPath);
    edit->addSeparator();
    edit->addAction(m_actRecycle);
    edit->addAction(m_actDelete);
    edit->addSeparator();
    edit->addAction(m_actProperties);

    QMenu* tm = menuBar()->addMenu(tr("&Treemap"));
    tm->addAction(m_actZoomIn);
    tm->addAction(m_actZoomOut);
    tm->addAction(m_actZoomReset);
    tm->addAction(m_actZoomHere);
    tm->addSeparator();
    tm->addAction(m_actSaveImage);
    tm->addSeparator();
    tm->addAction(m_actCushion);
    tm->addAction(m_actGrid);
    tm->addAction(m_actLabels);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_actShowSide);
    view->addAction(m_actShowTreemap);
    view->addSeparator();
    view->addAction(m_actShowUnknown);
    view->addAction(m_actShowFree);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_actAbout);
}

void MainWindow::createToolBar()
{
    QToolBar* tb = addToolBar(tr("Main"));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(18, 18));
    tb->setMovable(false);
    auto brand = new QWidget(tb);
    auto bl = new QHBoxLayout(brand);
    bl->setContentsMargins(4, 0, 14, 0);
    bl->setSpacing(8);
    auto logo = new QLabel(brand);
    logo->setPixmap(Icons::logo(22, devicePixelRatioF()));
    auto name = new QLabel(QStringLiteral("DR\u2009DIRECTORY"), brand);
    name->setObjectName(QStringLiteral("BrandText"));
    QFont bf = Theme::displayFont(11.5, QFont::DemiBold);
    bf.setLetterSpacing(QFont::AbsoluteSpacing, 2.2);
    name->setFont(bf);
    bl->addWidget(logo);
    bl->addWidget(name);
    tb->addWidget(brand);
    tb->addSeparator();
    m_actScan->setIconText(tr("Scan"));
    m_actRescan->setIconText(tr("Rescan"));
    m_actStop->setIconText(tr("Stop"));
    m_actRecycle->setIconText(tr("Trash"));
    m_actDelete->setIconText(tr("Delete"));
    m_actExplorer->setIconText(Platform::fileManagerName());
    tb->addAction(m_actScan);
    tb->addAction(m_actRescan);
    tb->addAction(m_actStop);
    tb->addSeparator();
    tb->addAction(m_actZoomOut);
    tb->addAction(m_actZoomIn);
    tb->addAction(m_actZoomReset);
    tb->addSeparator();
    tb->addAction(m_actExplorer);
    tb->addAction(m_actRecycle);
    tb->addAction(m_actDelete);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(this);
    m_statusLabel->setMinimumWidth(200);
    m_hoverLabel = new QLabel(this);
    m_hoverLabel->setObjectName(QStringLiteral("HoverLabel"));
    m_progress = new QProgressBar(this);
    m_progress->setMaximumWidth(220);
    m_progress->setTextVisible(true);
    m_progress->hide();
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(m_hoverLabel, 2);
    statusBar()->addPermanentWidget(m_progress);
}

void MainWindow::loadSettings()
{
    QSettings s;
    if (!restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray()))
        resize(1400, 900);
    restoreState(s.value(QStringLiteral("windowState")).toByteArray());
    m_mainSplit->restoreState(s.value(QStringLiteral("mainSplit")).toByteArray());
    m_topSplit->restoreState(s.value(QStringLiteral("topSplit")).toByteArray());
    const QByteArray th = s.value(QStringLiteral("treeHeader")).toByteArray();
    if (!th.isEmpty())
        m_treeView->header()->restoreState(th);
    m_actCushion->setChecked(s.value(QStringLiteral("cushion"), true).toBool());
    m_actGrid->setChecked(s.value(QStringLiteral("tileGaps"), true).toBool());
    m_actLabels->setChecked(s.value(QStringLiteral("labels"), true).toBool());
    m_actShowFree->setChecked(s.value(QStringLiteral("showFree"), false).toBool());
    m_actShowUnknown->setChecked(s.value(QStringLiteral("showUnknown"), true).toBool());
    m_actShowSide->setChecked(s.value(QStringLiteral("showSide"), true).toBool());
    m_actShowTreemap->setChecked(s.value(QStringLiteral("showTreemap"), true).toBool());
    m_sideTabs->setVisible(m_actShowSide->isChecked());
    m_treemapPane->setVisible(m_actShowTreemap->isChecked());
    m_threads = s.value(QStringLiteral("threads"), m_threads).toInt();
    bool found = false;
    for (QAction* a : m_threadGroup->actions()) {
        if (a->data().toInt() == m_threads) {
            a->setChecked(true);
            found = true;
        }
    }
    if (!found) {
        m_threads = 8;
        m_threadGroup->actions().at(3)->setChecked(true);
    }
    m_lastFolder = s.value(QStringLiteral("lastFolder"), QDir::toNativeSeparators(QDir::homePath())).toString();
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("geometry"), saveGeometry());
    s.setValue(QStringLiteral("windowState"), saveState());
    s.setValue(QStringLiteral("mainSplit"), m_mainSplit->saveState());
    s.setValue(QStringLiteral("topSplit"), m_topSplit->saveState());
    s.setValue(QStringLiteral("treeHeader"), m_treeView->header()->saveState());
    s.setValue(QStringLiteral("cushion"), m_actCushion->isChecked());
    s.setValue(QStringLiteral("tileGaps"), m_actGrid->isChecked());
    s.setValue(QStringLiteral("labels"), m_actLabels->isChecked());
    s.setValue(QStringLiteral("showFree"), m_actShowFree->isChecked());
    s.setValue(QStringLiteral("showUnknown"), m_actShowUnknown->isChecked());
    s.setValue(QStringLiteral("showSide"), m_actShowSide->isChecked());
    s.setValue(QStringLiteral("showTreemap"), m_actShowTreemap->isChecked());
    s.setValue(QStringLiteral("threads"), m_threads);
    s.setValue(QStringLiteral("lastFolder"), m_lastFolder);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    m_scanner->cancel();
    if (m_capture.file.isEmpty()) // screenshot runs must not overwrite the user's layout
        saveSettings();
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls() && !m_scanner->isRunning())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls()) {
        const QString p = u.toLocalFile();
        if (QFileInfo(p).isDir())
            paths << QDir::toNativeSeparators(p);
    }
    if (!paths.isEmpty())
        startScan(paths);
}

void MainWindow::applyTreemapOptions()
{
    TreemapWidget::Options o = m_treemap->options();
    o.cushion = m_actCushion->isChecked();
    o.grid = m_actGrid->isChecked();
    o.labels = m_actLabels->isChecked();
    m_treemap->setOptions(o);
}

// ------------------------------------------------------------------------------------------------ scanning

void MainWindow::openScanDialog()
{
    if (m_scanner->isRunning())
        return;
    DriveDialog dlg(m_lastFolder, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    startScan(dlg.selectedPaths());
}

void MainWindow::startScan(const QStringList& pathsIn)
{
    if (m_scanner->isRunning() || pathsIn.isEmpty())
        return;
    QStringList paths;
    for (const QString& p : pathsIn) {
        const QString n = Scanner::normalizePath(p);
        if (!paths.contains(n, Qt::CaseInsensitive))
            paths << n;
    }
    m_scanPaths = paths;
    m_scanMode = ScanMode::Full;
    m_refreshTarget = nullptr;
    if (paths.size() == 1 && !Scanner::isVolumeRoot(paths.first()))
        m_lastFolder = paths.first();

    // Detach every view from the old tree before destroying it.
    m_treemap->setTopRoot(nullptr);
    m_largestModel->clear();
    m_model->setRoot(nullptr);
    m_exts.clear();
    m_extModel->refresh();
    m_tree.reset();

    m_expectedBytes = 0;
    for (const QString& p : paths) {
        if (!Scanner::isVolumeRoot(p)) {
            m_expectedBytes = 0;
            break;
        }
        QStorageInfo si(p);
        if (si.isValid())
            m_expectedBytes += si.bytesTotal() - si.bytesFree();
    }
    m_progress->setRange(0, m_expectedBytes > 0 ? 1000 : 0);
    m_progress->setValue(0);
    m_progress->show();
    m_treemap->setMessage(tr("Scanning %1\u2026").arg(paths.join(QStringLiteral(", "))));
    m_treemap->setBusy(true);
    m_statusLabel->setText(tr("Scanning\u2026"));
    m_scanner->start(paths, m_threads);
    updateActions();
    updateTitle();
}

void MainWindow::rescanAll()
{
    if (!m_scanPaths.isEmpty())
        startScan(m_scanPaths);
}

void MainWindow::refreshSelected()
{
    if (m_scanner->isRunning() || !m_tree)
        return;
    FileNode* n = currentNode();
    if (!n)
        return;
    if (!n->isContainer())
        n = n->parent;
    if (!n)
        return;
    if (n->kind == FileNode::Root || n == m_tree.get()) {
        rescanAll();
        return;
    }
    if (n->reparsePoint)
        return;
    m_refreshTarget = n;
    m_scanMode = ScanMode::Refresh;
    m_expectedBytes = 0;
    m_progress->setRange(0, 0);
    m_progress->show();
    m_statusLabel->setText(tr("Refreshing %1\u2026").arg(n->fullPath()));
    m_scanner->start({n->fullPath()}, m_threads);
    updateActions();
}

void MainWindow::stopScan()
{
    if (m_scanner->isRunning()) {
        m_scanner->cancel();
        m_statusLabel->setText(tr("Stopping\u2026"));
    }
}

void MainWindow::onScanProgress(qint64 files, qint64 dirs, qint64 bytes, const QString& path)
{
    if (m_expectedBytes > 0)
        m_progress->setValue(int(std::clamp<qint64>(bytes * 1000 / m_expectedBytes, 0, 1000)));
    const QString text = tr("Scanning\u2026 %1 files, %2 folders, %3")
                             .arg(Fmt::count(files), Fmt::count(dirs), Fmt::size(bytes));
    m_statusLabel->setText(text);
    m_hoverLabel->setText(path);
    if (m_scanMode == ScanMode::Full)
        m_treemap->setMessage(text + QStringLiteral("\n") + QFontMetrics(m_treemap->font()).elidedText(path, Qt::ElideMiddle, qMax(200, m_treemap->width() - 80)));
}

void MainWindow::onScanFinished(bool cancelled)
{
    m_progress->hide();
    m_hoverLabel->clear();
    m_treemap->setBusy(false);
    std::unique_ptr<FileNode> result = m_scanner->takeResult();
    const double secs = m_scanner->elapsedMs() / 1000.0;
    const ScanMode mode = m_scanMode;
    m_scanMode = ScanMode::None;

    if (mode == ScanMode::Full) {
        if (cancelled || !result) {
            m_treemap->setMessage(tr("Scan cancelled. Press Ctrl+O to start a new scan."));
            m_summary = tr("Scan cancelled.");
        } else {
            m_tree = std::move(result);
            m_model->setRoot(m_tree.get());
            m_model->sort(m_treeView->header()->sortIndicatorSection(), m_treeView->header()->sortIndicatorOrder());
            updateSpecialNodes();
            m_exts.rebuild(m_tree.get());
            m_extModel->refresh();
            m_largestModel->rebuild(m_tree.get());
            m_treemap->setTopRoot(m_tree.get());
            const QModelIndex rootIdx = m_model->index(0, 0);
            m_treeView->expand(rootIdx);
            m_treeView->selectionModel()->setCurrentIndex(rootIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            if (!m_capture.file.isEmpty() && !m_capture.duringScan)
                QTimer::singleShot(300, this, &MainWindow::prepareAndCapture);
            m_summary = tr("Scanned %1 files in %2 folders (%3) in %4 s")
                            .arg(Fmt::count(m_tree->fileCount), Fmt::count(m_tree->dirCount),
                                 Fmt::size(m_tree->size), QString::number(secs, 'f', 1));
        }
    } else if (mode == ScanMode::Refresh) {
        FileNode* target = m_refreshTarget;
        m_refreshTarget = nullptr;
        if (!cancelled && result && target) {
            m_treemap->prepareRemoval(target, true);
            m_largestModel->clear();
            m_model->replaceChildren(target, std::move(result));
            updateSpecialNodes();
            afterRefresh();
            selectNode(target);
            m_summary = tr("Refreshed %1 in %2 s").arg(target->fullPath(), QString::number(secs, 'f', 1));
        }
    }
    updateActions();
    updateStatus();
    updateTitle();
}

void MainWindow::refreshCrumbs()
{
    m_crumbs->setNodes(m_treemap->topRoot(), m_treemap->zoomRoot());
}

void MainWindow::afterRefresh()
{
    m_exts.rebuild(m_tree.get());
    m_extModel->refresh();
    m_largestModel->rebuild(m_tree.get());
    m_treemap->invalidate();
    refreshCrumbs();
}

void MainWindow::updateSpecialNodes()
{
    if (!m_tree)
        return;
    std::vector<FileNode*> drives;
    if (m_tree->kind == FileNode::Drive)
        drives.push_back(m_tree.get());
    else if (m_tree->kind == FileNode::Root)
        for (auto& c : m_tree->children)
            if (c->kind == FileNode::Drive)
                drives.push_back(c.get());

    for (FileNode* d : drives) {
        for (int i = int(d->children.size()) - 1; i >= 0; --i) {
            FileNode* c = d->children[size_t(i)].get();
            if (c->isSpecial()) {
                m_treemap->prepareRemoval(c, false);
                m_model->removeNode(c);
            }
        }
        QStorageInfo si(d->name);
        if (!si.isValid() || !si.isReady())
            continue;
        const qint64 used = si.bytesTotal() - si.bytesFree();
        const qint64 unknown = used - d->size;
        if (m_actShowUnknown->isChecked() && unknown > 0) {
            auto n = std::make_unique<FileNode>();
            n->kind = FileNode::Unknown;
            n->name = tr("<Unknown>");
            n->size = unknown;
            m_model->insertNode(d, std::move(n));
        }
        if (m_actShowFree->isChecked() && si.bytesFree() > 0) {
            auto n = std::make_unique<FileNode>();
            n->kind = FileNode::FreeSpace;
            n->name = tr("<Free Space>");
            n->size = si.bytesFree();
            m_model->insertNode(d, std::move(n));
        }
    }
}

// ------------------------------------------------------------------------------------------------ selection

void MainWindow::selectNode(FileNode* node)
{
    if (!node)
        return;
    const QModelIndex idx = m_model->indexForNode(node);
    if (!idx.isValid())
        return;
    for (QModelIndex p = idx.parent(); p.isValid(); p = p.parent())
        m_treeView->expand(p);
    m_syncing = true;
    m_treeView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_syncing = false;
    m_treeView->scrollTo(idx, QAbstractItemView::EnsureVisible);
    m_treemap->setSelected(node);
}

FileNode* MainWindow::currentNode() const
{
    return m_model->nodeFromIndex(m_treeView->currentIndex());
}

QList<FileNode*> MainWindow::selectedNodes() const
{
    QList<FileNode*> out;
    const QModelIndexList rows = m_treeView->selectionModel()->selectedRows(0);
    for (const QModelIndex& i : rows)
        if (FileNode* n = m_model->nodeFromIndex(i))
            out << n;
    if (out.isEmpty())
        if (FileNode* n = currentNode())
            out << n;
    return out;
}

void MainWindow::showContextMenu(const QPoint& globalPos)
{
    updateActions();
    QMenu menu(this);
    menu.addAction(m_actOpen);
    menu.addAction(m_actExplorer);
    menu.addAction(m_actTerminal);
    menu.addAction(m_actCopyPath);
    menu.addSeparator();
    menu.addAction(m_actZoomHere);
    menu.addAction(m_actRefreshSel);
    menu.addSeparator();
    menu.addAction(m_actRecycle);
    menu.addAction(m_actDelete);
    menu.addSeparator();
    menu.addAction(m_actProperties);
    menu.exec(globalPos);
}

void MainWindow::updateActions()
{
    const bool idle = !m_scanner->isRunning();
    const bool hasTree = m_tree != nullptr;
    FileNode* n = currentNode();
    const bool real = n && n->hasRealPath();
    bool anyDeletable = false;
    if (idle && hasTree)
        for (FileNode* s : selectedNodes())
            anyDeletable = anyDeletable || s->isDeletable();

    m_actScan->setEnabled(idle);
    m_actRescan->setEnabled(idle && !m_scanPaths.isEmpty());
    m_actRefreshSel->setEnabled(idle && hasTree && n && !n->isSpecial());
    m_actStop->setEnabled(!idle);
    m_actExport->setEnabled(idle && hasTree);
    m_actSaveImage->setEnabled(m_treemap->zoomRoot() != nullptr);
    m_threadGroup->setEnabled(idle);
    m_actOpen->setEnabled(real);
    m_actExplorer->setEnabled(real);
    m_actTerminal->setEnabled(real);
    m_actCopyPath->setEnabled(real);
    m_actProperties->setEnabled(real);
    m_actRecycle->setEnabled(anyDeletable);
    m_actDelete->setEnabled(anyDeletable);
    m_actZoomHere->setEnabled(n && n->isContainer() && n->size > 0 && m_treemap->topRoot());
    m_actZoomOut->setEnabled(m_treemap->canZoomOut());
    m_actZoomReset->setEnabled(m_treemap->canZoomOut());
    m_actZoomIn->setEnabled(m_treemap->zoomRoot() != nullptr);
}

void MainWindow::updateStatus()
{
    if (m_scanner->isRunning())
        return;
    QString text = m_summary;
    const QList<FileNode*> sel = selectedNodes();
    if (sel.size() == 1) {
        FileNode* n = sel.first();
        QString s = QStringLiteral("%1: %2").arg(n->name, Fmt::size(n->size));
        if (n->isContainer())
            s += tr(" (%1 files)").arg(Fmt::count(n->fileCount));
        text = text.isEmpty() ? s : text + QStringLiteral("   |   ") + s;
    } else if (sel.size() > 1) {
        qint64 total = 0;
        for (FileNode* n : sel)
            total += n->size;
        text += tr("   |   %1 items selected: %2").arg(sel.size()).arg(Fmt::size(total));
    }
    m_statusLabel->setText(text);
}

void MainWindow::updateTitle()
{
    QString t = QStringLiteral("DrDirectory");
    if (!m_scanPaths.isEmpty())
        t += QStringLiteral(" \u2014 ") + m_scanPaths.join(QStringLiteral(", "));
    setWindowTitle(t);
}

// ------------------------------------------------------------------------------------------------ actions

void MainWindow::copyPath()
{
    QStringList paths;
    for (FileNode* n : selectedNodes())
        if (n->hasRealPath())
            paths << n->fullPath();
    if (!paths.isEmpty())
        QGuiApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
}

void MainWindow::openItem()
{
    if (FileNode* n = currentNode(); n && n->hasRealPath())
        Platform::openItem(n->fullPath());
}

void MainWindow::showInExplorer()
{
    if (FileNode* n = currentNode(); n && n->hasRealPath())
        Platform::showInFileManager(n->fullPath());
}

void MainWindow::openTerminal()
{
    FileNode* n = currentNode();
    if (n && !n->isContainer())
        n = n->parent;
    if (n && n->hasRealPath())
        Platform::openTerminal(n->fullPath());
}

void MainWindow::showProperties()
{
    if (FileNode* n = currentNode(); n && n->hasRealPath())
        Platform::showProperties(this, n->fullPath());
}

void MainWindow::zoomHere()
{
    if (FileNode* n = currentNode(); n && n->isContainer())
        m_treemap->setZoomRoot(n);
}

void MainWindow::deleteSelection(bool toRecycleBin)
{
    if (m_scanner->isRunning() || !m_tree)
        return;
    QList<FileNode*> nodes;
    for (FileNode* n : selectedNodes())
        if (n->isDeletable())
            nodes << n;
    QList<FileNode*> roots; // drop items already covered by a selected ancestor
    for (FileNode* n : nodes) {
        bool nested = false;
        for (FileNode* m : nodes)
            if (m != n && m->isAncestorOf(n)) {
                nested = true;
                break;
            }
        if (!nested)
            roots << n;
    }
    if (roots.isEmpty())
        return;

    qint64 total = 0;
    for (FileNode* n : roots)
        total += n->size;
    QString list;
    for (int i = 0; i < roots.size() && i < 10; ++i)
        list += QStringLiteral("\u2022 ") + roots[i]->fullPath() + QLatin1Char('\n');
    if (roots.size() > 10)
        list += tr("\u2026and %1 more\n").arg(roots.size() - 10);

    QMessageBox box(toRecycleBin ? QMessageBox::Question : QMessageBox::Warning,
                    toRecycleBin ? tr("Move to %1").arg(Platform::trashName()) : tr("Delete Permanently"),
                    toRecycleBin ? tr("Move %n item(s) (%1) to the %2?", nullptr, int(roots.size())).arg(Fmt::size(total), Platform::trashName())
                                 : tr("Permanently delete %n item(s) (%1)?\n\nThis cannot be undone.", nullptr, int(roots.size())).arg(Fmt::size(total)),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setInformativeText(list);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    QStringList paths;
    for (FileNode* n : roots)
        paths << n->fullPath();

    QProgressDialog prog(toRecycleBin ? tr("Moving to %1\u2026").arg(Platform::trashName()) : tr("Deleting\u2026"), QString(), 0, 0, this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    prog.setCancelButton(nullptr);
    prog.setValue(0);

    QFutureWatcher<QVector<bool>> watcher;
    QEventLoop loop;
    connect(&watcher, &QFutureWatcher<QVector<bool>>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run([paths, toRecycleBin] { return Platform::deletePaths(paths, toRecycleBin); }));
    if (!watcher.isFinished())
        loop.exec();
    prog.close();
    const QVector<bool> ok = watcher.result();

    m_largestModel->clear();
    QStringList failed;
    qint64 freed = 0;
    for (int i = 0; i < roots.size(); ++i) {
        if (ok.value(i)) {
            FileNode* n = roots[i];
            freed += n->size;
            m_treemap->prepareRemoval(n, false);
            m_exts.subtract(n);
            m_model->removeNode(n);
        } else {
            failed << paths[i];
        }
    }
    updateSpecialNodes();
    m_extModel->refresh();
    m_largestModel->rebuild(m_tree.get());
    m_treemap->invalidate();
    refreshCrumbs();
    if (freed > 0)
        m_summary = toRecycleBin ? tr("Moved %1 to the %2.").arg(Fmt::size(freed), Platform::trashName())
                                 : tr("Freed %1.").arg(Fmt::size(freed));
    updateActions();
    updateStatus();

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr("Some items were not deleted"),
                             tr("%n item(s) could not be deleted (in use, access denied, or cancelled). "
                                "Use Refresh (F5) on the parent folder if part of a folder was removed.\n\n%1",
                                nullptr, int(failed.size()))
                                 .arg(failed.mid(0, 10).join(QLatin1Char('\n'))));
    }
}

void MainWindow::exportCsv()
{
    if (!m_tree)
        return;
    FileNode* base = currentNode();
    if (!base || base->isSpecial())
        base = m_tree.get();
    if (!base->isContainer())
        base = base->parent;
    const QString suggested = QDir(QDir::homePath()).filePath(QStringLiteral("DrDirectory-report.csv"));
    const QString fn = QFileDialog::getSaveFileName(this, tr("Export to CSV"), suggested, tr("CSV files (*.csv)"));
    if (fn.isEmpty())
        return;
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export"), tr("Could not write %1").arg(fn));
        return;
    }
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(true);
    ts << "Path,Type,Size (bytes),Files,Folders,Last Modified\n";
    qint64 rows = 0;
    forEachNode(base, [&](FileNode* n) {
        if (n->kind == FileNode::Root)
            return;
        const char* type = "File";
        switch (n->kind) {
        case FileNode::Dir: type = "Folder"; break;
        case FileNode::Drive: type = "Drive"; break;
        case FileNode::FreeSpace: type = "Free Space"; break;
        case FileNode::Unknown: type = "Unknown"; break;
        default: break;
        }
        ts << csvField(n->hasRealPath() ? n->fullPath() : n->name) << ',' << type << ',' << n->size << ','
           << (n->isContainer() ? n->fileCount : 0) << ',' << (n->isContainer() ? n->dirCount : 0) << ','
           << (n->lastWrite > 0 ? QDateTime::fromMSecsSinceEpoch(n->lastWrite).toString(Qt::ISODate) : QString())
           << '\n';
        ++rows;
    });
    ts.flush();
    QGuiApplication::restoreOverrideCursor();
    m_summary = tr("Exported %1 rows to %2").arg(Fmt::count(rows), QDir::toNativeSeparators(fn));
    updateStatus();
}

void MainWindow::restartAsAdmin()
{
    m_scanner->cancel();
    if (Platform::restartElevated(m_scanPaths)) {
        saveSettings();
        QTimer::singleShot(0, qApp, &QApplication::quit);
    } else {
        QMessageBox::information(this, tr("Restart as Administrator"), tr("Elevation was cancelled."));
    }
}

void MainWindow::saveTreemapImage()
{
    if (!m_treemap->zoomRoot())
        return;
    const QString fn = QFileDialog::getSaveFileName(this, tr("Save Treemap as Image"),
                                                    QDir(QDir::homePath()).filePath(QStringLiteral("treemap.png")),
                                                    tr("PNG images (*.png);;JPEG images (*.jpg)"));
    if (fn.isEmpty())
        return;
    if (!m_treemap->grab().save(fn))
        QMessageBox::warning(this, tr("Save Treemap"), tr("Could not write %1").arg(fn));
}

void MainWindow::prepareAndCapture()
{
    if (!m_capture.zoomPath.isEmpty() && m_tree) {
        const QString want = Scanner::normalizePath(m_capture.zoomPath);
        FileNode* found = nullptr;
        forEachNode(m_tree.get(), [&](FileNode* n) {
            if (!found && n->isContainer() && n->fullPath().compare(want, Qt::CaseInsensitive) == 0)
                found = n;
        });
        if (found) {
            selectNode(found);
            m_treeView->expand(m_model->indexForNode(found));
            m_treemap->setZoomRoot(found);
        }
    }
    m_sideTabs->setCurrentIndex(m_capture.tab);
    if (!m_capture.highlightExt.isEmpty()) {
        for (int r = 0; r < m_extProxy->rowCount(); ++r) {
            const QModelIndex idx = m_extProxy->index(r, 0);
            if (idx.data().toString().compare(QLatin1Char('.') + m_capture.highlightExt, Qt::CaseInsensitive) == 0) {
                m_extView->setCurrentIndex(idx);
                break;
            }
        }
    }
    QTimer::singleShot(900, this, &MainWindow::doCapture);
}

void MainWindow::doCapture()
{
    grab().save(m_capture.file);
    m_scanner->cancel();
    QTimer::singleShot(0, qApp, &QApplication::quit);
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About DrDirectory"),
                       tr("<h3 style='color:#22D3EE'>DrDirectory %2</h3>"
                          "<p>A fast disk-usage analyzer: multithreaded scanning, a cushion-shaded squarified "
                          "treemap, per-type statistics, largest-file finder and safe cleanup.</p>"
                          "<p>Built with Qt %1.</p>"
                          "<p>Created by Glen Rhodes.</p>"
                          "<p>Open source under the MIT License.<br>"
                          "<a href='https://github.com/glenwrhodes/DrDirectory'>github.com/glenwrhodes/DrDirectory</a></p>"
                          "<p>&copy; 2026 Tiny Moosh Games Inc.</p>")
                           .arg(QString::fromLatin1(qVersion()), QStringLiteral(DRD_VERSION)));
}
