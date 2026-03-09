#pragma once

#include <QString>
#include "FileScanner.h"

class MessageFormatter
{
public:
    MessageFormatter() = default;

    // Format a notification message from scan results
    // Used for notify-mode watches (action → channel)
    QString formatNotify(const WatchScanResult &scan) const;

    // Format a git commit message from scan results
    // Used for sync-mode watches (action = "sync")
    QString formatCommit(const WatchScanResult &scan) const;

    // Format a simple file-list notification (no scanning, just filenames)
    // Used when no tail/match features are needed
    QString formatFileList(const QString &pathName, const QString &emoji,
                           const QStringList &files) const;

    // Format a git commit message from filenames + sizes
    // Used for sync mode without scanning
    QString formatCommitSimple(const QString &pathName, const QString &emoji,
                               const QStringList &files, const QString &watchPath) const;
};
