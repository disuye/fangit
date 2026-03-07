#pragma once

#include <QObject>
#include <QString>

class GitManager;
class ConfigManager;

class WorkflowManager : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowManager(GitManager &git, ConfigManager &config, QObject *parent = nullptr);

    // Ensure the GitHub Actions workflow file exists in the repo
    bool ensureWorkflowExists();

    // Create Issue #1 via GitHub API (requires PAT)
    bool ensureNotificationIssue();

signals:
    void setupComplete(bool success);
    void setupError(const QString &error);

private:
    QString workflowYaml() const;

    GitManager &m_git;
    ConfigManager &m_config;
};
