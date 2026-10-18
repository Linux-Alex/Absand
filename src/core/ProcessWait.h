// SPDX-License-Identifier: MPL-2.0

#ifndef PROCESSWAIT_H
#define PROCESSWAIT_H

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QProcess>

namespace ProcessWait {

/**
 * Waits for a process while keeping the window repainting.
 *
 * Qt only repaints widgets from inside an event loop, so a plain
 * QProcess::waitForFinished() call blocks the GUI thread completely. Windows
 * then grays the window out and shows "not responding" as soon as a network
 * transfer takes more than a moment. Pumping input-less events keeps the
 * application responsive without allowing re-entrant user actions.
 *
 * @return true if the process finished, false if the timeout expired first.
 */
inline bool waitFinished(QProcess& process, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (process.state() != QProcess::NotRunning) {
        if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
            return false;
        }
        if (process.waitForFinished(100)) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    return process.state() == QProcess::NotRunning;
}

} // namespace ProcessWait

#endif // PROCESSWAIT_H
