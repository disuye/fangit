#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>

class ConfigManager;
class GitManager;
class WatcherManager;
class CommitBatcher;
class WorkflowManager;
class NotifyManager;

class TrayManager : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(ConfigManager &config, GitManager &git,
                         WatcherManager &watcher, CommitBatcher &batcher,
                         WorkflowManager &workflow, NotifyManager &notify,
                         QObject *parent = nullptr);
    ~TrayManager();

    void show();

    enum class TrayState {
        Idle,       // watching, no pending changes
        Pending,    // changes queued in debounce window
        Pushing,    // push in progress
        Error       // auth failure, network issue, etc.
    };

private slots:
    void onPushNow();
    void onPauseToggle();
    void onOpenGitHub();
    void onOpenConfig();
    void onQuit();
    void updateStatus();

private:
    void buildMenu();
    void setState(TrayState state);
    QIcon stateIcon(TrayState state) const;
    QIcon dotIcon(TrayState state) const;
    QIcon logoIcon(TrayState state) const;
    QIcon tintIcon(TrayState state) const;
    QColor stateColor(TrayState state) const;

    ConfigManager &m_config;
    GitManager &m_git;
    WatcherManager &m_watcher;
    CommitBatcher &m_batcher;
    WorkflowManager &m_workflow;
    NotifyManager &m_notify;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QPixmap m_trayTemplate;   // loaded once from resource

    // Menu items that need updating
    QAction *m_statusAction;
    QAction *m_lastPushAction;
    QAction *m_pauseAction;

    TrayState m_state = TrayState::Idle;
    QTimer m_statusTimer;
    QDateTime m_lastPushTime;
};
