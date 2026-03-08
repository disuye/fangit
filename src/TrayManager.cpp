#include "TrayManager.h"
#include "ConfigManager.h"
#include "GitManager.h"
#include "WatcherManager.h"
#include "CommitBatcher.h"
#include "WorkflowManager.h"

#include "NotifyManager.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>

TrayManager::TrayManager(ConfigManager &config, GitManager &git,
                         WatcherManager &watcher, CommitBatcher &batcher,
                         WorkflowManager &workflow, NotifyManager &notify,
                         QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_git(git)
    , m_watcher(watcher)
    , m_batcher(batcher)
    , m_workflow(workflow)
    , m_notify(notify)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    buildMenu();
    m_trayIcon->setContextMenu(m_menu);
    setState(TrayState::Idle);

    // Connect signals
    connect(&m_batcher, &CommitBatcher::pendingCountChanged, this, [this](int count) {
        if (count > 0)
            setState(TrayState::Pending);
        else if (m_state == TrayState::Pending)
            setState(TrayState::Idle);
        updateStatus();
    });

    connect(&m_git, &GitManager::pushStarted, this, [this]() {
        setState(TrayState::Pushing);
    });

    connect(&m_git, &GitManager::pushFinished, this, [this](bool success) {
        if (success) {
            m_lastPushTime = QDateTime::currentDateTime();
            setState(TrayState::Idle);
        } else {
            setState(TrayState::Error);
        }
        updateStatus();
    });

    connect(&m_git, &GitManager::errorOccurred, this, [this](const QString &error) {
        setState(TrayState::Error);
        m_trayIcon->showMessage("fangit", error, QSystemTrayIcon::Critical, 5000);
    });

    // Update status display every 30 seconds
    m_statusTimer.setInterval(30000);
    connect(&m_statusTimer, &QTimer::timeout, this, &TrayManager::updateStatus);
    m_statusTimer.start();
}

TrayManager::~TrayManager()
{
    delete m_menu;
}

void TrayManager::show()
{
    m_trayIcon->show();
    qDebug() << "Tray icon shown";
}

void TrayManager::buildMenu()
{
    m_menu->clear();

    // Status section
    m_statusAction = m_menu->addAction("Status: Idle");
    m_statusAction->setEnabled(false);

    m_lastPushAction = m_menu->addAction("Last push: never");
    m_lastPushAction->setEnabled(false);

    m_menu->addSeparator();

    // Watch directories section
    QMenu *watchMenu = m_menu->addMenu("Watch Directories");
    for (const auto &entry : m_config.watchEntries()) {
        QString label = entry.emoji + " " + entry.pathName + "  " + entry.path;
        QAction *action = watchMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(true);
        action->setEnabled(false); // display only for now
    }
    if (m_config.watchEntries().isEmpty()) {
        QAction *empty = watchMenu->addAction("No directories configured");
        empty->setEnabled(false);
    }

    m_menu->addSeparator();

    // Actions
    QAction *pushNow = m_menu->addAction("Push now");
    connect(pushNow, &QAction::triggered, this, [this]() { onPushNow(); });

    m_pauseAction = m_menu->addAction("Pause watching");
    connect(m_pauseAction, &QAction::triggered, this, [this]() { onPauseToggle(); });

    m_menu->addSeparator();

    // Notify channels
    const auto &channels = m_config.channels();
    if (!channels.isEmpty()) {
        QMenu *notifyMenu = m_menu->addMenu("Notify");
        for (const auto &ch : channels) {
            QString label = ch.emoji + " " + ch.pathName + " (issue #" + QString::number(ch.issue) + ")";
            QAction *action = notifyMenu->addAction(label);
            connect(action, &QAction::triggered, this, [this, pathName = ch.pathName]() {
                m_notify.notify(pathName, "Manual test from fangit tray");
            });
        }
        m_menu->addSeparator();
    }

    QAction *openGH = m_menu->addAction("View on GitHub");
    connect(openGH, &QAction::triggered, this, [this]() { onOpenGitHub(); });

    QAction *openConfig = m_menu->addAction("Open config.toml");
    connect(openConfig, &QAction::triggered, this, [this]() { onOpenConfig(); });

    m_menu->addSeparator();

    QAction *quit = m_menu->addAction("Quit");
    connect(quit, &QAction::triggered, this, [this]() { onQuit(); });
}

