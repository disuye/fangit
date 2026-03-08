#include "ConfigManager.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include <toml.hpp>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    ensureDefaults();
    load();
}

QString ConfigManager::configFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/config.toml";
}

QString ConfigManager::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void ConfigManager::ensureDefaults()
{
    m_repoLocalPath = dataDir() + "/repo";
}

bool ConfigManager::load()
{
    QString path = configFilePath();
    if (!QFileInfo::exists(path)) {
        m_firstLaunch = true;
        qDebug() << "No config file found at" << path << "— first launch";
        return false;
    }

    try {
        auto data = toml::parse(path.toStdString());

        // [general]
        if (data.contains("general")) {
            auto &gen = data.at("general");
            if (gen.contains("github_user"))
                m_githubUser = QString::fromStdString(toml::find<std::string>(gen, "github_user"));
            if (gen.contains("batch_interval"))
                m_batchInterval = qBound(30, toml::find<int>(gen, "batch_interval"), 300);
            if (gen.contains("scan_interval"))
                m_scanInterval = qBound(10, toml::find<int>(gen, "scan_interval"), 300);
            if (gen.contains("use_icon_logo"))
                m_useIconLogo = toml::find<bool>(gen, "use_icon_logo");
        }

        // [repo]
        if (data.contains("repo")) {
            auto &repo = data.at("repo");
            if (repo.contains("url"))
                m_repoUrl = QString::fromStdString(toml::find<std::string>(repo, "url"));
            if (repo.contains("local_path"))
                m_repoLocalPath = QString::fromStdString(toml::find<std::string>(repo, "local_path"));
            if (repo.contains("branch"))
                m_repoBranch = QString::fromStdString(toml::find<std::string>(repo, "branch"));
            if (repo.contains("auth"))
                m_authMethod = QString::fromStdString(toml::find<std::string>(repo, "auth"));
        }

        // [[watch]]
        if (data.contains("watch")) {
            auto &watches = data.at("watch");
            if (watches.is_array_of_tables()) {
                for (const auto &w : watches.as_array()) {
                    WatchEntry entry;
                    if (w.contains("path_name"))
                        entry.pathName = QString::fromStdString(toml::find<std::string>(w, "path_name"));
                    if (w.contains("path")) {
                        entry.path = QString::fromStdString(toml::find<std::string>(w, "path"));
                        // Expand ~ to home dir
                        if (entry.path.startsWith("~/"))
                            entry.path = QDir::homePath() + entry.path.mid(1);
                    }
                    if (w.contains("emoji"))
                        entry.emoji = QString::fromStdString(toml::find<std::string>(w, "emoji"));
                    if (w.contains("extensions")) {
                        auto exts = toml::find<std::vector<std::string>>(w, "extensions");
                        for (const auto &e : exts)
                            entry.extensions.append(QString::fromStdString(e));
                    }
                    m_watchEntries.append(entry);
                }
            }
        }

        // [[channel]]
        if (data.contains("channel")) {
            auto &channels = data.at("channel");
            if (channels.is_array_of_tables()) {
                for (const auto &c : channels.as_array()) {
                    NotifyChannel ch;
                    if (c.contains("path_name"))
                        ch.pathName = QString::fromStdString(toml::find<std::string>(c, "path_name"));
                    if (c.contains("issue"))
                        ch.issue = toml::find<int>(c, "issue");
                    if (c.contains("emoji"))
                        ch.emoji = QString::fromStdString(toml::find<std::string>(c, "emoji"));
                    if (c.contains("mode")) {
                        std::string mode = toml::find<std::string>(c, "mode");
                        ch.mode = (mode == "direct")
                            ? NotifyChannel::Direct
                            : NotifyChannel::Dispatch;
                    }
                    if (c.contains("action")) {
                        std::string action = toml::find<std::string>(c, "action");
                        ch.action = (action == "notify+push")
                            ? NotifyChannel::NotifyAndPush
                            : NotifyChannel::NotifyOnly;
                    }
                    if (c.contains("push_dir")) {
                        ch.pushDir = QString::fromStdString(toml::find<std::string>(c, "push_dir"));
                        if (ch.pushDir.startsWith("~/"))
                            ch.pushDir = QDir::homePath() + ch.pushDir.mid(1);
                    }
                    m_channels.append(ch);
                }
            }
        }

        m_firstLaunch = false;
        qDebug() << "Config loaded:" << m_watchEntries.size() << "watch entries,"
                 << m_channels.size() << "channels";
        return true;

    } catch (const std::exception &e) {
        qWarning() << "Failed to parse config:" << e.what();
        return false;
    }
}

bool ConfigManager::save()
{
    // Build TOML data and write to file
    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot write config to" << path;
        return false;
    }

    QTextStream out(&file);
    out << "# fangit configuration\n\n";
    out << "[general]\n";
    out << "github_user = \"" << m_githubUser << "\"\n";
    out << "batch_interval = " << m_batchInterval << "  # seconds before committing (30-300)\n";
    out << "scan_interval = " << m_scanInterval << "   # filesystem scan interval (10-300)\n";
    out << "use_icon_logo = " << (m_useIconLogo ? "true" : "false") << "  # true = app icon, false = dot indicator\n";
    out << "\n";
    out << "[repo]\n";
    out << "url = \"" << m_repoUrl << "\"\n";
    out << "local_path = \"" << m_repoLocalPath << "\"\n";
    out << "branch = \"" << m_repoBranch << "\"\n";
    out << "auth = \"" << m_authMethod << "\"\n";

    for (const auto &w : m_watchEntries) {
        out << "\n[[watch]]\n";
        out << "path_name = \"" << w.pathName << "\"\n";
        out << "path = \"" << w.path << "\"\n";
        out << "emoji = \"" << w.emoji << "\"\n";
        if (!w.extensions.isEmpty()) {
            out << "extensions = [";
            for (int i = 0; i < w.extensions.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << w.extensions[i] << "\"";
            }
            out << "]\n";
        }
    }

    for (const auto &ch : m_channels) {
        out << "\n[[channel]]\n";
        out << "path_name = \"" << ch.pathName << "\"\n";
        out << "issue = " << ch.issue << "\n";
        out << "emoji = \"" << ch.emoji << "\"\n";
        out << "mode = \"" << (ch.mode == NotifyChannel::Direct ? "direct" : "dispatch") << "\"\n";
        out << "action = \"" << (ch.action == NotifyChannel::NotifyAndPush ? "notify+push" : "notify") << "\"\n";
        if (!ch.pushDir.isEmpty())
            out << "push_dir = \"" << ch.pushDir << "\"\n";
    }

    file.close();
    qDebug() << "Config saved to" << path;
    return true;
}

void ConfigManager::setGithubUser(const QString &user)
{
    m_githubUser = user;
    emit configChanged();
}

void ConfigManager::setRepoUrl(const QString &url)
{
    m_repoUrl = url;
    emit configChanged();
}

void ConfigManager::setRepoBranch(const QString &branch)
{
    m_repoBranch = branch;
    emit configChanged();
}

void ConfigManager::setBatchInterval(int seconds)
{
    m_batchInterval = qBound(30, seconds, 300);
    emit configChanged();
}

void ConfigManager::setScanInterval(int seconds)
{
    m_scanInterval = qBound(10, seconds, 300);
    emit configChanged();
}

void ConfigManager::addWatchEntry(const WatchEntry &entry)
{
    m_watchEntries.append(entry);
    emit configChanged();
}

void ConfigManager::removeWatchEntry(int index)
{
    if (index >= 0 && index < m_watchEntries.size()) {
        m_watchEntries.removeAt(index);
        emit configChanged();
    }
}
