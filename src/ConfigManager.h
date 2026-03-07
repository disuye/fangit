#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDir>
#include <QStandardPaths>

struct WatchEntry {
    QString name;
    QString path;
    QString emoji;
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

    // Accessors
    QString githubUser() const { return m_githubUser; }
    int batchInterval() const { return m_batchInterval; }
    QString repoUrl() const { return m_repoUrl; }
    QString repoLocalPath() const { return m_repoLocalPath; }
    QString repoBranch() const { return m_repoBranch; }
    QString authMethod() const { return m_authMethod; }
    QList<WatchEntry> watchEntries() const { return m_watchEntries; }

    // Mutators
    void setGithubUser(const QString &user);
    void setRepoUrl(const QString &url);
    void setRepoBranch(const QString &branch);
    void setBatchInterval(int seconds);
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

    QString m_githubUser;
    int m_batchInterval = 60;
    QString m_repoUrl;
    QString m_repoLocalPath;
    QString m_repoBranch = "main";
    QString m_authMethod = "https";
    QList<WatchEntry> m_watchEntries;
    bool m_firstLaunch = false;
};
