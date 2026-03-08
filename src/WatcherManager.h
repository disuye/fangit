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

    void startWatching(const QList<WatchEntry> &entries, int scanIntervalSecs = 30);
    void stopWatching();
    void addWatch(const WatchEntry &entry);
    void removeWatch(const QString &pathName);
    void pause();
    void resume();
    bool isPaused() const { return m_paused; }

signals:
    void filesChanged(const QString &pathName, const QString &emoji,
                      const QString &action, const QStringList &files);
    void watchError(const QString &pathName, const QString &error);

private slots:
    void onDirectoryChanged(const QString &path);
    void onScanTimer();

private:
    void scanDirectory(const QString &watchPath, const QString &pathName,
                       const QString &emoji, const QString &action);

    QFileSystemWatcher m_watcher;
    QTimer m_scanTimer;

    struct WatchInfo {
        WatchEntry entry;
        QMap<QString, qint64> knownFiles; // path -> last modified timestamp
    };

    QMap<QString, WatchInfo> m_watches; // path -> info
    bool m_paused = false;
};
