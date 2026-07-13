// SPDX-License-Identifier: MPL-2.0

#include "TestHarness.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>

TestHarness::TestHarness(QWidget* parent)
    : QDialog(parent)
    , m_fileListWidget(nullptr)
    , m_logWidget(nullptr)
    , m_launchButton(nullptr)
    , m_debugButton(nullptr)
{
    setupUI();
}

void TestHarness::setupUI()
{
    setWindowTitle("Absand Test Harness");
    setMinimumSize(800, 600);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Instructions
    QTextEdit* instructions = new QTextEdit(this);
    instructions->setPlainText(
        "=== Absand Test Harness ===\n\n"
        "This tool simulates Dolphin's context menu.\n"
        "1. Add files/folders below\n"
        "2. Click 'Launch Absand' to test\n"
        "3. Or use 'Debug Mode' to see what gets passed\n\n"
        "Expected behavior: Absand should open with your selected items."
    );
    instructions->setReadOnly(true);
    instructions->setMaximumHeight(150);
    mainLayout->addWidget(instructions);
    
    // File list area
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* addFilesBtn = new QPushButton("Add Files", this);
    QPushButton* addFolderBtn = new QPushButton("Add Folder", this);
    QPushButton* clearBtn = new QPushButton("Clear All", this);
    
    buttonLayout->addWidget(addFilesBtn);
    buttonLayout->addWidget(addFolderBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    
    m_fileListWidget = new QListWidget(this);
    mainLayout->addWidget(m_fileListWidget);
    
    // Log area
    m_logWidget = new QTextEdit(this);
    m_logWidget->setReadOnly(true);
    m_logWidget->setMaximumHeight(200);
    mainLayout->addWidget(m_logWidget);
    
    // Action buttons
    QHBoxLayout* actionLayout = new QHBoxLayout();
    m_launchButton = new QPushButton("Launch Absand (Normal)", this);
    m_debugButton = new QPushButton("Launch Absand (Debug Mode)", this);
    QPushButton* closeBtn = new QPushButton("Close", this);
    
    actionLayout->addWidget(m_launchButton);
    actionLayout->addWidget(m_debugButton);
    actionLayout->addStretch();
    actionLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(actionLayout);
    
    // Connect signals
    connect(addFilesBtn, &QPushButton::clicked, this, &TestHarness::onAddFilesClicked);
    connect(addFolderBtn, &QPushButton::clicked, this, &TestHarness::onAddFolderClicked);
    connect(clearBtn, &QPushButton::clicked, this, &TestHarness::onClearClicked);
    connect(m_launchButton, &QPushButton::clicked, this, &TestHarness::onLaunchAbsandClicked);
    connect(m_debugButton, &QPushButton::clicked, this, &TestHarness::onRunDebugClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    logMessage("Test Harness ready");
}

void TestHarness::onAddFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Test");
    
    // Fix: Use const reference and index-based loop to avoid detach
    for (int i = 0; i < files.size(); ++i) {
        const QString& file = files.at(i);
        m_testFiles.append(file);
        m_fileListWidget->addItem(file);
        logMessage(QString("Added file: %1").arg(file));
    }
}

void TestHarness::onAddFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select Folder to Test");
    if (!folder.isEmpty()) {
        m_testFiles.append(folder);
        m_fileListWidget->addItem(folder);
        logMessage(QString("Added folder: %1").arg(folder));
    }
}

void TestHarness::onClearClicked()
{
    m_testFiles.clear();
    m_fileListWidget->clear();
    logMessage("Cleared all items");
}

void TestHarness::onLaunchAbsandClicked()
{
    if (m_testFiles.isEmpty()) {
        QMessageBox::warning(this, "No Files", "Please add files or folders to test.");
        return;
    }
    
    logMessage("========================================");
    logMessage(QString("Launching Absand with %1 items").arg(m_testFiles.size()));
    
    // Fix: Use index-based loop to avoid detach
    for (int i = 0; i < m_testFiles.size(); ++i) {
        logMessage(QString("  - %1").arg(m_testFiles.at(i)));
    }
    
    // Path to Absand executable
    QString absandPath = QCoreApplication::applicationDirPath() + "/Absand";
    
#ifdef Q_OS_WIN
    absandPath += ".exe";
#endif
    
    if (!QFile::exists(absandPath)) {
        // Try alternative paths
        QStringList altPaths;
        altPaths << "/home/aleks/QT Projects/Absand/build/Desktop_Qt_6_11_1-Debug/Absand";
        altPaths << "/usr/local/bin/Absand";
        altPaths << "/usr/bin/Absand";
        
        for (int i = 0; i < altPaths.size(); ++i) {
            if (QFile::exists(altPaths.at(i))) {
                absandPath = altPaths.at(i);
                break;
            }
        }
    }
    
    if (!QFile::exists(absandPath)) {
        logMessage("ERROR: Cannot find Absand executable!");
        QMessageBox::critical(this, "Error", 
            QString("Cannot find Absand executable.\nLooked in:\n%1").arg(absandPath));
        return;
    }
    
    logMessage(QString("Using Absand at: %1").arg(absandPath));
    
    // Launch Absand with test files
    QProcess* process = new QProcess(this);
    process->setProgram(absandPath);
    process->setArguments(m_testFiles);
    
    connect(process, &QProcess::started, [this]() {
        logMessage("Absand process started");
    });
    
    connect(process, &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        logMessage(QString("Process error: %1").arg(error));
    });
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, process](int exitCode, QProcess::ExitStatus status) {
            logMessage(QString("Absand finished with exit code: %1").arg(exitCode));
            if (status == QProcess::CrashExit) {
                logMessage("  Process crashed!");
            }
            process->deleteLater();
        });
    
    process->start();
    
    if (!process->waitForStarted(3000)) {
        logMessage("ERROR: Failed to start Absand process!");
    }
}

void TestHarness::onRunDebugClicked()
{
    if (m_testFiles.isEmpty()) {
        QMessageBox::warning(this, "No Files", "Please add files or folders to test.");
        return;
    }
    
    logMessage("========================================");
    logMessage("DEBUG MODE: Showing what would be passed");
    logMessage(QString("Would launch: Absand with %1 arguments:").arg(m_testFiles.size()));
    
    // Fix: Use index-based loop to avoid detach
    for (int i = 0; i < m_testFiles.size(); ++i) {
        logMessage(QString("  Arg[%1]: %2").arg(i).arg(m_testFiles.at(i)));
    }
    
    logMessage("\nThis is what Dolphin would send to Absand.");
    logMessage("If this works, the issue is with Dolphin integration.");
    logMessage("If not, the issue is with Absand's argument handling.");
    
    QMessageBox::information(this, "Debug Info", 
        QString("Test Harness would launch:\n\nAbsand %1\n\nCheck the log for details.")
        .arg(m_testFiles.join("\n")));
}

void TestHarness::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logWidget->append(QString("[%1] %2").arg(timestamp, message));
    qDebug() << message;
}

QStringList TestHarness::getTestFiles() const
{
    return m_testFiles;
}