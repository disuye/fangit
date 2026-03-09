#include "WatcherManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

WatcherManager::WatcherManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &WatcherManager::onDirectoryChanged);

    // Periodic full scan — interval set by startWatching()
    connect(&m_scanTimer, &QTimer::timeout, this, &WatcherManager::onScanTimer);
}

void WatcherManager::startWatching(const QList<WatchEntry> &entries, int scanIntervalSecs)
{
    m_scanTimer.setInterval(scanIntervalSecs * 1000);

    for (const auto &entry : entries)
        addWatch(entry);

    if (!m_watches.isEmpty())
        m_scanTimer.start();
}

void WatcherManager::stopWatching()
{
    m_scanTimer.stop();
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_watches.clear();
}

void WatcherManager::addWatch(const WatchEntry &entry)
{
    QDir dir(entry.path);
    if (!dir.exists()) {
        qWarning() << "Watch directory does not exist:" << entry.path;
        emit watchError(entry.pathName, "Directory does not exist: " + entry.path);
        return;
    }

    QString absPath = dir.absolutePath();

    WatchInfo info;
    info.entry = entry;
    info.entry.path = absPath;

    // Build initial snapshot of files (filtered by this watch's extensions)
    QDirIterator it(absPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();

        // Only snapshot files matching this watch's extension filter
        if (!entry.extensions.isEmpty()) {
            QString suffix = it.fileInfo().suffix();
            if (!entry.extensions.contains(suffix, Qt::CaseInsensitive))
                continue;
        }

        // Skip hidden files
        if (it.fileName().startsWith('.'))
            continue;

        info.knownFiles[it.filePath()] = it.fileInfo().lastModified().toSecsSinceEpoch();
        info.fileOffsets[it.filePath()] = it.fileInfo().size();  // start from current end
    }

    // Key by pathName — each watch gets its own entry even if paths overlap
    m_watches[entry.pathName] = info;

    // Add filesystem path to QFileSystemWatcher (safe to add same path twice — Qt deduplicates)
    m_watcher.addPath(absPath);

    // Also watch subdirectories
    QDirIterator dirIt(absPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        m_watcher.addPath(dirIt.filePath());
    }

    qDebug() << "Watching:" << entry.pathName << "at" << absPath
             << "(" << info.knownFiles.size() << "existing files,"
             << "extensions:" << (entry.extensions.isEmpty() ? "all" : entry.extensions.join(","))
             << "action:" << (entry.action.isEmpty() ? "sync" : entry.action) << ")";
}

void WatcherManager::removeWatch(const QString &pathName)
{
    auto it = m_watches.find(pathName);
    if (it != m_watches.end()) {
        QString watchPath = it->entry.path;
        m_watches.erase(it);

        // Only remove the filesystem path if no other watch uses it
        bool pathStillUsed = false;
        for (const auto &info : m_watches) {
            if (info.entry.path == watchPath) {
                pathStillUsed = true;
                break;
            }
        }
        if (!pathStillUsed)
            m_watcher.removePath(watchPath);
    }

    if (m_watches.isEmpty())
        m_scanTimer.stop();
}

void WatcherManager::pause()
{
    m_paused = true;
    m_scanTimer.stop();
}

void WatcherManager::resume()
{
    m_paused = false;
    if (!m_watches.isEmpty())
        m_scanTimer.start();
}

void WatcherManager::onDirectoryChanged(const QString &path)
{
    if (m_paused) return;

    // Scan ALL watches whose path matches — no break, multiple watches can share a directory
    for (auto &info : m_watches) {
        if (path.startsWith(info.entry.path)) {
            scanDirectory(info.entry.pathName);
        }
    }

    // Re-add the path (QFileSystemWatcher can drop paths after events on some platforms)
    if (!m_watcher.directories().contains(path) && QDir(path).exists())
        m_watcher.addPath(path);
}

void WatcherManager::onScanTimer()
{
    if (m_paused) return;

    for (const auto &pathName : m_watches.keys()) {
        scanDirectory(pathName);
    }
}

void WatcherManager::scanDirectory(const QString &pathName)
{
    auto watchIt = m_watches.find(pathName);
    if (watchIt == m_watches.end()) return;

    WatchInfo &info = watchIt.value();
    const QString &watchPath = info.entry.path;
    QStringList changedFiles;

    QDirIterator it(watchPath, QDir::Files, QDirIterator::Subdirectories);
    QSet<QString> currentFiles;

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();
        qint64 modified = it.fileInfo().lastModified().toSecsSinceEpoch();
        currentFiles.insert(filePath);

        // Check extension filter for THIS watch
        if (!info.entry.extensions.isEmpty()) {
            QString suffix = it.fileInfo().suffix();
            if (!info.entry.extensions.contains(suffix, Qt::CaseInsensitive))
                continue;
        }

        // Skip .DS_Store and hidden files
        if (it.fileName().startsWith('.'))
            continue;

        auto known = info.knownFiles.find(filePath);
        if (known == info.knownFiles.end() || known.value() < modified) {
            // New or modified file — store relative path from watch dir
            QString relPath = filePath.mid(watchPath.length() + 1);
            changedFiles.append(relPath);
            info.knownFiles[filePath] = modified;
        }
    }

    // Detect deleted files (update snapshot, but don't emit — append-only by design)
    QStringList toRemove;
    for (auto it = info.knownFiles.begin(); it != info.knownFiles.end(); ++it) {
        if (!currentFiles.contains(it.key()))
            toRemove.append(it.key());
    }
    for (const auto &r : toRemove)
        info.knownFiles.remove(r);

    if (!changedFiles.isEmpty()) {
        qDebug() << "Changes detected in" << pathName << ":" << changedFiles.size() << "file(s)";
        emit filesChanged(pathName, info.entry.emoji, info.entry.action,
                         info.entry.path, info.entry.match, changedFiles);
    }
}
