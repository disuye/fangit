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
#include "TrayManager.h"

#define VERSION_STR "0.0.1"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("fangit");
    app.setApplicationVersion(VERSION_STR);
    app.setOrganizationName("disuye");
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
    CommitBatcher commitBatcher(gitManager, configManager);
    WorkflowManager workflowManager(gitManager, configManager);

    // Connect watcher to batcher
    QObject::connect(&watcherManager, &WatcherManager::filesChanged,
                     &commitBatcher, &CommitBatcher::enqueueFiles);

    // System tray (owns the UI lifecycle)
    TrayManager trayManager(configManager, gitManager, watcherManager,
                            commitBatcher, workflowManager);
    trayManager.show();

    // Start watching configured directories
    watcherManager.startWatching(configManager.watchEntries());

    return app.exec();
}
