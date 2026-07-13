// SPDX-License-Identifier: MPL-2.0

#ifndef ARCHIVEBROWSERDIALOG_H
#define ARCHIVEBROWSERDIALOG_H

#include <QDialog>
#include <QStringList>
#include <memory>
#include <string>

namespace bit7z {
class Bit7zLibrary;
class BitArchiveReader;
class BitInOutFormat;
}

class QLineEdit;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QAction;
class QToolButton;

class ArchiveBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    static constexpr int RoleArchiveIndex = Qt::UserRole;
    static constexpr int RoleArchivePath = Qt::UserRole + 1;
    static constexpr int RoleIsDirectory = Qt::UserRole + 2;

    explicit ArchiveBrowserDialog(QWidget* parent = nullptr);
    ~ArchiveBrowserDialog() override;

private slots:
    void onOpenArchiveClicked();
    void onAddFilesClicked();
    void onExtractClicked();
    void onOpenClicked();
    void onOpenWithClicked();
    void onCopyPathsClicked();
    void onRefreshClicked();
    void onTreeContextMenuRequested(const QPoint& pos);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onViewModeTriggered();
    void onFilterChanged(const QString& text);

private:
    enum class ViewMode
    {
        Details,
        Compact,
        Icons
    };

    void setupUi();
    void setupToolbar();
    void applyViewMode(ViewMode mode);
    void applyFilter(const QString& text);
    bool loadArchive(const QString& archivePath, const QString& password = {});
    void rebuildTree();
    void rebuildTreeRecursive(QTreeWidgetItem* parent, const QString& archivePath, int index);
    QTreeWidgetItem* ensurePathNode(const QStringList& segments);
    QString normalizeArchivePath(QString path) const;
    QStringList pathSegments(const QString& path) const;
    QString selectedArchiveItemPath() const;
    QStringList selectedArchiveItemPaths() const;
    QStringList selectedFilePathsForExtraction() const;
    QString promptPassword() const;
    QString promptDestinationDirectory(const QString& title, const QString& startDir) const;
    QString promptProgramPath() const;
    QString createScratchDirectory(const QString& hint) const;
    void extractPathsToDirectory(const QStringList& archivePaths, const QString& destinationDir, bool notify = true);
    void openArchiveItemDefault();
    void openArchiveItemWithProgram();
    void openArchiveItemWithProgram(const QString& programPath);
    void openPathInSystem(const QString& path);
    bool addFilesToArchive(const QStringList& files);
    bool archiveIsWritable() const;
    const bit7z::BitInOutFormat* formatForArchivePath(const QString& archivePath) const;
    const bit7z::Bit7zLibrary& library() const;
    void setStatusText(const QString& text);
    void updateStatusText();
    void showError(const QString& text);
    void showInfo(const QString& text);
    static QString humanReadableSize(std::uint64_t size);
    static QString toQString(const std::string& value);

    std::unique_ptr<bit7z::Bit7zLibrary> m_library;
    std::unique_ptr<bit7z::BitArchiveReader> m_reader;
    const bit7z::BitInOutFormat* m_format = nullptr;
    QString m_archivePath;
    QString m_password;
    QString m_statusText;
    QLineEdit* m_archivePathEdit = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QToolBar* m_toolbar = nullptr;
    QToolButton* m_viewButton = nullptr;
    QTreeWidget* m_tree = nullptr;
    QAction* m_detailsAction = nullptr;
    QAction* m_compactAction = nullptr;
    QAction* m_iconsAction = nullptr;
    ViewMode m_viewMode = ViewMode::Details;
};

#endif // ARCHIVEBROWSERDIALOG_H
