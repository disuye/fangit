#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QStringList>

class GitManager;
class ConfigManager;
class NotifyManager;

struct PendingBatch {
    QString pathName;
    QString emoji;
    QString action;     // "sync" / empty = git, or channel path_name = notify
    QStringList files;
};

class CommitBatcher : public QObject
{
    Q_OBJECT

public:
    explicit CommitBatcher(GitManager &git, ConfigManager &config,
                           NotifyManager &notify, QObject *parent = nullptr);

    int pendingCount() const;
    void pushNow(); // force immediate commit+push

public slots:
    void enqueueFiles(const QString &pathName, const QString &emoji,
                      const QString &action, const QStringList &files);

signals:
    void batchStarted();
    void batchFinished(bool success);
    void pendingCountChanged(int count);

private slots:
    void onTimerFired();

private:
    bool isNotifyAction(const QString &action) const;
    void processSyncBatch(const PendingBatch &batch, bool &allSuccess);
    void processNotifyBatch(const PendingBatch &batch);
    QString formatCommitMessage(const PendingBatch &batch) const;
    QString formatNotifyMessage(const PendingBatch &batch) const;
    void copyFilesToRepo(const PendingBatch &batch);

    GitManager &m_git;
    ConfigManager &m_config;
    NotifyManager &m_notify;
    QTimer m_debounceTimer;

    // pathName -> pending batch
    QMap<QString, PendingBatch> m_pending;
};