void TrayManager::setState(TrayState state)
{
    m_state = state;
    m_trayIcon->setIcon(stateIcon(state));

    QString tooltip;
    switch (state) {
        case TrayState::Idle:    tooltip = "fangit — watching"; break;
        case TrayState::Pending: tooltip = "fangit — changes pending"; break;
        case TrayState::Pushing: tooltip = "fangit — pushing..."; break;
        case TrayState::Error:   tooltip = "fangit — error"; break;
    }
    m_trayIcon->setToolTip(tooltip);
}

QIcon TrayManager::stateIcon(TrayState state) const
{
    // Create simple colored circle icons for each state
    int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color;
    switch (state) {
        case TrayState::Idle:    color = QColor(76, 175, 80);   break; // green
        case TrayState::Pending: color = QColor(255, 193, 7);   break; // amber
        case TrayState::Pushing: color = QColor(33, 150, 243);  break; // blue
        case TrayState::Error:   color = QColor(244, 67, 54);   break; // red
    }

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, size - 4, size - 4);

    return QIcon(pix);
}

void TrayManager::updateStatus()
{
    int pending = m_batcher.pendingCount();

    QString statusText = "Status: ";
    switch (m_state) {
        case TrayState::Idle:
            statusText += pending > 0
                ? QString("Watching (%1 files pending)").arg(pending)
                : "Watching";
            break;
        case TrayState::Pending:
            statusText += QString("%1 files pending").arg(pending);
            break;
        case TrayState::Pushing:
            statusText += "Pushing...";
            break;
        case TrayState::Error:
            statusText += "Error — " + m_git.lastError();
            break;
    }
    m_statusAction->setText(statusText);

    // Last push time
    if (m_lastPushTime.isValid()) {
        qint64 secsAgo = m_lastPushTime.secsTo(QDateTime::currentDateTime());
        QString timeStr;
        if (secsAgo < 60)
            timeStr = "just now";
        else if (secsAgo < 3600)
            timeStr = QString("%1 min ago").arg(secsAgo / 60);
        else
            timeStr = QString("%1 hr ago").arg(secsAgo / 3600);
        m_lastPushAction->setText("Last push: " + timeStr);
    }
}

void TrayManager::onPushNow()
{
    m_batcher.pushNow();
}

void TrayManager::onPauseToggle()
{
    if (m_watcher.isPaused()) {
        m_watcher.resume();
        m_pauseAction->setText("Pause watching");
    } else {
        m_watcher.pause();
        m_pauseAction->setText("Resume watching");
    }
}

void TrayManager::onOpenGitHub()
{
    QString url = m_config.repoUrl();
    // Convert git URL to browser URL
    url.replace("git@github.com:", "https://github.com/");
    if (url.endsWith(".git"))
        url.chop(4);
    QDesktopServices::openUrl(QUrl(url));
}

void TrayManager::onOpenConfig()
{
    QString configPath = m_config.configFilePath();

    // Create default config if it doesn't exist
    if (!QFileInfo::exists(configPath)) {
        QDir().mkpath(QFileInfo(configPath).absolutePath());
        QFile file(configPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# fangit configuration\n"
                << "\n"
                << "[general]\n"
                << "# GitHub username to @mention in push notifications\n"
                << "github_user = \"\"\n"
                << "\n"
                << "# Debounce interval in seconds before committing (30-300)\n"
                << "batch_interval = 60\n"
                << "\n"
                << "[repo]\n"
                << "# Remote repository URL (HTTPS or SSH)\n"
                << "url = \"\"\n"
                << "\n"
                << "# Default branch\n"
                << "branch = \"main\"\n"
                << "\n"
                << "# Auth method: \"https\" or \"ssh\"\n"
                << "auth = \"https\"\n"
                << "\n"
                << "# Watch directories — add one [[watch]] block per folder\n"
                << "# [[watch]]\n"
                << "# name = \"MyFolder\"\n"
                << "# path = \"~/path/to/folder\"\n"
                << "# emoji = \"📁\"\n"
                << "# extensions = [\"txt\", \"json\"]  # optional filter\n";
            file.close();
        }
    }

    // Reveal in Finder/file manager
#ifdef Q_OS_MACOS
    QProcess::startDetached("open", {"-R", configPath});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(configPath).absolutePath()));
#endif
}

void TrayManager::onQuit()
{
    m_watcher.stopWatching();
    QApplication::quit();
}
