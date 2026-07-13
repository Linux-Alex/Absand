// SPDX-License-Identifier: MPL-2.0

#include "ArchiveBrowserDialog.h"

#include "core/SevenZipRuntime.h"

#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivewriter.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitabstractarchivecreator.hpp>

#include <QClipboard>
#include <QAction>
#include <QActionGroup>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QHeaderView>
#include <QInputDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>
#include <QUuid>
#include <QPainter>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

namespace {

constexpr int ColumnName = 0;
constexpr int ColumnSize = 1;
constexpr int ColumnType = 2;
constexpr int ColumnModified = 3;
constexpr int ColumnCount = 4;

std::vector<bit7z::tstring> toBit7zPaths(const QStringList& files)
{
    std::vector<bit7z::tstring> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const QString& file : files) {
        paths.push_back(file.toStdString());
    }
    return paths;
}

bit7z::IndicesVector collectIndices(QTreeWidgetItem* item)
{
    bit7z::IndicesVector indices;
    if (!item) {
        return indices;
    }

    const int indexValue = item->data(ColumnName, ArchiveBrowserDialog::RoleArchiveIndex).toInt();
    if (indexValue >= 0) {
        indices.push_back(static_cast<std::uint32_t>(indexValue));
    }

    for (int i = 0; i < item->childCount(); ++i) {
        const auto childIndices = collectIndices(item->child(i));
        indices.insert(indices.end(), childIndices.begin(), childIndices.end());
    }

    return indices;
}

QString normalizeSeparators(QString path)
{
    path.replace('\\', '/');
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    while (path.contains("//")) {
        path.replace("//", "/");
    }
    return path;
}

QString fileTypeLabel(bool isDir, const QString& path)
{
    if (isDir) {
        return QStringLiteral("Folder");
    }

    const QFileInfo info(path);
    const QString suffix = info.suffix();
    return suffix.isEmpty() ? QStringLiteral("File") : suffix.toUpper() + QStringLiteral(" file");
}

QString formatDateTime(const std::chrono::time_point<std::chrono::system_clock>& timePoint)
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    if (ms <= 0) {
        return QString();
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QIcon themedIcon(QWidget* widget,
                 std::initializer_list<const char*> names,
                 QStyle::StandardPixmap fallback)
{
    for (const char* name : names) {
        const QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
        if (!icon.isNull()) {
            return icon;
        }
    }
    return widget->style()->standardIcon(fallback);
}

QIcon tintedIcon(QWidget* widget,
                 std::initializer_list<const char*> names,
                 QStyle::StandardPixmap fallback,
                 const QColor& color)
{
    const QIcon source = themedIcon(widget, names, fallback);
    if (source.isNull()) {
        return source;
    }

    const QSize size = QSize(22, 22);
    QPixmap pixmap = source.pixmap(size);
    if (pixmap.isNull()) {
        return source;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();

    return QIcon(QPixmap::fromImage(image));
}

} // namespace

ArchiveBrowserDialog::ArchiveBrowserDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

ArchiveBrowserDialog::~ArchiveBrowserDialog() = default;

void ArchiveBrowserDialog::setupUi()
{
    setWindowTitle(tr("Archive Browser"));
    resize(1040, 700);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    setupToolbar();
    rootLayout->addWidget(m_toolbar);

    auto* topRow = new QHBoxLayout();
    m_archivePathEdit = new QLineEdit(this);
    m_archivePathEdit->setReadOnly(true);
    m_archivePathEdit->setPlaceholderText(tr("Open an archive to browse its contents"));
    m_archivePathEdit->setClearButtonEnabled(false);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter items"));
    m_filterEdit->setClearButtonEnabled(true);

    topRow->addWidget(m_archivePathEdit, 1);
    topRow->addWidget(m_filterEdit, 0);
    rootLayout->addLayout(topRow);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColumnCount);
    m_tree->setHeaderLabels({tr("Name"), tr("Size"), tr("Type"), tr("Modified")});
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSortingEnabled(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(ColumnName, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(ColumnSize, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(ColumnType, QHeaderView::ResizeToContents);
    rootLayout->addWidget(m_tree, 1);

    m_statusLabel = new QLabel(tr("Open an archive to inspect or edit it."), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setObjectName("archiveStatusLabel");

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_summaryLabel->setObjectName("archiveSummaryLabel");

    auto* statusRow = new QHBoxLayout();
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_summaryLabel, 0);
    rootLayout->addLayout(statusRow);

    auto* footerRow = new QHBoxLayout();
    auto* closeButton = new QPushButton(tr("Close"), this);
    footerRow->addStretch(1);
    footerRow->addWidget(closeButton);
    rootLayout->addLayout(footerRow);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &ArchiveBrowserDialog::onFilterChanged);
    connect(closeButton, &QPushButton::clicked, this, &ArchiveBrowserDialog::reject);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ArchiveBrowserDialog::onTreeContextMenuRequested);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &ArchiveBrowserDialog::onItemDoubleClicked);

    setStatusText(tr("Select an archive to begin."));
    applyViewMode(ViewMode::Details);
}

