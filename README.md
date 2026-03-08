# fangit

Fang It! Fast, zero-config file watching → auto git commit → push → GitHub as the auth/notification/data layer.

A macOS menu bar app that watches local directories and automatically commits/pushes file changes to a GitHub repo. GitHub's iOS app delivers push notifications via an Actions workflow.

## How it works

```
File changed in watched directory
  -> fangit detects change (QFileSystemWatcher + periodic scan)
    -> Debounce timer collects changes (configurable, 30-300s)
      -> Files copied into local repo clone
        -> git add + commit (formatted message with emoji + file list)
          -> git push
            -> GitHub Actions workflow fires
              -> Issue comment posted @mentioning you
                -> iOS push notification via GitHub mobile app
```

## Requirements

- macOS 13+
- Qt 6.10.1 (installed at `~/Qt/6.10.1/macos`)
- CMake 3.20+
- Ninja
- git CLI

## Build

```bash
# Debug build (incremental)
./scripts/ninja.sh

# Debug build + launch
./scripts/ninja.sh run

# Clean build directory
./scripts/ninja.sh clean

# Release build (signed + notarized DMG)
./scripts/ninja-release.sh
./scripts/ninja-release.sh universal   # arm64 + x86_64
```

## Setup

1. Create a GitHub repo for monitoring (can be private)
2. Generate a fine-grained PAT: GitHub > Settings > Developer Settings > Personal Access Tokens
   - Scope: Contents (Read & Write), Issues (Read & Write)
   - Repository: select your monitoring repo only
3. Create the config file at `~/Library/Application Support/fangit/config.toml` (or launch the app and click "Open config.toml" from the menu bar)
4. Create your watch directories (the folders fangit monitors for changes)
5. Launch fangit — it clones the repo and starts watching

## Configuration

Config lives at `~/Library/Application Support/fangit/config.toml`:

```toml
[general]
github_user = "yourname"
batch_interval = 60          # seconds before committing (30-300)

[repo]
url = "https://github.com/yourname/yourrepo.git"
branch = "main"
auth = "https"               # "https" or "ssh"

[[watch]]
name = "SMPLR"
path = "~/fangit/SMPLR"
emoji = "🎵"
# extensions = ["txt", "json"]  # optional filter

[[watch]]
name = "Scripts"
path = "~/fangit/Scripts"
emoji = "📝"
```

Watched directories map to repo subdirectories:

```
~/fangit/SMPLR/recipe_001.txt  ->  repo/SMPLR/recipe_001.txt
~/fangit/Scripts/output.log    ->  repo/Scripts/output.log
```

## Menu bar

- **Status** — current state (watching, pending, pushing, error)
- **Last push** — time since last successful push
- **Watch Directories** — list of monitored folders
- **Push now** — force immediate commit + push
- **Pause/Resume watching**
- **View on GitHub** — opens repo in browser
- **Open config.toml** — reveals config in Finder (creates default if missing)
- **Quit**

Tray icon colors: green (idle), amber (pending), blue (pushing), red (error).

## Codebase

```
fangit/
├── CMakeLists.txt                # Qt6, macOS app bundle, vendored toml11
├── config.toml                   # Reference config (copy to App Support)
├── macos/
│   └── Info.plist                # LSUIElement=true (menu bar only, no dock)
├── fonts/
│   └── FiraCode-VariableFont_wght.ttf
├── images/
│   ├── AppIcon.png
│   └── AppIcon.icns
├── linux/
│   └── AppIcon-512.png
├── scripts/
│   ├── ninja.sh                  # Debug build script
│   ├── ninja-release.sh          # Signed + notarized release build
│   ├── dummyAGL.sh               # Legacy Qt compatibility shim
│   └── png2icns.sh               # Icon conversion utility
├── src/
│   ├── main.cpp                  # App setup, Fira Code font, startup flow
│   ├── resources.qrc             # Embedded font + icon
│   ├── ConfigManager.h/.cpp      # TOML config (toml11), read/write/defaults
│   ├── TrayManager.h/.cpp        # System tray icon, menu, state management
│   ├── WatcherManager.h/.cpp     # QFileSystemWatcher + periodic scan
│   ├── GitManager.h/.cpp         # git CLI wrapper (clone/add/commit/push/pull)
│   ├── CommitBatcher.h/.cpp      # Debounce timer, file copy, commit formatting
│   └── WorkflowManager.h/.cpp    # GitHub Actions workflow + Issue #1 creation
├── third_party/
│   └── toml11/                   # Vendored toml11 v4 headers
└── notes/
    ├── fangit_DESIGN.md          # Full design document
    ├── notify.yml                # Actions workflow template
    ├── smplr_git_monitor.sh      # Shell prototype (superseded)
    └── test_github_latency.sh    # Push notification latency tests
```

## Architecture

```
main.cpp
├── ConfigManager        — TOML config read/write, path resolution
├── GitManager           — All git CLI interactions (clone, add, commit, push, pull)
├── WatcherManager       — QFileSystemWatcher + 10s periodic scan per watch dir
├── CommitBatcher        — Debounce timer, copies files to repo, formats commits
├── WorkflowManager      — Ensures .github/workflows/notify.yml exists in repo
└── TrayManager          — macOS menu bar icon, menu, status display
```

## Design decisions

- **No Qt styling** — native macOS rendering, zero stylesheets
- **Fira Code** — loaded from embedded resource, set as app-wide font
- **LSUIElement** — menu bar only, no dock icon
- **Shells out to git** — inherits system auth (Keychain, SSH agent), no libgit2
- **toml11 vendored** — no network dependency at build time
- **Append-only by design** — watches for new/modified files, ignores deletes
