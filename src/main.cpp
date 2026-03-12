#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QFileInfo>

#include "ConfigManager.h"
#include "GitManager.h"
#include "WatcherManager.h"
#include "CommitBatcher.h"
#include "WorkflowManager.h"
#include "NotifyManager.h"
#include "TrayManager.h"
#include "SetupWizard.h"

#define VERSION_STR "0.0.9"

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

    // ── First launch: run setup wizard ─────────────────────────────
    // Wizard runs only if config.toml does not exist.
    // To re-run: move/delete config.toml and restart.
    if (configManager.isFirstLaunch()) {
        SetupWizard wizard(configManager, gitManager, workflowManager);

        if (wizard.exec() != QDialog::Accepted || !wizard.wasCompleted()) {
            // Cancelled — don't create a default config so wizard runs again next time
            if (!QFileInfo::exists(configManager.configFilePath())) {
                QMessageBox::information(nullptr, "fangit",
                    "Setup was cancelled.\n\n"
                    "fangit will start in the menu bar but won't do anything "
                    "until configured. \n\nClick the tray icon \xe2\x86\x92 'Open config.toml' "
                    "to configure manually, or restart fangit to run the wizard again.");
            }
        }

        // Reload config after wizard writes it
        configManager.load();
    }

    // ── Normal startup ─────────────────────────────────────────────

    // Ensure repo is cloned
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

    // Ensure workflow exists
    if (gitManager.isRepoCloned()) {
        workflowManager.ensureWorkflowExists();
    }

    // Connect watcher to batcher
    QObject::connect(&watcherManager, &WatcherManager::filesChanged,
                     &commitBatcher, &CommitBatcher::enqueueFiles);

    // Seed offsets so first scan doesn't fire notifications for existing files
    QObject::connect(&watcherManager, &WatcherManager::initialFileOffsets,
                    &commitBatcher, &CommitBatcher::seedOffsets);

    // System tray
    TrayManager trayManager(configManager, gitManager, watcherManager,
                            commitBatcher, workflowManager, notifyManager);
    trayManager.show();

    // Start watching
    if (!configManager.watchEntries().isEmpty()) {
        bool repoReady = gitManager.isRepoCloned();
        bool hasNotifyOnly = false;

        for (const auto &entry : configManager.watchEntries()) {
            for (const auto &ch : configManager.channels()) {
                if (ch.pathName == entry.action) {
                    hasNotifyOnly = true;
                    break;
                }
            }
            if (hasNotifyOnly) break;
        }

        if (repoReady || hasNotifyOnly) {
            watcherManager.startWatching(configManager.watchEntries(),
                                         configManager.scanInterval());
        }
    }

    return app.exec();
}