void ArchiveBrowserDialog::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setIconSize(QSize(22, 22));
    m_toolbar->setContentsMargins(4, 4, 4, 4);
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { border: 1px solid palette(mid); border-radius: 10px; padding: 4px; spacing: 4px; }"
        "QToolButton { border: none; padding: 6px; border-radius: 8px; }"
        "QToolButton:hover { background: palette(alternate-base); }"
        "QToolButton:pressed { background: palette(midlight); }"
        "QToolButton::menu-indicator { image: none; }"
    ));

    auto color = palette().color(QPalette::ButtonText);
    if (!color.isValid() || color == palette().color(QPalette::Base)) {
        color = palette().color(QPalette::Text);
    }

    auto addButton = [&](const QString& tooltip,
                         std::initializer_list<const char*> names,
                         QStyle::StandardPixmap fallback,
                         const std::function<void()>& handler) {
        auto* button = new QToolButton(m_toolbar);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(22, 22));
        button->setIcon(tintedIcon(this, names, fallback, color));
        button->setToolTip(tooltip);
        connect(button, &QToolButton::clicked, this, handler);
        m_toolbar->addWidget(button);
        return button;
    };

    addButton(tr("Open archive"), {"document-open", "folder-open"}, QStyle::SP_DialogOpenButton,
              [this]() { onOpenArchiveClicked(); });
    addButton(tr("Add files"), {"list-add", "document-new"}, QStyle::SP_FileDialogNewFolder,
              [this]() { onAddFilesClicked(); });
    addButton(tr("Extract selected items"), {"archive-extract", "folder-download", "download"}, QStyle::SP_ArrowDown,
              [this]() { onExtractClicked(); });
    addButton(tr("Copy archive paths"), {"edit-copy", "copy"}, QStyle::SP_FileIcon,
              [this]() { onCopyPathsClicked(); });
    addButton(tr("Refresh archive"), {"view-refresh", "reload"}, QStyle::SP_BrowserReload,
              [this]() { onRefreshClicked(); });

    m_toolbar->addSeparator();

    m_viewButton = new QToolButton(m_toolbar);
    m_viewButton->setAutoRaise(true);
    m_viewButton->setPopupMode(QToolButton::InstantPopup);
    m_viewButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_viewButton->setIconSize(QSize(22, 22));
    m_viewButton->setText(tr("View"));
    m_viewButton->setToolTip(tr("Choose how the archive contents are displayed"));
    m_viewButton->setIcon(tintedIcon(this, {"view-list-details", "view-details"}, QStyle::SP_FileDialogDetailedView, color));

    auto* viewMenu = new QMenu(m_viewButton);
    auto* viewGroup = new QActionGroup(viewMenu);
    viewGroup->setExclusive(true);

    m_detailsAction = viewMenu->addAction(tr("Details"));
    m_detailsAction->setCheckable(true);
    m_detailsAction->setChecked(true);
    viewGroup->addAction(m_detailsAction);

    m_compactAction = viewMenu->addAction(tr("Compact"));
    m_compactAction->setCheckable(true);
    viewGroup->addAction(m_compactAction);

    m_iconsAction = viewMenu->addAction(tr("Icons"));
    m_iconsAction->setCheckable(true);
    viewGroup->addAction(m_iconsAction);

    connect(m_detailsAction, &QAction::triggered, this, &ArchiveBrowserDialog::onViewModeTriggered);
    connect(m_compactAction, &QAction::triggered, this, &ArchiveBrowserDialog::onViewModeTriggered);
    connect(m_iconsAction, &QAction::triggered, this, &ArchiveBrowserDialog::onViewModeTriggered);

    m_viewButton->setMenu(viewMenu);
    m_toolbar->addWidget(m_viewButton);
}

