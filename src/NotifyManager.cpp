#include "NotifyManager.h"
#include "ConfigManager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QEventLoop>

NotifyManager::NotifyManager(ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

void NotifyManager::notify(const QString &channelName, const QString &message)
{
    const NotifyChannel *ch = findChannel(channelName);
    if (!ch) {
        // Fall back to issue #1 with default emoji
        qDebug() << "Channel" << channelName << "not found, using issue #1";
        notifyIssue(1, QString(), message);
        return;
    }

    notifyIssue(ch->issue, ch->emoji, message);
}

void NotifyManager::notifyIssue(int issueNumber, const QString &emoji, const QString &message)
{
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

    // Build the comment body with @mention
    QString prefix = emoji.isEmpty() ? QString() : emoji + " ";
    QString body = QString("@%1 %2%3")
        .arg(m_config.githubUser(), prefix, message);

    // POST to GitHub Issues API
    QUrl url(QString("https://api.github.com/repos/%1/%2/issues/%3/comments")
        .arg(info.owner, info.repo)
        .arg(issueNumber));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("token %1").arg(pat).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "fangit");

    QJsonObject json;
    json["body"] = body;

    QNetworkReply *reply = m_network.post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, issueNumber]() {
        reply->deleteLater();

        bool success = (reply->error() == QNetworkReply::NoError);
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (!success || httpStatus < 200 || httpStatus >= 300) {
            QString err = QString("Notify failed (HTTP %1): %2")
                .arg(httpStatus)
                .arg(QString::fromUtf8(reply->readAll()));
            qWarning() << err;
            emit notifyError(err);
            emit notifySent(QString::number(issueNumber), false);
        } else {
            qDebug() << "Notification posted to issue" << issueNumber;
            emit notifySent(QString::number(issueNumber), true);
        }
    });
}

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
    // This piggybacks on the credential the user already configured for git
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
