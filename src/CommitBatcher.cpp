#include "CommitBatcher.h"
#include "GitManager.h"
#include "ConfigManager.h"
#include "NotifyManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

CommitBatcher::CommitBatcher(GitManager &git, ConfigManager &config,
                             NotifyManager &notify, QObject *parent)
    : QObject(parent)
    , m_git(git)
    , m_config(config)
    , m_notify(notify)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &CommitBatcher::onTimerFired);
}

int CommitBatcher::pendingCount() const
{
    int total = 0;
    for (const auto &batch : m_pending)
        total += batch.files.size();
    return total;
}

void CommitBatcher::seedOffsets(const QString &pathName, const QMap<QString, qint64> &offsets)
{
    m_fileOffsets[pathName] = offsets;
}

void CommitBatcher::enqueueFiles(const QString &pathName, const QString &emoji,
                                  const QString &action, const QString &watchPath,
                                  const QString &match, const QStringList &files)
{
    // ── Notify route: fire immediately ─────────────────────────────
    if (isNotifyAction(action)) {
        PendingBatch batch;
        batch.pathName = pathName;
        batch.emoji = emoji;
        batch.action = action;
        batch.watchPath = watchPath;
        batch.match = match;
        batch.files = files;
        processNotifyBatch(batch);
        return;
    }

    // ── Sync route: accumulate and debounce ────────────────────────
    auto &batch = m_pending[pathName];
    batch.pathName = pathName;
    batch.emoji = emoji;
    batch.action = action;
    batch.watchPath = watchPath;
    batch.match = match;

    for (const auto &f : files) {
        if (!batch.files.contains(f))
            batch.files.append(f);
    }

    emit pendingCountChanged(pendingCount());

    // Start timer only if not already running
    if (!m_debounceTimer.isActive())
        m_debounceTimer.start(m_config.batchInterval() * 1000);
}

void CommitBatcher::pushNow()
{
    m_debounceTimer.stop();
    onTimerFired();
}

bool CommitBatcher::isNotifyAction(const QString &action) const
{
    if (action.isEmpty() || action == "sync")
        return false;

    const auto &channels = m_config.channels();
    for (const auto &ch : channels) {
        if (ch.pathName == action)
            return true;
    }
    return false;
}

// ── Timer fired: process all pending sync batches ──────────────────────────

void CommitBatcher::onTimerFired()
{
    if (m_pending.isEmpty()) return;

    emit batchStarted();

    bool allSuccess = true;

    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const PendingBatch &batch = it.value();
        processSyncBatch(batch, allSuccess);
    }

    if (!m_git.push())
        allSuccess = false;

    m_pending.clear();
    emit pendingCountChanged(0);
    emit batchFinished(allSuccess);
}

// ── Sync route ─────────────────────────────────────────────────────────────

void CommitBatcher::processSyncBatch(const PendingBatch &batch, bool &allSuccess)
{
    // If match is set, scan files and only sync if there's a match
    if (!batch.match.isEmpty()) {
        auto &offsets = m_fileOffsets[batch.pathName];
        WatchScanResult scan = m_scanner.scanFiles(
            batch.watchPath, batch.pathName, batch.emoji, batch.action,
            batch.files, offsets, batch.match);

        if (!scan.hasAnyMatch) {
            qDebug() << "Sync skipped for" << batch.pathName << "— no regex match";
            return;
        }

        // Only copy and commit files that had matches
        QStringList matchedFiles;
        for (const auto &f : scan.files) {
            if (f.hasMatch)
                matchedFiles.append(f.relPath);
        }

        PendingBatch filtered = batch;
        filtered.files = matchedFiles;

        copyFilesToRepo(filtered);

        QStringList repoFiles;
        for (const auto &f : matchedFiles)
            repoFiles.append(batch.pathName + "/" + f);

        if (!m_git.add(repoFiles)) { allSuccess = false; return; }

        QString msg = m_formatter.formatCommit(scan);
        if (!m_git.commit(msg)) { allSuccess = false; }
        return;
    }

    // No match filter — sync all changed files
    copyFilesToRepo(batch);

    QStringList repoFiles;
    for (const auto &f : batch.files)
        repoFiles.append(batch.pathName + "/" + f);

    if (!m_git.add(repoFiles)) { allSuccess = false; return; }

    QString msg = m_formatter.formatCommitSimple(
        batch.pathName, batch.emoji, batch.files, batch.watchPath);
    if (!m_git.commit(msg)) { allSuccess = false; }
}

// ── Notify route ───────────────────────────────────────────────────────────

void CommitBatcher::processNotifyBatch(const PendingBatch &batch)
{
    auto &offsets = m_fileOffsets[batch.pathName];

    // Scan files for new content and optionally filter by match
    WatchScanResult scan = m_scanner.scanFiles(
        batch.watchPath, batch.pathName, batch.emoji, batch.action,
        batch.files, offsets, batch.match);

    // If match is set but nothing matched, skip notification
    if (!batch.match.isEmpty() && !scan.hasAnyMatch) {
        qDebug() << "Notify skipped for" << batch.pathName << "— no regex match";
        return;
    }

    // Format and send
    QString message;
    if (scan.files.isEmpty()) {
        // Fallback: just list filenames (new files with no readable content yet)
        message = m_formatter.formatFileList(batch.pathName, batch.emoji, batch.files);
    } else {
        message = m_formatter.formatNotify(scan);
    }

    m_notify.notify(batch.action, message);

    qDebug() << "Notify routed:" << batch.pathName
             << "\xe2\x86\x92 channel" << batch.action
             << "(" << batch.files.size() << "files,"
             << scan.totalNewLines << "new lines,"
             << scan.totalMatches << "matches)";
}

// ── File copy (sync mode only) ─────────────────────────────────────────────

void CommitBatcher::copyFilesToRepo(const PendingBatch &batch)
{
    QString repoBase = m_config.repoLocalPath();

    if (batch.watchPath.isEmpty()) {
        qWarning() << "Cannot find watch path for" << batch.pathName;
        return;
    }

    for (const auto &relFile : batch.files) {
        QString srcPath = batch.watchPath + "/" + relFile;
        QString dstPath = repoBase + "/" + batch.pathName + "/" + relFile;

        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        if (QFile::exists(dstPath))
            QFile::remove(dstPath);

        if (!QFile::copy(srcPath, dstPath))
            qWarning() << "Failed to copy" << srcPath << "to" << dstPath;
    }
}
