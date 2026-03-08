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
    // Load tray icon template from resources
    m_trayTemplate = QPixmap(":/images/TrayIcon.png");
    if (m_trayTemplate.isNull())
        qWarning() << "Failed to load TrayIcon.png from resources";

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
    if (m_config.useIconLogo() && !m_trayTemplate.isNull())
        return logoIcon(state);
    return dotIcon(state);
}

QColor TrayManager::stateColor(TrayState state) const
{
    switch (state) {
        case TrayState::Idle:    return QColor(76, 175, 80);    // green
        case TrayState::Pending: return QColor(255, 193, 7);    // amber
        case TrayState::Pushing: return QColor(33, 150, 243);   // blue
        case TrayState::Error:   return QColor(244, 67, 54);    // red
    }
    return QColor(128, 128, 128);
}

// Simple colored dot — compact, works on all platforms
QIcon TrayManager::dotIcon(TrayState state) const
{
    int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(stateColor(state));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, size - 4, size - 4);

    return QIcon(pix);
}

// App icon with colored status dot in bottom-right corner
QIcon TrayManager::logoIcon(TrayState state) const
{
    // Scale template to tray size (22pt, 44px for @2x)
    int size = 44;
    QPixmap icon = m_trayTemplate.scaled(size, size,
        Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // On macOS, we want a template image (white silhouette) that the OS
    // adapts for dark/light mode. The status dot is composited on top.
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw status dot — bottom-right corner
    int dotSize = 12;
    int dotX = size - dotSize - 1;
    int dotY = size - dotSize - 1;

    // Dark outline for contrast against any background
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(dotX - 1, dotY - 1, dotSize + 2, dotSize + 2);

    // Colored dot
    painter.setBrush(stateColor(state));
    painter.drawEllipse(dotX, dotY, dotSize, dotSize);

    painter.end();

#ifdef Q_OS_MACOS
    // Set as template image so macOS handles dark/light mode for the icon body.
    // Note: the colored dot won't be affected by template rendering since
    // macOS only templates fully opaque white regions.
    // This requires accessing the native NSImage — for now we skip setTemplate
    // and let the white PNG work naturally in both modes.
    // TODO: Use Objective-C bridge to call [nsImage setTemplate:YES]
#endif

    return QIcon(icon);
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

    if (!QFileInfo::exists(configPath)) {
        QDir().mkpath(QFileInfo(configPath).absolutePath());
        QFile defaultConfig(":/config/default.toml");
        if (defaultConfig.open(QIODevice::ReadOnly)) {
            QFile out(configPath);
            if (out.open(QIODevice::WriteOnly))
                out.write(defaultConfig.readAll());
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
