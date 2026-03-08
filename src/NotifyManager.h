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

    // How to deliver the notification
    // "dispatch" = trigger GitHub Action which posts as github-actions[bot] (single account)
    // "direct"   = POST issue comment directly (faster, needs bot account for notifications)
    enum Mode { Dispatch, Direct } mode = Dispatch;

    // What to do alongside the notification
    // "notify"       = notification only
    // "notify+push"  = notification + push push_dir to repo
    enum Action { NotifyOnly, NotifyAndPush } action = NotifyOnly;
    QString pushDir;  // only used when action == NotifyAndPush
};

class NotifyManager : public QObject
{
    Q_OBJECT

public:
    explicit NotifyManager(ConfigManager &config, QObject *parent = nullptr);

    // Post a message to a named channel
    void notify(const QString &channelName, const QString &message);

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

    // Two delivery routes
    void notifyViaDispatch(const RepoInfo &info, const QString &pat,
                           const NotifyChannel &channel, const QString &message);
    void notifyViaDirect(const RepoInfo &info, const QString &pat,
                         const NotifyChannel &channel, const QString &message);

    RepoInfo parseRepoUrl() const;
    QString findPat() const;
    const NotifyChannel *findChannel(const QString &name) const;

    ConfigManager &m_config;
    QNetworkAccessManager m_network;
};
