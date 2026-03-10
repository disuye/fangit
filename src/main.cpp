#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QSystemTrayIcon>
#include <QMessageBox>

#include "ConfigManager.h"
#include "GitManager.h"
#include "WatcherManager.h"
#include "CommitBatcher.h"
#include "WorkflowManager.h"
#include "NotifyManager.h"
#include "TrayManager.h"

#define VERSION_STR "0.0.8"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("fangit");
    app.setApplicationVersion(VERSION_STR);
    app.setOrganizationDomain("com.disuye");
    app.setQuitOnLastWindowClosed(false);

    // Load Fira Code from resources and set as application font
    int fontId = QFontDatabase::addApplicationFont(":/fonts/FiraCode-Variable.ttf");
    if (fontId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont firaCode(families.first(), 13);
            app.setFont(firaCode);
        }
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "fangit", "System tray not available on this system.");
        return 1;
    }

    // Core components
    ConfigManager configManager;
    GitManager gitManager(configManager);
    WatcherManager watcherManager;
    NotifyManager notifyManager(configManager);
    CommitBatcher commitBatcher(gitManager, configManager, notifyManager);
    WorkflowManager workflowManager(gitManager, configManager);

    // Ensure repo is cloned (needed for watch/sync mode AND dispatch notify mode)
    bool needsRepo = !configManager.watchEntries().isEmpty()
                  || !configManager.channels().isEmpty();

    if (!configManager.repoUrl().isEmpty() && !gitManager.isRepoCloned() && needsRepo) {
        if (!gitManager.cloneRepo()) {
            QMessageBox::warning(nullptr, "fangit",
                "Failed to clone repository.\n\n"
                "Check your config.toml repo URL and credentials.\n\n"
                + gitManager.lastError());
        }
    }

    // Ensure workflow and notification issues exist in repo
    if (gitManager.isRepoCloned()) {
        workflowManager.ensureWorkflowExists();
    }

    // Connect watcher to batcher
    QObject::connect(&watcherManager, &WatcherManager::filesChanged,
                     &commitBatcher, &CommitBatcher::enqueueFiles);
    
    // Connect batcher to watcher, don't notify user on first folder scan; only changes
    QObject::connect(&watcherManager, &WatcherManager::initialFileOffsets,
                    &commitBatcher, &CommitBatcher::seedOffsets);

    // System tray (owns the UI lifecycle)
    TrayManager trayManager(configManager, gitManager, watcherManager,
                            commitBatcher, workflowManager, notifyManager);
    trayManager.show();

    // Start watching configured directories
    // Sync watches need the repo cloned; notify-only watches work without it
    if (!configManager.watchEntries().isEmpty()) {
        bool repoReady = gitManager.isRepoCloned();
        bool hasNotifyOnly = false;

        // Check if there are any notify-only watches that can start without repo
        for (const auto &entry : configManager.watchEntries()) {
            bool isNotify = false;
            for (const auto &ch : configManager.channels()) {
                if (ch.pathName == entry.action) {
                    isNotify = true;
                    break;
                }
            }
            if (isNotify) hasNotifyOnly = true;
        }

        if (repoReady || hasNotifyOnly) {
            watcherManager.startWatching(configManager.watchEntries(),
                                         configManager.scanInterval());
        }
    }

    return app.exec();
}
