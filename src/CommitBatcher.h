#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QStringList>

class GitManager;
class ConfigManager;

struct PendingBatch {
    QString watchName;
    QString emoji;
    QStringList files;
};

class CommitBatcher : public QObject
{
    Q_OBJECT

public:
    explicit CommitBatcher(GitManager &git, ConfigManager &config, QObject *parent = nullptr);

    int pendingCount() const;
    void pushNow(); // force immediate commit+push

public slots:
    void enqueueFiles(const QString &watchName, const QString &emoji, const QStringList &files);

signals:
    void batchStarted();
    void batchFinished(bool success);
    void pendingCountChanged(int count);

private slots:
    void onTimerFired();

private:
    QString formatCommitMessage(const PendingBatch &batch) const;
    void copyFilesToRepo(const PendingBatch &batch);

    GitManager &m_git;
    ConfigManager &m_config;
    QTimer m_debounceTimer;

    // watchName -> pending batch
    QMap<QString, PendingBatch> m_pending;
};
