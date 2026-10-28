#pragma once
#include <QDialog>
#include <QStringList>

class QRadioButton;
class QTreeWidget;
class QLineEdit;

// WinDirStat-style "what do you want to scan" dialog.
class DriveDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DriveDialog(const QString& lastFolder, QWidget* parent = nullptr);
    QStringList selectedPaths() const;

public slots:
    void accept() override;

private:
    void populate();
    QRadioButton* m_allLocal;
    QRadioButton* m_someDrives;
    QRadioButton* m_folder;
    QTreeWidget* m_drives;
    QLineEdit* m_folderEdit;
};
