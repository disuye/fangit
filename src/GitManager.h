#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

class ConfigManager;

class GitManager : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(ConfigManager &config, QObject *parent = nullptr);

    // Git availability
    bool isGitInstalled();
    QString gitVersion();

    // Repo operations
    bool cloneRepo();
    bool isRepoCloned() const;

    // Core operations
    bool add(const QStringList &files);
    bool commit(const QString &message);
    bool push();
    bool pull();
    bool fetch();

    // Status
    bool hasRemoteChanges();
    QString lastError() const { return m_lastError; }

signals:
    void operationStarted(const QString &op);
    void operationFinished(const QString &op, bool success);
    void pushStarted();
    void pushFinished(bool success);
    void errorOccurred(const QString &error);

private:
    struct GitResult {
        bool success;
        QString output;
        QString error;
    };

    GitResult runGit(const QStringList &args, int timeoutMs = 30000);
    QString repoPath() const;

    ConfigManager &m_config;
    QString m_lastError;
};
