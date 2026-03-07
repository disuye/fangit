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

    // Periodic full scan to catch anything QFileSystemWatcher misses
    m_scanTimer.setInterval(10000); // 10 second scan interval
    connect(&m_scanTimer, &QTimer::timeout, this, &WatcherManager::onScanTimer);
}

void WatcherManager::startWatching(const QList<WatchEntry> &entries)
{
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
        emit watchError(entry.name, "Directory does not exist: " + entry.path);
        return;
    }

    QString absPath = dir.absolutePath();

    WatchInfo info;
    info.entry = entry;
    info.entry.path = absPath;

    // Build initial snapshot of files
    QDirIterator it(absPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        info.knownFiles[it.filePath()] = it.fileInfo().lastModified().toSecsSinceEpoch();
    }

    m_watches[absPath] = info;
    m_watcher.addPath(absPath);

    // Also watch subdirectories
    QDirIterator dirIt(absPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        m_watcher.addPath(dirIt.filePath());
    }

    qDebug() << "Watching:" << entry.name << "at" << absPath
             << "(" << info.knownFiles.size() << "existing files)";
}

void WatcherManager::removeWatch(const QString &name)
{
    for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
        if (it->entry.name == name) {
            m_watcher.removePath(it.key());
            m_watches.erase(it);
            break;
        }
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

    // Find which watch this belongs to
    for (auto &info : m_watches) {
        if (path.startsWith(info.entry.path)) {
            scanDirectory(info.entry.path, info.entry.name, info.entry.emoji);
            break;
        }
    }

    // Re-add the path (QFileSystemWatcher can drop paths after events on some platforms)
    if (!m_watcher.directories().contains(path) && QDir(path).exists())
        m_watcher.addPath(path);
}

void WatcherManager::onScanTimer()
{
    if (m_paused) return;

    for (const auto &key : m_watches.keys()) {
        auto &info = m_watches[key];
        scanDirectory(info.entry.path, info.entry.name, info.entry.emoji);
    }
}

void WatcherManager::scanDirectory(const QString &watchPath, const QString &watchName, const QString &emoji)
{
    auto &info = m_watches[watchPath];
    QStringList changedFiles;

    QDirIterator it(watchPath, QDir::Files, QDirIterator::Subdirectories);
    QSet<QString> currentFiles;

    while (it.hasNext()) {
        it.next();
        QString filePath = it.filePath();
        qint64 modified = it.fileInfo().lastModified().toSecsSinceEpoch();
        currentFiles.insert(filePath);

        // Check extension filter
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

    // Detect deleted files (update snapshot, but don't emit — we're append-only by design)
    QStringList toRemove;
    for (auto it = info.knownFiles.begin(); it != info.knownFiles.end(); ++it) {
        if (!currentFiles.contains(it.key()))
            toRemove.append(it.key());
    }
    for (const auto &r : toRemove)
        info.knownFiles.remove(r);

    if (!changedFiles.isEmpty()) {
        qDebug() << "Changes detected in" << watchName << ":" << changedFiles.size() << "file(s)";
        emit filesChanged(watchName, emoji, changedFiles);
    }
}