void ArchiveBrowserDialog::applyViewMode(ViewMode mode)
{
    m_viewMode = mode;

    const bool details = mode == ViewMode::Details;
    const bool compact = mode == ViewMode::Compact;
    const bool icons = mode == ViewMode::Icons;

    m_tree->setIconSize(icons ? QSize(32, 32) : QSize(18, 18));
    m_tree->setColumnHidden(ColumnSize, icons ? true : false);
    m_tree->setColumnHidden(ColumnType, compact || icons);
    m_tree->setColumnHidden(ColumnModified, compact || icons);
    m_tree->setHeaderHidden(icons);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(icons ? 18 : 20);

    if (details) {
        m_tree->header()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(ColumnSize, QHeaderView::ResizeToContents);
        m_tree->header()->setSectionResizeMode(ColumnType, QHeaderView::ResizeToContents);
        m_tree->header()->setSectionResizeMode(ColumnModified, QHeaderView::ResizeToContents);
    } else if (compact) {
        m_tree->header()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
    } else {
        m_tree->header()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
    }

    if (m_detailsAction) m_detailsAction->setChecked(details);
    if (m_compactAction) m_compactAction->setChecked(compact);
    if (m_iconsAction) m_iconsAction->setChecked(icons);

    if (m_viewButton) {
        const QColor color = palette().color(QPalette::ButtonText);
        const std::initializer_list<const char*> detailsNames = {"view-list-details", "view-details"};
        const std::initializer_list<const char*> compactNames = {"view-list-text", "view-list-icons"};
        const std::initializer_list<const char*> iconNames = {"view-grid", "view-icons"};
        if (details) {
            m_viewButton->setIcon(tintedIcon(this, detailsNames, QStyle::SP_FileDialogDetailedView, color));
            m_viewButton->setText(tr("Details"));
        } else if (compact) {
            m_viewButton->setIcon(tintedIcon(this, compactNames, QStyle::SP_FileDialogListView, color));
            m_viewButton->setText(tr("Compact"));
        } else {
            m_viewButton->setIcon(tintedIcon(this, iconNames, QStyle::SP_DirIcon, color));
            m_viewButton->setText(tr("Icons"));
        }
    }
}

void ArchiveBrowserDialog::applyFilter(const QString& text)
{
    const QString needle = text.trimmed();

    std::function<bool(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) -> bool {
        bool childVisible = false;
        for (int i = 0; i < item->childCount(); ++i) {
            childVisible |= walk(item->child(i));
        }

        const bool selfMatch = needle.isEmpty()
            || item->text(ColumnName).contains(needle, Qt::CaseInsensitive)
            || item->text(ColumnType).contains(needle, Qt::CaseInsensitive);
        const bool visible = selfMatch || childVisible;
        item->setHidden(!visible);
        return visible;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        walk(m_tree->topLevelItem(i));
    }
}

