#include "WorkflowManager.h"
#include "GitManager.h"
#include "ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QUrl>

WorkflowManager::WorkflowManager(GitManager &git, ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_git(git)
    , m_config(config)
{
}

bool WorkflowManager::ensureWorkflowExists()
{
    QString repoPath = m_config.repoLocalPath();
    QString workflowDir = repoPath + "/.github/workflows";
    QString workflowPath = workflowDir + "/notify.yml";

    if (QFileInfo::exists(workflowPath)) {
        qDebug() << "Workflow already exists at" << workflowPath;
        return true;
    }

    // Create workflow directory and file
    QDir().mkpath(workflowDir);

    QFile file(workflowPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString err = "Cannot create workflow file: " + workflowPath;
        qWarning() << err;
        emit setupError(err);
        return false;
    }

    QTextStream out(&file);
    out << workflowYaml();
    file.close();

    // Stage, commit, and push the workflow
    if (!m_git.add({".github/workflows/notify.yml"})) {
        emit setupError("Failed to stage workflow file");
        return false;
    }

    if (!m_git.commit("Add notification workflow")) {
        emit setupError("Failed to commit workflow file");
        return false;
    }

    if (!m_git.push()) {
        emit setupError("Failed to push workflow file");
        return false;
    }

    qDebug() << "Workflow created and pushed";
    return true;
}

bool WorkflowManager::ensureNotificationIssue()
{
    // Parse owner/repo from URL
    // Supports: https://github.com/owner/repo.git or git@github.com:owner/repo.git
    QString url = m_config.repoUrl();
    QString owner, repo;

    if (url.contains("github.com")) {
        QStringList parts;
        if (url.startsWith("https://")) {
            parts = url.mid(QString("https://github.com/").length()).split('/');
        } else if (url.contains("github.com:")) {
            parts = url.mid(url.indexOf(':') + 1).split('/');
        }
        if (parts.size() >= 2) {
            owner = parts[0];
            repo = parts[1];
            if (repo.endsWith(".git"))
                repo.chop(4);
        }
    }

    if (owner.isEmpty() || repo.isEmpty()) {
        emit setupError("Cannot parse owner/repo from URL: " + url);
        return false;
    }

    // Check if issue #1 exists, create if not
    // This requires a PAT — for now we'll attempt it via the GitHub API
    // TODO: Retrieve PAT from system keychain
    qDebug() << "Issue creation for" << owner << "/" << repo << "— not yet implemented (needs PAT from keychain)";
    return true;
}

QString WorkflowManager::workflowYaml() const
{
    QString user = m_config.githubUser();
    if (user.isEmpty()) user = "GITHUB_USER";

    return QString(
        "name: Notify on push\n"
        "\n"
        "on:\n"
        "  push:\n"
        "    branches: [%1]\n"
        "\n"
        "jobs:\n"
        "  notify:\n"
        "    runs-on: ubuntu-latest\n"
        "    permissions:\n"
        "      issues: write\n"
        "    steps:\n"
        "      - name: Post issue comment\n"
        "        uses: actions/github-script@v7\n"
        "        with:\n"
        "          script: |\n"
        "            const username = '%2';\n"
        "            const issueNumber = 1;\n"
        "\n"
        "            const msg = context.payload.head_commit.message;\n"
        "            const sha = context.payload.head_commit.id.substring(0, 7);\n"
        "            const ts = context.payload.head_commit.timestamp;\n"
        "            const author = context.payload.head_commit.author.username || 'unknown';\n"
        "\n"
        "            await github.rest.issues.createComment({\n"
        "              owner: context.repo.owner,\n"
        "              repo: context.repo.repo,\n"
        "              issue_number: issueNumber,\n"
        "              body: `@${username} \\`${sha}\\` by ${author}: ${msg}\\n<sub>${ts}</sub>`\n"
        "            });\n"
    ).arg(m_config.repoBranch(), user);
}
