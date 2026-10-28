#include "DriveDialog.h"
#include "Format.h"
#include "Icons.h"
#include "PercentBarDelegate.h"
#include "Platform.h"
#include "Roles.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
enum { LocalRole = Qt::UserRole + 50, PathRole };
}

DriveDialog::DriveDialog(const QString& lastFolder, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("DrDirectory \u2014 Select What to Scan"));
    resize(720, 440);
    auto lay = new QVBoxLayout(this);

    m_allLocal = new QRadioButton(tr("&All local drives"), this);
    m_someDrives = new QRadioButton(tr("&Individual drives (Ctrl/Shift-click for several)"), this);
    m_folder = new QRadioButton(tr("A &folder:"), this);

    m_drives = new QTreeWidget(this);
    m_drives->setRootIsDecorated(false);
    m_drives->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_drives->setHeaderLabels({tr("Volume"), tr("Label"), tr("Total"), tr("Free"), tr("Used"), tr("File System")});
    m_drives->setItemDelegateForColumn(4, new PercentBarDelegate(m_drives));
    m_drives->header()->setStretchLastSection(false);
    m_drives->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_drives->setColumnWidth(0, 140);
    m_drives->setColumnWidth(2, 90);
    m_drives->setColumnWidth(3, 90);
    m_drives->setColumnWidth(4, 130);
    m_drives->setColumnWidth(5, 90);

    m_folderEdit = new QLineEdit(lastFolder, this);
    m_folderEdit->setPlaceholderText(QDir::toNativeSeparators(QDir::homePath()));
    auto browse = new QPushButton(tr("&Browse\u2026"), this);
    auto folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folder);
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(browse);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("&Scan"));

    lay->addWidget(m_allLocal);
    lay->addWidget(m_someDrives);
    lay->addWidget(m_drives, 1);
    lay->addLayout(folderRow);
    lay->addWidget(buttons);

    populate();
    m_someDrives->setChecked(true);
    if (m_drives->topLevelItemCount() > 0)
        m_drives->topLevelItem(0)->setSelected(true);

    connect(m_drives, &QTreeWidget::itemSelectionChanged, this, [this] { m_someDrives->setChecked(true); });
    connect(m_drives, &QTreeWidget::itemDoubleClicked, this, [this] {
        m_someDrives->setChecked(true);
        accept();
    });
    connect(m_folderEdit, &QLineEdit::textEdited, this, [this] { m_folder->setChecked(true); });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Choose a folder to scan"), m_folderEdit->text());
        if (!d.isEmpty()) {
            m_folderEdit->setText(QDir::toNativeSeparators(d));
            m_folder->setChecked(true);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &DriveDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DriveDialog::reject);
}

void DriveDialog::populate()
{
    const QIcon driveIcon = Icons::tinted(Icons::Drive, QColor(94, 234, 212));
    const auto volumes = Platform::scannableVolumes();
    for (const QStorageInfo& s : volumes) {
        const QString root = QDir::toNativeSeparators(s.rootPath());
        auto it = new QTreeWidgetItem(m_drives);
        it->setText(0, root);
        it->setIcon(0, driveIcon);
        const QString label = s.name().isEmpty() ? s.displayName() : s.name();
        it->setText(1, label == s.rootPath() ? QString() : label);
        it->setText(2, Fmt::size(s.bytesTotal()));
        it->setText(3, Fmt::size(s.bytesFree()));
        const double used = s.bytesTotal() > 0 ? double(s.bytesTotal() - s.bytesFree()) / double(s.bytesTotal()) : 0;
        it->setData(4, Roles::Fraction, used);
        it->setData(4, Roles::Depth, 0);
        it->setText(5, QString::fromLatin1(s.fileSystemType()));
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        it->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        it->setData(0, LocalRole, Platform::isLocalVolume(s));
        it->setData(0, PathRole, root);
    }
}

QStringList DriveDialog::selectedPaths() const
{
    QStringList out;
    if (m_allLocal->isChecked()) {
        for (int i = 0; i < m_drives->topLevelItemCount(); ++i) {
            auto it = m_drives->topLevelItem(i);
            if (it->data(0, LocalRole).toBool())
                out << it->data(0, PathRole).toString();
        }
    } else if (m_someDrives->isChecked()) {
        for (int i = 0; i < m_drives->topLevelItemCount(); ++i) {
            auto it = m_drives->topLevelItem(i);
            if (it->isSelected())
                out << it->data(0, PathRole).toString();
        }
    } else {
        const QString f = m_folderEdit->text().trimmed();
        if (!f.isEmpty())
            out << QDir::toNativeSeparators(f);
    }
    return out;
}

void DriveDialog::accept()
{
    const QStringList p = selectedPaths();
    if (p.isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Select at least one drive or enter a folder."));
        return;
    }
    if (m_folder->isChecked() && !QFileInfo(p.first()).isDir()) {
        QMessageBox::warning(this, windowTitle(), tr("The folder \"%1\" does not exist.").arg(p.first()));
        return;
    }
    QDialog::accept();
}
