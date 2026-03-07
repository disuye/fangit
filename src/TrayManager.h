#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDateTime>

class ConfigManager;
class GitManager;
class WatcherManager;
class CommitBatcher;
class WorkflowManager;

class TrayManager : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(ConfigManager &config, GitManager &git,
                         WatcherManager &watcher, CommitBatcher &batcher,
                         WorkflowManager &workflow, QObject *parent = nullptr);
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
    void onQuit();
    void updateStatus();

private:
    void buildMenu();
    void setState(TrayState state);
    QIcon stateIcon(TrayState state) const;

    ConfigManager &m_config;
    GitManager &m_git;
    WatcherManager &m_watcher;
    CommitBatcher &m_batcher;
    WorkflowManager &m_workflow;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;

    // Menu items that need updating
    QAction *m_statusAction;
    QAction *m_lastPushAction;
    QAction *m_pauseAction;

    TrayState m_state = TrayState::Idle;
    QTimer m_statusTimer;
    QDateTime m_lastPushTime;
};