void ArchiveBrowserDialog::onOpenArchiveClicked()
{
    const QString archivePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Archive"),
        QString(),
        tr("Archives (*.7z *.zip);;All Files (*)")
    );

    if (archivePath.isEmpty()) {
        return;
    }

    const QString password = promptPassword();
    if (!loadArchive(archivePath, password)) {
        return;
    }
}

void ArchiveBrowserDialog::onAddFilesClicked()
{
    if (!archiveIsWritable()) {
        showError(tr("Open a ZIP or 7z archive before adding files."));
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files to Add"));
    if (files.isEmpty()) {
        return;
    }

    if (addFilesToArchive(files)) {
        loadArchive(m_archivePath, m_password);
    }
}

void ArchiveBrowserDialog::onExtractClicked()
{
    const QStringList archivePaths = selectedFilePathsForExtraction();
    if (archivePaths.isEmpty()) {
        showError(tr("Select one or more archive items first."));
        return;
    }

    const QString destinationDir = promptDestinationDirectory(tr("Choose Copy Destination"),
                                                             QDir::homePath());
    if (destinationDir.isEmpty()) {
        return;
    }

    extractPathsToDirectory(archivePaths, destinationDir);
}

void ArchiveBrowserDialog::onOpenClicked()
{
    openArchiveItemDefault();
}

void ArchiveBrowserDialog::onOpenWithClicked()
{
    openArchiveItemWithProgram();
}

void ArchiveBrowserDialog::onCopyPathsClicked()
{
    const QStringList archivePaths = selectedFilePathsForExtraction();
    if (archivePaths.isEmpty()) {
        return;
    }

    QGuiApplication::clipboard()->setText(archivePaths.join('\n'));
    showInfo(tr("Copied %1 archive path(s) to the clipboard.").arg(archivePaths.size()));
}

void ArchiveBrowserDialog::onRefreshClicked()
{
    if (m_archivePath.isEmpty()) {
        return;
    }
    loadArchive(m_archivePath, m_password);
}

void ArchiveBrowserDialog::onTreeContextMenuRequested(const QPoint& pos)
{
    if (!m_tree->itemAt(pos)) {
        return;
    }

    QMenu menu(this);
    menu.addAction(tr("Open"), this, &ArchiveBrowserDialog::onOpenClicked);
    menu.addAction(tr("Open With..."), this, &ArchiveBrowserDialog::onOpenWithClicked);
    menu.addAction(tr("Extract / Copy..."), this, &ArchiveBrowserDialog::onExtractClicked);
    if (archiveIsWritable()) {
        menu.addAction(tr("Add Files..."), this, &ArchiveBrowserDialog::onAddFilesClicked);
    }
    menu.addSeparator();
    menu.addAction(tr("Copy Paths"), this, &ArchiveBrowserDialog::onCopyPathsClicked);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void ArchiveBrowserDialog::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    openArchiveItemDefault();
}

void ArchiveBrowserDialog::onViewModeTriggered()
{
    const QAction* action = qobject_cast<QAction*>(sender());
    if (action == m_compactAction) {
        applyViewMode(ViewMode::Compact);
    } else if (action == m_iconsAction) {
        applyViewMode(ViewMode::Icons);
    } else {
        applyViewMode(ViewMode::Details);
    }
}

void ArchiveBrowserDialog::onFilterChanged(const QString& text)
{
    applyFilter(text);
}

bool ArchiveBrowserDialog::loadArchive(const QString& archivePath, const QString& password)
{
    const auto* format = formatForArchivePath(archivePath);
    if (!format) {
        showError(tr("Unsupported archive type. Please open a .zip or .7z archive."));
        return false;
    }

    const QString runtimePath = resolveSevenZipLibraryPath();
    if (runtimePath.isEmpty()) {
        showError(tr("Could not find the bundled 7-Zip runtime. Put it next to the app or set ABSAND_7ZIP_LIBRARY."));
        return false;
    }

    try {
        m_library = std::make_unique<bit7z::Bit7zLibrary>(runtimePath.toStdString());
        m_reader = std::make_unique<bit7z::BitArchiveReader>(
            *m_library,
            archivePath.toStdString(),
            *format,
            password.toStdString()
        );
        m_format = format;
        m_archivePath = archivePath;
        m_password = password;
        m_archivePathEdit->setText(archivePath);
        rebuildTree();
        updateStatusText();
        return true;
    } catch (const std::exception& ex) {
        showError(tr("Failed to open archive: %1").arg(ex.what()));
    } catch (...) {
        showError(tr("Failed to open archive."));
    }

    return false;
}

void ArchiveBrowserDialog::rebuildTree()
{
    m_tree->clear();
    if (!m_reader) {
        return;
    }

    for (std::uint32_t i = 0; i < m_reader->itemsCount(); ++i) {
        const auto item = m_reader->itemAt(i);
        const QString archivePath = normalizeArchivePath(toQString(item.path()));
        if (archivePath.isEmpty()) {
            continue;
        }

        rebuildTreeRecursive(m_tree->invisibleRootItem(), archivePath, static_cast<int>(i));
    }

    m_tree->expandToDepth(1);
    m_tree->sortItems(ColumnName, Qt::AscendingOrder);
    applyFilter(m_filterEdit ? m_filterEdit->text() : QString());
    updateStatusText();
}

void ArchiveBrowserDialog::rebuildTreeRecursive(QTreeWidgetItem* parent, const QString& archivePath, int index)
{
    Q_UNUSED(parent);
    const QStringList segments = pathSegments(archivePath);
    if (segments.isEmpty()) {
        return;
    }

    QString currentPath;
    for (int i = 0; i < segments.size(); ++i) {
        const QString& segment = segments.at(i);
        currentPath = currentPath.isEmpty() ? segment : currentPath + '/' + segment;

        QTreeWidgetItem* node = ensurePathNode(pathSegments(currentPath));
        if (i == segments.size() - 1) {
            const auto itemInfo = m_reader->itemAt(static_cast<std::uint32_t>(index));
            node->setText(ColumnName, segment);
            node->setData(ColumnName, RoleArchiveIndex, index);
            node->setData(ColumnName, RoleArchivePath, archivePath);
            node->setData(ColumnName, RoleIsDirectory, itemInfo.isDir());
            node->setText(ColumnSize, itemInfo.isDir() ? QString() : humanReadableSize(itemInfo.size()));
            node->setText(ColumnType, fileTypeLabel(itemInfo.isDir(), archivePath));
            node->setText(ColumnModified, formatDateTime(itemInfo.lastWriteTime()));
            node->setIcon(ColumnName, style()->standardIcon(itemInfo.isDir() ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
        }
    }
}

QTreeWidgetItem* ArchiveBrowserDialog::ensurePathNode(const QStringList& segments)
{
    if (segments.isEmpty()) {
        return m_tree->invisibleRootItem();
    }

    QTreeWidgetItem* parent = m_tree->invisibleRootItem();
    QString currentPath;

    for (const QString& segment : segments) {
        currentPath = currentPath.isEmpty() ? segment : currentPath + '/' + segment;

        QTreeWidgetItem* existing = nullptr;
        for (int i = 0; i < parent->childCount(); ++i) {
            auto* child = parent->child(i);
            if (child->data(ColumnName, RoleArchivePath).toString() == currentPath) {
                existing = child;
                break;
            }
        }

        if (!existing) {
            existing = new QTreeWidgetItem(parent);
            existing->setText(ColumnName, segment);
            existing->setData(ColumnName, RoleArchiveIndex, -1);
            existing->setData(ColumnName, RoleArchivePath, currentPath);
            existing->setData(ColumnName, RoleIsDirectory, true);
            existing->setText(ColumnType, tr("Folder"));
            existing->setIcon(ColumnName, style()->standardIcon(QStyle::SP_DirIcon));
        }

        parent = existing;
    }

    return parent;
}

QString ArchiveBrowserDialog::normalizeArchivePath(QString path) const
{
    return normalizeSeparators(std::move(path));
}

QStringList ArchiveBrowserDialog::pathSegments(const QString& path) const
{
    return normalizeArchivePath(path).split('/', Qt::SkipEmptyParts);
}

QString ArchiveBrowserDialog::selectedArchiveItemPath() const
{
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        return {};
    }

    return items.first()->data(ColumnName, RoleArchivePath).toString();
}

QStringList ArchiveBrowserDialog::selectedArchiveItemPaths() const
{
    QStringList paths;
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    for (QTreeWidgetItem* item : items) {
        const int indexValue = item->data(ColumnName, RoleArchiveIndex).toInt();
        if (indexValue < 0) {
            continue;
        }
        paths.append(item->data(ColumnName, RoleArchivePath).toString());
    }
    paths.removeDuplicates();
    return paths;
}

QStringList ArchiveBrowserDialog::selectedFilePathsForExtraction() const
{
    QStringList paths;
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    for (QTreeWidgetItem* item : items) {
        const auto indices = collectIndices(item);
        for (std::uint32_t index : indices) {
            const auto archiveItem = m_reader->itemAt(index);
            const QString path = normalizeArchivePath(toQString(archiveItem.path()));
            if (!path.isEmpty()) {
                paths.append(path);
            }
        }
    }
    paths.removeDuplicates();
    return paths;
}

QString ArchiveBrowserDialog::promptPassword() const
{
    bool ok = false;
    const QString password = QInputDialog::getText(
        const_cast<ArchiveBrowserDialog*>(this),
        tr("Archive Password"),
        tr("Enter password if this archive is encrypted:"),
        QLineEdit::Password,
        QString(),
        &ok
    );

    return ok ? password : QString();
}

QString ArchiveBrowserDialog::promptDestinationDirectory(const QString& title, const QString& startDir) const
{
    return QFileDialog::getExistingDirectory(const_cast<ArchiveBrowserDialog*>(this), title, startDir);
}

QString ArchiveBrowserDialog::promptProgramPath() const
{
    return QFileDialog::getOpenFileName(
        const_cast<ArchiveBrowserDialog*>(this),
        tr("Choose Program"),
        QDir::homePath(),
        tr("Programs (*);;All Files (*)")
    );
}

QString ArchiveBrowserDialog::createScratchDirectory(const QString& hint) const
{
    const QString baseName = QStringLiteral("absand-%1-%2").arg(hint, QUuid::createUuid().toString(QUuid::Id128));
    const QString dirPath = QDir(QDir::tempPath()).filePath(baseName);
    QDir().mkpath(dirPath);
    return dirPath;
}

void ArchiveBrowserDialog::extractPathsToDirectory(const QStringList& archivePaths, const QString& destinationDir, bool notify)
{
    if (!m_reader) {
        return;
    }

    bit7z::IndicesVector indices;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const auto itemIndices = collectIndices(item);
        indices.insert(indices.end(), itemIndices.begin(), itemIndices.end());
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    try {
        m_reader->extractTo(destinationDir.toStdString(), indices);
        if (notify) {
            showInfo(tr("Copied %1 item(s) to %2").arg(archivePaths.size()).arg(destinationDir));
        }
    } catch (const std::exception& ex) {
        showError(tr("Failed to copy items: %1").arg(ex.what()));
    } catch (...) {
        showError(tr("Failed to copy items."));
    }
}

void ArchiveBrowserDialog::openArchiveItemDefault()
{
    const QString path = selectedArchiveItemPath();
    if (path.isEmpty()) {
        return;
    }

    const QString scratchDir = createScratchDirectory("open");
    extractPathsToDirectory({path}, scratchDir, false);

    const QString extractedPath = QDir(scratchDir).filePath(path);
    openPathInSystem(extractedPath);
}

void ArchiveBrowserDialog::openArchiveItemWithProgram()
{
    const QString program = promptProgramPath();
    if (program.isEmpty()) {
        return;
    }
    openArchiveItemWithProgram(program);
}

void ArchiveBrowserDialog::openArchiveItemWithProgram(const QString& programPath)
{
    const QString path = selectedArchiveItemPath();
    if (path.isEmpty()) {
        return;
    }

    const QString scratchDir = createScratchDirectory("openwith");
    extractPathsToDirectory({path}, scratchDir, false);

    const QString extractedPath = QDir(scratchDir).filePath(path);
    if (!QProcess::startDetached(programPath, {extractedPath})) {
        showError(tr("Could not launch %1").arg(programPath));
    }
}

void ArchiveBrowserDialog::openPathInSystem(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        showError(tr("Extracted file was not found: %1").arg(path));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        showError(tr("Could not open %1").arg(path));
    }
}

bool ArchiveBrowserDialog::addFilesToArchive(const QStringList& files)
{
    if (!m_library || !m_format || m_archivePath.isEmpty()) {
        return false;
    }

    try {
        bit7z::BitArchiveWriter writer(
            *m_library,
            m_archivePath.toStdString(),
            *m_format,
            m_password.toStdString()
        );
        writer.setUpdateMode(bit7z::UpdateMode::Update);
        writer.addFiles(toBit7zPaths(files));
        writer.compressTo(m_archivePath.toStdString());
        return true;
    } catch (const std::exception& ex) {
        showError(tr("Failed to add files: %1").arg(ex.what()));
    } catch (...) {
        showError(tr("Failed to add files."));
    }

    return false;
}

bool ArchiveBrowserDialog::archiveIsWritable() const
{
    return m_format != nullptr;
}

const bit7z::BitInOutFormat* ArchiveBrowserDialog::formatForArchivePath(const QString& archivePath) const
{
    const QString lower = QFileInfo(archivePath).suffix().toLower();
    if (lower == "7z") {
        return &bit7z::BitFormat::SevenZip;
    }
    if (lower == "zip") {
        return &bit7z::BitFormat::Zip;
    }
    return nullptr;
}

const bit7z::Bit7zLibrary& ArchiveBrowserDialog::library() const
{
    return *m_library;
}

void ArchiveBrowserDialog::setStatusText(const QString& text)
{
    m_statusText = text;
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

void ArchiveBrowserDialog::updateStatusText()
{
    if (!m_reader) {
        setStatusText(tr("No archive loaded."));
        if (m_summaryLabel) {
            m_summaryLabel->setText(QString());
        }
        return;
    }

    setStatusText(tr("%1 — %2 item(s)").arg(QFileInfo(m_archivePath).fileName()).arg(m_reader->itemsCount()));
    if (m_summaryLabel) {
        const QString format = m_format ? (m_format == &bit7z::BitFormat::Zip ? tr("ZIP") : tr("7z")) : tr("Unknown");
        const int visibleItems = [&]() {
            int count = 0;
            for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
                if (!m_tree->topLevelItem(i)->isHidden()) {
                    ++count;
                }
            }
            return count;
        }();
        m_summaryLabel->setText(tr("%1 • %2 visible").arg(format).arg(visibleItems));
    }
}

void ArchiveBrowserDialog::showError(const QString& text)
{
    QMessageBox::critical(this, tr("Archive Browser"), text);
}

void ArchiveBrowserDialog::showInfo(const QString& text)
{
    QMessageBox::information(this, tr("Archive Browser"), text);
}

QString ArchiveBrowserDialog::humanReadableSize(std::uint64_t size)
{
    const double value = static_cast<double>(size);
    if (size < 1024) {
        return QString::number(size) + QStringLiteral(" B");
    }
    if (size < 1024ull * 1024ull) {
        return QString::number(value / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }
    if (size < 1024ull * 1024ull * 1024ull) {
        return QString::number(value / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(value / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

QString ArchiveBrowserDialog::toQString(const std::string& value)
{
    return QString::fromStdString(value);
}
