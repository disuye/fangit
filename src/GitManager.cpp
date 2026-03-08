#include "GitManager.h"
#include "ConfigManager.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>

GitManager::GitManager(ConfigManager &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

QString GitManager::repoPath() const
{
    return m_config.repoLocalPath();
}

bool GitManager::isGitInstalled()
{
    auto result = runGit({"--version"});
    return result.success;
}

QString GitManager::gitVersion()
{
    auto result = runGit({"--version"});
    return result.success ? result.output.trimmed() : QString();
}

bool GitManager::isRepoCloned() const
{
    return QFileInfo::exists(repoPath() + "/.git");
}

bool GitManager::cloneRepo()
{
    if (m_config.repoUrl().isEmpty()) {
        m_lastError = "No repository URL configured";
        emit errorOccurred(m_lastError);
        return false;
    }

    emit operationStarted("clone");

    // If directory exists but isn't a git repo, remove it so clone can proceed
    QDir repoDir(repoPath());
    if (repoDir.exists() && !QFileInfo::exists(repoPath() + "/.git")) {
        qDebug() << "Removing stale repo directory:" << repoPath();
        repoDir.removeRecursively();
    }

    QDir().mkpath(QFileInfo(repoPath()).absolutePath());

    auto result = runGit({"clone", m_config.repoUrl(), repoPath()}, 120000);
    if (!result.success) {
        m_lastError = "Clone failed: " + result.error;
        emit errorOccurred(m_lastError);
    }

    emit operationFinished("clone", result.success);
    return result.success;
}

bool GitManager::add(const QStringList &files)
{
    if (files.isEmpty()) return true;

    QStringList args = {"add"};
    args.append(files);

    auto result = runGit(args);
    if (!result.success) {
        m_lastError = "git add failed: " + result.error;
        emit errorOccurred(m_lastError);
    }
    return result.success;
}

bool GitManager::commit(const QString &message)
{
    auto result = runGit({"commit", "-m", message});
    if (!result.success) {
        // "nothing to commit" can appear in stdout or stderr depending on git version
        bool nothingToCommit = result.error.contains("nothing to commit")
                            || result.output.contains("nothing to commit");
        m_lastError = "git commit failed: " + result.error + result.output;
        if (!nothingToCommit)
            emit errorOccurred(m_lastError);
    }
    return result.success;
}

bool GitManager::push()
{
    emit pushStarted();
    emit operationStarted("push");

    auto result = runGit({"push", "origin", m_config.repoBranch()}, 60000);

    if (!result.success) {
        // Try pull + push if rejected (remote ahead)
        if (result.error.contains("rejected") || result.error.contains("non-fast-forward")) {
            qDebug() << "Push rejected, attempting pull then retry...";
            if (pull()) {
                result = runGit({"push", "origin", m_config.repoBranch()}, 60000);
            }
        }

        if (!result.success) {
            m_lastError = "git push failed: " + result.error;
            emit errorOccurred(m_lastError);
        }
    }

    emit pushFinished(result.success);
    emit operationFinished("push", result.success);
    return result.success;
}

bool GitManager::pull()
{
    emit operationStarted("pull");
    auto result = runGit({"pull", "--ff-only", "origin", m_config.repoBranch()}, 60000);
    if (!result.success) {
        m_lastError = "git pull failed: " + result.error;
        emit errorOccurred(m_lastError);
    }
    emit operationFinished("pull", result.success);
    return result.success;
}

bool GitManager::fetch()
{
    auto result = runGit({"fetch", "origin"}, 60000);
    return result.success;
}

bool GitManager::hasRemoteChanges()
{
    if (!fetch()) return false;

    auto result = runGit({"rev-list", "HEAD..origin/" + m_config.repoBranch(), "--count"});
    if (!result.success) return false;

    return result.output.trimmed().toInt() > 0;
}

GitManager::GitResult GitManager::runGit(const QStringList &args, int timeoutMs)
{
    GitResult result;
    QProcess process;

    // Run in repo directory if it exists, otherwise use system git
    if (QFileInfo::exists(repoPath() + "/.git") && args.first() != "clone" && args.first() != "--version") {
        process.setWorkingDirectory(repoPath());
    }

    process.start("git", args);

    if (!process.waitForFinished(timeoutMs)) {
        result.success = false;
        result.error = "git timed out";
        process.kill();
        return result;
    }

    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.success = (process.exitCode() == 0);

    if (!result.success) {
        qDebug() << "git" << args << "failed:" << result.error << result.output;
    }

    return result;
}
