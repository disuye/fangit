#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDir>
#include <QStandardPaths>

#include "NotifyManager.h"  // for NotifyChannel

struct WatchEntry {
    QString pathName;   // required — unique ID, also used as repo subdirectory name
    QString path;
    QString emoji;
    QString action;     // "sync" (default) or a channel path_name for notify routing
    QStringList extensions; // optional filter
};

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    // Load / save
    bool load();
    bool save();

    // Accessors — general
    QString githubUser() const { return m_githubUser; }
    int batchInterval() const { return m_batchInterval; }
    int scanInterval() const { return m_scanInterval; }
    enum class TrayStyle { Dot, Logo, Tint };
    TrayStyle trayStyle() const { return m_trayStyle; }

    // Accessors — repo
    QString repoUrl() const { return m_repoUrl; }
    QString repoLocalPath() const { return m_repoLocalPath; }
    QString repoBranch() const { return m_repoBranch; }
    QString authMethod() const { return m_authMethod; }

    // Accessors — watches and channels
    QList<WatchEntry> watchEntries() const { return m_watchEntries; }
    const QList<NotifyChannel> &channels() const { return m_channels; }

    // Mutators
    void setGithubUser(const QString &user);
    void setRepoUrl(const QString &url);
    void setRepoBranch(const QString &branch);
    void setBatchInterval(int seconds);
    void setScanInterval(int seconds);
    void addWatchEntry(const WatchEntry &entry);
    void removeWatchEntry(int index);

    // Paths
    QString configFilePath() const;
    QString dataDir() const;

    // First launch check
    bool isFirstLaunch() const { return m_firstLaunch; }

signals:
    void configChanged();

private:
    void ensureDefaults();

    // General
    QString m_githubUser;
    int m_batchInterval = 60;
    int m_scanInterval = 30;      // filesystem scan interval (10–300s)
    TrayStyle m_trayStyle = TrayStyle::Dot;

    // Repo
    QString m_repoUrl;
    QString m_repoLocalPath;
    QString m_repoBranch = "main";
    QString m_authMethod = "https";

    // Watches and channels
    QList<WatchEntry> m_watchEntries;
    QList<NotifyChannel> m_channels;

    bool m_firstLaunch = false;
};
