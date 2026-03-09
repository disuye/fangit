#include "FileScanner.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

FileScanResult FileScanner::scan(const QString &filePath, const QString &relPath,
                                  qint64 &lastOffset, const QString &matchPattern)
{
    FileScanResult result;
    result.filePath = filePath;
    result.relPath = relPath;
    result.fileName = QFileInfo(filePath).fileName();
    result.previousSize = lastOffset;
    result.newLineCount = 0;
    result.hasMatch = false;

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        result.currentSize = 0;
        return result;
    }

    result.currentSize = fi.size();

    // No new content
    if (result.currentSize <= lastOffset) {
        lastOffset = result.currentSize;  // handle truncation
        return result;
    }

    // Read new bytes from lastOffset to end of file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "FileScanner: cannot open" << filePath;
        return result;
    }

    file.seek(lastOffset);
    QByteArray newBytes = file.readAll();
    file.close();

    // Update offset for next scan
    lastOffset = result.currentSize;

    // Split into lines
    QString newText = QString::fromUtf8(newBytes);
    QStringList lines = newText.split('\n', Qt::SkipEmptyParts);
    result.newLineCount = lines.size();

    if (lines.isEmpty())
        return result;

    // Store last line (trimmed to MAX_LINE_CHARS)
    result.lastLine = lines.last().trimmed();
    if (result.lastLine.length() > MAX_LINE_CHARS)
        result.lastLine = result.lastLine.left(MAX_LINE_CHARS) + "...";

    // Apply regex match if pattern is set
    if (!matchPattern.isEmpty()) {
        QRegularExpression regex(matchPattern, QRegularExpression::CaseInsensitiveOption);
        if (regex.isValid()) {
            for (const auto &line : lines) {
                if (regex.match(line).hasMatch()) {
                    QString trimmed = line.trimmed();
                    if (trimmed.length() > MAX_LINE_CHARS)
                        trimmed = trimmed.left(MAX_LINE_CHARS) + "...";
                    result.matchedLines.append(trimmed);
                }
            }
            result.hasMatch = !result.matchedLines.isEmpty();
        } else {
            qWarning() << "FileScanner: invalid regex pattern:" << matchPattern;
        }
    }

    return result;
}

WatchScanResult FileScanner::scanFiles(const QString &watchPath, const QString &pathName,
                                        const QString &emoji, const QString &action,
                                        const QStringList &changedFiles,
                                        QMap<QString, qint64> &fileOffsets,
                                        const QString &matchPattern)
{
    WatchScanResult result;
    result.pathName = pathName;
    result.emoji = emoji;
    result.action = action;
    result.totalNewLines = 0;
    result.totalMatches = 0;
    result.hasAnyMatch = false;

    for (const auto &relFile : changedFiles) {
        QString fullPath = watchPath + "/" + relFile;

        // Get or create offset for this file
        qint64 &offset = fileOffsets[fullPath];

        FileScanResult fileScan = scan(fullPath, relFile, offset, matchPattern);

        if (fileScan.newLineCount > 0) {
            result.totalNewLines += fileScan.newLineCount;
            result.totalMatches += fileScan.matchedLines.size();
            if (fileScan.hasMatch)
                result.hasAnyMatch = true;
            result.files.append(fileScan);
        }
    }

    return result;
}
