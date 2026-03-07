#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QMap>
#include <QTimer>
#include <QSet>

#include "ConfigManager.h"

class WatcherManager : public QObject
{
    Q_OBJECT

public:
    explicit WatcherManager(QObject *parent = nullptr);

    void startWatching(const QList<WatchEntry> &entries);
    void stopWatching();
    void addWatch(const WatchEntry &entry);
    void removeWatch(const QString &name);
    void pause();
    void resume();
    bool isPaused() const { return m_paused; }

signals:
    // Emits the watch name + list of changed/new file paths (relative to watch dir)
    void filesChanged(const QString &watchName, const QString &emoji, const QStringList &files);
    void watchError(const QString &watchName, const QString &error);

private slots:
    void onDirectoryChanged(const QString &path);
    void onScanTimer();

private:
    void scanDirectory(const QString &watchPath, const QString &watchName, const QString &emoji);

    QFileSystemWatcher m_watcher;
    QTimer m_scanTimer;

    struct WatchInfo {
        WatchEntry entry;
        QMap<QString, qint64> knownFiles; // path -> last modified timestamp
    };

    QMap<QString, WatchInfo> m_watches; // path -> info
    bool m_paused = false;
};
