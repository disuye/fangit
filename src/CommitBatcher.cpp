#include "CommitBatcher.h"
#include "GitManager.h"
#include "ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

CommitBatcher::CommitBatcher(GitManager &git, ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_git(git)
    , m_config(config)
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

void CommitBatcher::enqueueFiles(const QString &watchName, const QString &emoji, const QStringList &files)
{
    auto &batch = m_pending[watchName];
    batch.watchName = watchName;
    batch.emoji = emoji;

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

void CommitBatcher::onTimerFired()
{
    if (m_pending.isEmpty()) return;

    emit batchStarted();

    bool allSuccess = true;

    // Process each watch's pending files as a separate commit
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const PendingBatch &batch = it.value();

        // Copy files from watch directory into repo subdirectory
        copyFilesToRepo(batch);

        // Stage the files in the repo
        QStringList repoFiles;
        for (const auto &f : batch.files)
            repoFiles.append(batch.watchName + "/" + f);

        if (!m_git.add(repoFiles)) {
            allSuccess = false;
            continue;
        }

        // Commit
        QString msg = formatCommitMessage(batch);
        if (!m_git.commit(msg)) {
            allSuccess = false;
            continue;
        }
    }

    // Push all commits
    if (allSuccess || pendingCount() > 0) {
        if (!m_git.push())
            allSuccess = false;
    }

    m_pending.clear();
    emit pendingCountChanged(0);
    emit batchFinished(allSuccess);
}

void CommitBatcher::copyFilesToRepo(const PendingBatch &batch)
{
    QString repoBase = m_config.repoLocalPath();

    // Find the original watch path from config
    QString watchPath;
    for (const auto &entry : m_config.watchEntries()) {
        if (entry.name == batch.watchName) {
            watchPath = entry.path;
            break;
        }
    }

    if (watchPath.isEmpty()) {
        qWarning() << "Cannot find watch path for" << batch.watchName;
        return;
    }

    // Copy each file
    for (const auto &relFile : batch.files) {
        QString srcPath = watchPath + "/" + relFile;
        QString dstPath = repoBase + "/" + batch.watchName + "/" + relFile;

        QDir().mkpath(QFileInfo(dstPath).absolutePath());

        // Remove destination if exists (overwrite)
        if (QFile::exists(dstPath))
            QFile::remove(dstPath);

        if (!QFile::copy(srcPath, dstPath)) {
            qWarning() << "Failed to copy" << srcPath << "to" << dstPath;
        }
    }
}

QString CommitBatcher::formatCommitMessage(const PendingBatch &batch) const
{
    QString emoji = batch.emoji.isEmpty() ? QString::fromUtf8("\xF0\x9F\x93\x81") : batch.emoji; // default: folder emoji

    if (batch.files.size() == 1) {
        QFileInfo fi(batch.files.first());
        qint64 size = fi.size();
        QString sizeStr;
        if (size < 1024)
            sizeStr = QString::number(size) + "B";
        else
            sizeStr = QString::number(size / 1024.0, 'f', 1) + "KB";

        return emoji + " " + batch.watchName + " — " + batch.files.first() + " (" + sizeStr + ")";
    }

    QString msg = emoji + " " + batch.watchName + " — " + QString::number(batch.files.size()) + " file(s)\n";
    for (const auto &f : batch.files) {
        msg += "\n• " + f;
    }
    return msg;
}
