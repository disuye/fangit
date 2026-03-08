#include "NotifyManager.h"
#include "ConfigManager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QProcess>
#include <QDebug>

NotifyManager::NotifyManager(ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

void NotifyManager::notify(const QString &channelName, const QString &message)
{
    const NotifyChannel *ch = findChannel(channelName);
    if (!ch) {
        qWarning() << "Channel" << channelName << "not found";
        emit notifyError("Channel not found: " + channelName);
        return;
    }

    RepoInfo info = parseRepoUrl();
    if (!info.valid) {
        emit notifyError("Cannot parse repo owner/name from URL");
        return;
    }

    QString pat = findPat();
    if (pat.isEmpty()) {
        emit notifyError("No GitHub PAT found — needed for notify mode");
        return;
    }

    qDebug() << "Notify:" << channelName << "via"
             << (ch->mode == NotifyChannel::Dispatch ? "dispatch" : "direct")
             << "→ issue #" << ch->issue;

    if (ch->mode == NotifyChannel::Direct) {
        notifyViaDirect(info, pat, *ch, message);
    } else {
        notifyViaDispatch(info, pat, *ch, message);
    }
}

// ── Dispatch mode ──────────────────────────────────────────────────────────
// Triggers a GitHub Action via workflow_dispatch API.
// The Action runs as github-actions[bot] and posts the issue comment,
// which generates a notification even for the repo owner (single account).

void NotifyManager::notifyViaDispatch(const RepoInfo &info, const QString &pat,
                                       const NotifyChannel &channel, const QString &message)
{
    QString prefix = channel.emoji.isEmpty() ? QString() : channel.emoji + " ";

    QUrl url(QString("https://api.github.com/repos/%1/%2/actions/workflows/notify.yml/dispatches")
        .arg(info.owner, info.repo));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("token %1").arg(pat).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "fangit");

    QJsonObject inputs;
    inputs["channel"] = channel.pathName;
    inputs["issue"] = QString::number(channel.issue);
    inputs["message"] = prefix + message;
    inputs["github_user"] = m_config.githubUser();

    QJsonObject json;
    json["ref"] = m_config.repoBranch();
    json["inputs"] = inputs;

    QNetworkReply *reply = m_network.post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, channel]() {
        reply->deleteLater();

        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // workflow_dispatch returns 204 No Content on success
        if (httpStatus == 204) {
            qDebug() << "Dispatch triggered for channel" << channel.pathName
                     << "→ issue #" << channel.issue;
            emit notifySent(channel.pathName, true);
        } else {
            QString err = QString("Dispatch failed (HTTP %1): %2")
                .arg(httpStatus)
                .arg(QString::fromUtf8(reply->readAll()));
            qWarning() << err;
            emit notifyError(err);
            emit notifySent(channel.pathName, false);
        }
    });
}

// ── Direct mode ────────────────────────────────────────────────────────────
// Posts an issue comment directly via the GitHub API.
// Faster (~20s) but requires a second GitHub account for notifications,
// since GitHub suppresses self-notifications.

void NotifyManager::notifyViaDirect(const RepoInfo &info, const QString &pat,
                                     const NotifyChannel &channel, const QString &message)
{
    QString prefix = channel.emoji.isEmpty() ? QString() : channel.emoji + " ";
    QString body = QString("@%1 %2%3")
        .arg(m_config.githubUser(), prefix, message);

    QUrl url(QString("https://api.github.com/repos/%1/%2/issues/%3/comments")
        .arg(info.owner, info.repo)
        .arg(channel.issue));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("token %1").arg(pat).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "fangit");

    QJsonObject json;
    json["body"] = body;

    QNetworkReply *reply = m_network.post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, channel]() {
        reply->deleteLater();

        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (httpStatus >= 200 && httpStatus < 300) {
            qDebug() << "Direct notification posted to issue #" << channel.issue;
            emit notifySent(channel.pathName, true);
        } else {
            QString err = QString("Direct notify failed (HTTP %1): %2")
                .arg(httpStatus)
                .arg(QString::fromUtf8(reply->readAll()));
            qWarning() << err;
            emit notifyError(err);
            emit notifySent(channel.pathName, false);
        }
    });
}

// ── Helpers ────────────────────────────────────────────────────────────────

bool NotifyManager::channelRequiresPush(const QString &channelName) const
{
    const NotifyChannel *ch = findChannel(channelName);
    return ch && ch->action == NotifyChannel::NotifyAndPush;
}

QString NotifyManager::channelPushDir(const QString &channelName) const
{
    const NotifyChannel *ch = findChannel(channelName);
    return ch ? ch->pushDir : QString();
}

NotifyManager::RepoInfo NotifyManager::parseRepoUrl() const
{
    RepoInfo info;
    QString url = m_config.repoUrl();

    if (!url.contains("github.com"))
        return info;

    QStringList parts;
    if (url.startsWith("https://")) {
        parts = url.mid(QString("https://github.com/").length()).split('/');
    } else if (url.contains("github.com:")) {
        parts = url.mid(url.indexOf(':') + 1).split('/');
    }

    if (parts.size() >= 2) {
        info.owner = parts[0];
        info.repo = parts[1];
        if (info.repo.endsWith(".git"))
            info.repo.chop(4);
        info.valid = true;
    }

    return info;
}

QString NotifyManager::findPat() const
{
    // Try to get PAT from git's credential helper (same one used for pushes)
    QProcess process;
    process.start("git", {"credential", "fill"});
    if (!process.waitForStarted(5000))
        return QString();

    process.write("protocol=https\nhost=github.com\n\n");
    process.closeWriteChannel();

    if (!process.waitForFinished(5000))
        return QString();

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    for (const QString &line : output.split('\n')) {
        if (line.startsWith("password="))
            return line.mid(9).trimmed();
    }

    return QString();
}

const NotifyChannel *NotifyManager::findChannel(const QString &name) const
{
    const auto &channels = m_config.channels();
    for (const auto &ch : channels) {
        if (ch.pathName == name)
            return &ch;
    }
    return nullptr;
}
