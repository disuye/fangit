#include "MessageFormatter.h"

#include <QFileInfo>

// ── Notify formatting ──────────────────────────────────────────────────────
// Used when watch action routes to a channel
//
// Layout principle: lead with the content (what happened), then metadata.
// The first line is what the user sees on the iOS notification banner.

QString MessageFormatter::formatNotify(const WatchScanResult &scan) const
{
    QString emoji = scan.emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : scan.emoji;

    if (scan.files.isEmpty())
        return emoji + " " + scan.pathName + ": no new content";

    // Single file
    if (scan.files.size() == 1) {
        const auto &f = scan.files.first();

        if (f.hasMatch) {
            QString matchLine = f.matchedLines.last();
            return "\xe2\x86\x92 \"" + matchLine + "\" "
                 + "(" + QString::number(f.newLineCount) + " new lines, "
                 + QString::number(f.matchedLines.size()) + " match)\n"
                 + emoji + " " + scan.pathName + ": " + f.fileName;
        }

        // No match pattern or no matches — show last line
        return "\xe2\x86\x92 \"" + f.lastLine + "\" "
             + "(" + QString::number(f.newLineCount) + " new lines)\n"
             + emoji + " " + scan.pathName + ": " + f.fileName;
    }

    // Multiple files
    if (scan.hasAnyMatch) {
        // Lead with the last match from the most recent matched file
        QString lastMatch;
        for (int i = scan.files.size() - 1; i >= 0; --i) {
            if (scan.files[i].hasMatch) {
                lastMatch = scan.files[i].matchedLines.last();
                break;
            }
        }
        QString msg = "\xe2\x86\x92 \"" + lastMatch + "\" "
                    + "(" + QString::number(scan.totalMatches) + " match across "
                    + QString::number(scan.files.size()) + " files)\n"
                    + emoji + " " + scan.pathName;

        for (const auto &f : scan.files) {
            msg += "\n  " + f.fileName + " (" + QString::number(f.newLineCount) + " new lines)";
            if (f.hasMatch)
                msg += " \xe2\x86\x92 \"" + f.matchedLines.last() + "\"";
        }
        return msg;
    }

    // Multiple files, no match filter — lead with last file's last line
    const auto &lastFile = scan.files.last();
    QString msg = "\xe2\x86\x92 \"" + lastFile.lastLine + "\" "
                + "(" + QString::number(scan.files.size()) + " files changed)\n"
                + emoji + " " + scan.pathName;

    for (const auto &f : scan.files) {
        msg += "\n  " + f.fileName + " (" + QString::number(f.newLineCount) + " new lines)";
        if (!f.lastLine.isEmpty())
            msg += " \xe2\x86\x92 \"" + f.lastLine + "\"";
    }

    return msg;
}

// ── Commit formatting ──────────────────────────────────────────────────────
// Used when watch action is "sync" and file scanning is active

QString MessageFormatter::formatCommit(const WatchScanResult &scan) const
{
    QString emoji = scan.emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : scan.emoji;

    if (scan.files.size() == 1) {
        const auto &f = scan.files.first();
        return emoji + " " + scan.pathName + " \xe2\x80\x94 " + f.fileName
             + " (" + QString::number(f.newLineCount) + " new lines)";
    }

    QString msg = emoji + " " + scan.pathName + " \xe2\x80\x94 "
                + QString::number(scan.files.size()) + " file(s)\n";
    for (const auto &f : scan.files) {
        msg += "\n\xe2\x80\xa2 " + f.fileName;
    }
    return msg;
}

// ── Simple file list (no scanning) ─────────────────────────────────────────

QString MessageFormatter::formatFileList(const QString &pathName, const QString &emoji,
                                          const QStringList &files) const
{
    QString e = emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : emoji;

    if (files.size() == 1)
        return "\xe2\x86\x92 " + files.first() + "\n"
             + e + " " + pathName;

    QString msg = "\xe2\x86\x92 " + QString::number(files.size()) + " file(s)\n"
                + e + " " + pathName;
    if (files.size() <= 5) {
        for (const auto &f : files)
            msg += "\n  " + f;
    }
    return msg;
}

// ── Simple commit message (no scanning, with file sizes) ───────────────────

QString MessageFormatter::formatCommitSimple(const QString &pathName, const QString &emoji,
                                              const QStringList &files,
                                              const QString &watchPath) const
{
    QString e = emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : emoji;

    if (files.size() == 1) {
        QString fullPath = watchPath + "/" + files.first();
        QFileInfo fi(fullPath);
        qint64 size = fi.size();
        QString sizeStr;
        if (size < 1024)
            sizeStr = QString::number(size) + "B";
        else
            sizeStr = QString::number(size / 1024.0, 'f', 1) + "KB";

        return e + " " + pathName + " \xe2\x80\x94 " + files.first() + " (" + sizeStr + ")";
    }

    QString msg = e + " " + pathName + " \xe2\x80\x94 " + QString::number(files.size()) + " file(s)\n";
    for (const auto &f : files) {
        msg += "\n\xe2\x80\xa2 " + f;
    }
    return msg;
}