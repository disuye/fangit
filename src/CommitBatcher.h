#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QStringList>

#include "FileScanner.h"
#include "MessageFormatter.h"

class GitManager;
class ConfigManager;
class NotifyManager;

struct PendingBatch {
    QString pathName;
    QString emoji;
    QString action;
    QString watchPath;
    QString match;
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
                      const QString &action, const QString &watchPath,
                      const QString &match, const QStringList &files);

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
    void copyFilesToRepo(const PendingBatch &batch);

    GitManager &m_git;
    ConfigManager &m_config;
    NotifyManager &m_notify;
    FileScanner m_scanner;
    MessageFormatter m_formatter;
    QTimer m_debounceTimer;

    // pathName -> pending batch
    QMap<QString, PendingBatch> m_pending;

    // pathName -> file byte offsets (persistent across batches for tail reading)
    QMap<QString, QMap<QString, qint64>> m_fileOffsets;
};
