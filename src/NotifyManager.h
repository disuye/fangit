#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class ConfigManager;

// Channel definition — maps a named channel to a GitHub issue number
struct NotifyChannel {
    QString pathName;   // required — unique ID for this channel
    int issue = 1;
    QString emoji;
    enum Action { NotifyOnly, NotifyAndPush } action = NotifyOnly;
    QString pushDir;  // only used when action == NotifyAndPush
};

class NotifyManager : public QObject
{
    Q_OBJECT

public:
    explicit NotifyManager(ConfigManager &config, QObject *parent = nullptr);

    // Post a message to a named channel (issues API, no git involved)
    void notify(const QString &channelName, const QString &message);

    // Post directly to a specific issue number
    void notifyIssue(int issueNumber, const QString &emoji, const QString &message);

    // Check if a channel triggers a push alongside notification
    bool channelRequiresPush(const QString &channelName) const;
    QString channelPushDir(const QString &channelName) const;

signals:
    void notifySent(const QString &channel, bool success);
    void notifyError(const QString &error);

private:
    struct RepoInfo {
        QString owner;
        QString repo;
        bool valid = false;
    };

    RepoInfo parseRepoUrl() const;
    QString findPat() const;
    const NotifyChannel *findChannel(const QString &name) const;

    ConfigManager &m_config;
    QNetworkAccessManager m_network;
};
