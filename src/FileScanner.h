#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

// Result of scanning a single file for new content
struct FileScanResult {
    QString filePath;       // absolute path
    QString relPath;        // relative to watch directory
    QString fileName;       // just the filename
    qint64 previousSize;    // byte offset before this scan
    qint64 currentSize;     // current file size
    int newLineCount;       // number of new lines added
    QString lastLine;       // last new line (trimmed)
    QStringList matchedLines;  // lines matching the regex (if any)
    bool hasMatch;          // true if regex was set and at least one line matched
};

// Result of scanning all changed files for one watch entry
struct WatchScanResult {
    QString pathName;
    QString emoji;
    QString action;
    QList<FileScanResult> files;
    int totalNewLines;
    int totalMatches;
    bool hasAnyMatch;       // true if any file had a regex match
};

class FileScanner
{
public:
    FileScanner() = default;

    // Scan a file for new content since lastOffset bytes.
    // If matchPattern is non-empty, only lines matching the regex are flagged.
    // Returns the scan result. Updates lastOffset to current file size.
    FileScanResult scan(const QString &filePath, const QString &relPath,
                        qint64 &lastOffset, const QString &matchPattern = QString());

    // Scan multiple files and aggregate into a WatchScanResult
    WatchScanResult scanFiles(const QString &watchPath, const QString &pathName,
                              const QString &emoji, const QString &action,
                              const QStringList &changedFiles,
                              QMap<QString, qint64> &fileOffsets,
                              const QString &matchPattern = QString());

    // Max characters to retain from a matched/last line
    static constexpr int MAX_LINE_CHARS = 300;
};
