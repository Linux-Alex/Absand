// SPDX-License-Identifier: MPL-2.0

#ifndef TESTHARNESS_H
#define TESTHARNESS_H

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;
class QTextEdit;

class TestHarness : public QDialog
{
    Q_OBJECT

public:
    explicit TestHarness(QWidget* parent = nullptr);
    
    // Get the list of test files
    QStringList getTestFiles() const;

private slots:
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onClearClicked();
    void onLaunchAbsandClicked();
    void onRunDebugClicked();

private:
    void setupUI();
    void logMessage(const QString& message);
    
    QListWidget* m_fileListWidget;
    QTextEdit* m_logWidget;
    QPushButton* m_launchButton;
    QPushButton* m_debugButton;
    QStringList m_testFiles;
};

#endif // TESTHARNESS_H