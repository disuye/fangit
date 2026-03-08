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

void CommitBatcher::enqueueFiles(const QString &pathName, const QString &emoji,
                                  const QString &action, const QStringList &files)
{
    auto &batch = m_pending[pathName];
    batch.pathName = pathName;
    batch.emoji = emoji;
    batch.action = action;

    for (const auto &f : files) {
        if (!batch.files.contains(f))
            batch.files.append(f);
    }

    emit pendingCountChanged(pendingCount());

    // Restart debounce timer
    m_debounceTimer.start(m_config.batchInterval() * 1000);
}

void CommitBatcher::pushNow()
{
    m_debounceTimer.stop();
    onTimerFired();
}

// Check if the action string matches any configured channel path_name
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

void CommitBatcher::onTimerFired()
{
    if (m_pending.isEmpty()) return;

    emit batchStarted();

    bool allSuccess = true;
    bool hasSyncBatches = false;

    // Route each batch based on its action
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const PendingBatch &batch = it.value();

        if (isNotifyAction(batch.action)) {
            processNotifyBatch(batch);
        } else {
            processSyncBatch(batch, allSuccess);
            hasSyncBatches = true;
        }
    }

    // Push all sync commits in one go
    if (hasSyncBatches) {
        if (!m_git.push())
            allSuccess = false;
    }

    m_pending.clear();
    emit pendingCountChanged(0);
    emit batchFinished(allSuccess);
}

// ── Sync route: git add + commit ───────────────────────────────────────────

void CommitBatcher::processSyncBatch(const PendingBatch &batch, bool &allSuccess)
{
    copyFilesToRepo(batch);

    QStringList repoFiles;
    for (const auto &f : batch.files)
        repoFiles.append(batch.pathName + "/" + f);

    if (!m_git.add(repoFiles)) {
        allSuccess = false;
        return;
    }

    QString msg = formatCommitMessage(batch);
    if (!m_git.commit(msg)) {
        allSuccess = false;
    }
}

// ── Notify route: send filenames via channel, no git ───────────────────────

void CommitBatcher::processNotifyBatch(const PendingBatch &batch)
{
    QString message = formatNotifyMessage(batch);
    m_notify.notify(batch.action, message);

    qDebug() << "Notify routed:" << batch.pathName
             << "→ channel" << batch.action
             << "(" << batch.files.size() << "files)";
}

// ── Message formatting ─────────────────────────────────────────────────────

QString CommitBatcher::formatNotifyMessage(const PendingBatch &batch) const
{
    QString emoji = batch.emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : batch.emoji;

    if (batch.files.size() == 1) {
        return emoji + " " + batch.pathName + ": " + batch.files.first();
    }

    QString msg = emoji + " " + batch.pathName + ": " + QString::number(batch.files.size()) + " file(s)";
    // Include filenames for small batches
    if (batch.files.size() <= 5) {
        for (const auto &f : batch.files)
            msg += "\n  " + f;
    }
    return msg;
}

QString CommitBatcher::formatCommitMessage(const PendingBatch &batch) const
{
    QString emoji = batch.emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : batch.emoji;

    // Resolve full path for file size calculation
    QString watchPath;
    for (const auto &entry : m_config.watchEntries()) {
        if (entry.pathName == batch.pathName) {
            watchPath = entry.path;
            break;
        }
    }

    if (batch.files.size() == 1) {
        QString fullPath = watchPath + "/" + batch.files.first();
        QFileInfo fi(fullPath);
        qint64 size = fi.size();
        QString sizeStr;
        if (size < 1024)
            sizeStr = QString::number(size) + "B";
        else
            sizeStr = QString::number(size / 1024.0, 'f', 1) + "KB";

        return emoji + " " + batch.pathName + " \xe2\x80\x94 " + batch.files.first() + " (" + sizeStr + ")";
    }

    QString msg = emoji + " " + batch.pathName + " \xe2\x80\x94 " + QString::number(batch.files.size()) + " file(s)\n";
    for (const auto &f : batch.files) {
        msg += "\n\xe2\x80\xa2 " + f;
    }
    return msg;
}

// ── File copy (sync mode only) ─────────────────────────────────────────────

void CommitBatcher::copyFilesToRepo(const PendingBatch &batch)
{
    QString repoBase = m_config.repoLocalPath();

    QString watchPath;
    for (const auto &entry : m_config.watchEntries()) {
        if (entry.pathName == batch.pathName) {
            watchPath = entry.path;
            break;
        }
    }

    if (watchPath.isEmpty()) {
        qWarning() << "Cannot find watch path for" << batch.pathName;
        return;
    }

    for (const auto &relFile : batch.files) {
        QString srcPath = watchPath + "/" + relFile;
        QString dstPath = repoBase + "/" + batch.pathName + "/" + relFile;

        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        if (QFile::exists(dstPath))
            QFile::remove(dstPath);

        if (!QFile::copy(srcPath, dstPath)) {
            qWarning() << "Failed to copy" << srcPath << "to" << dstPath;
        }
    }
}
