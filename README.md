# fangit

**Fang it!** — a lightweight system tray app that turns a GitHub repo into a notification and file archiving system. No server, no extra apps, no infrastructure. Just a GitHub account and the GitHub iOS/Android app.

Fangit has three modes:

| Mode | What it does | Latency | Best for |
|------|-------------|---------|----------|
| **Sync** | Watches folders, auto-commits + pushes files to GitHub | ~45-60s | Archiving outputs, logs, generated files |
| **Notify (dispatch)** | Triggers a GitHub Action that posts an issue comment | ~30-35s | Alerts with one GitHub account |
| **Notify (direct)** | Posts an issue comment via API | ~20-30s | Fastest alerts (needs a second GitHub account) |

All three deliver push notifications to your phone via the GitHub mobile app.

## Why

You have a machine doing something — rendering video, running a generative music stream, processing batch jobs — and you want to know what it's doing from your phone. The existing options are either complex (Prometheus, Grafana) or require extra infrastructure (self-hosted notification servers).

Fangit uses GitHub as the entire backend: hosting, auth, iOS/Android notifications, web viewer, history, and collaboration. You get all of that for free with a GitHub account you probably already have.

## Quick start

### 1. Create a GitHub repo

Create a new **private** repository on GitHub (e.g. `my-fangit`). This is where fangit stores files and posts notifications.

### 2. Set up authentication

Fangit uses git's built-in credential system. If you can `git push` from the terminal, fangit can too.

**HTTPS (recommended):** Generate a [Personal Access Token](https://github.com/settings/tokens) and enter it when git prompts for a password on first push.

- **Classic PAT**: check `repo` + `workflow` scopes. Can be set to never expire.
- **Fine-grained PAT**: select your fangit repo only, grant `Contents: Read & Write` + `Issues: Read & Write` + `Actions: Read & Write`. Can be set to never expire for personal repos.

**SSH:** Configure an SSH key as usual. Use a host alias in `~/.ssh/config` if you need multiple identities.

### 3. Install and configure fangit

Launch fangit. Click the tray icon → **Open config.toml**. Edit the config:

```toml
[general]
github_user = "yourname"
batch_interval = 60
scan_interval = 30

[repo]
url = "https://github.com/yourname/my-fangit.git"
branch = "main"
auth = "https"
```

Add a `[[watch]]` block to monitor a folder, and/or `[[channel]]` blocks for notifications. See [Configuration](#configuration) below.

### 4. Restart fangit

On first launch with a valid config, fangit will clone the repo and push a `notify.yml` GitHub Actions workflow. This workflow handles push notifications — fangit creates it automatically.

### 5. Create GitHub issues for notification channels

Each `[[channel]]` posts to a specific GitHub issue. Create issues in your repo:
- Issue #1: "Status"
- Issue #2: "Errors"
- Issue #3: "Log"

(Or whatever makes sense for your setup. Issue titles are what appear in the iOS notification.)

### 6. Install the GitHub mobile app

Install [GitHub for iOS](https://apps.apple.com/app/github/id1477376905) or [Android](https://play.google.com/store/apps/details?id=com.github.android). Sign in, enable push notifications. You'll receive alerts whenever fangit posts to an issue.

---

## Configuration

Config file location:
- **macOS**: `~/Library/Application Support/fangit/config.toml`
- **Linux**: `~/.config/fangit/config.toml`

### General settings

```toml
[general]
# GitHub username — used in @mentions for notifications
github_user = "yourname"

# Seconds to wait before committing file changes (30-300)
# Lower = more responsive, higher = fewer commits
batch_interval = 60

# Filesystem scan interval in seconds (10-300)
# Supplements QFileSystemWatcher for reliability
scan_interval = 30

# Tray icon style: false = colored dot, true = app icon with status tint
use_icon_logo = false
```

### Repository

```toml
[repo]
url = "https://github.com/yourname/my-fangit.git"
branch = "main"
auth = "https"    # "https" or "ssh"
```

### Watch directories (Sync mode)

Each `[[watch]]` block monitors a local folder. New or modified files are committed and pushed to the repo automatically.

```toml
[[watch]]
path_name = "SMPLR"                # required — unique ID + repo subdirectory name
path = "~/SMPLR/recipes"           # local folder to watch
emoji = "🎵"                       # prefix for commit messages
extensions = ["txt", "json"]       # optional — only watch these file types
```

`path_name` is important: it becomes the subdirectory in the git repo. Files from `~/SMPLR/recipes/` appear in the repo under `SMPLR/`.

For multi-machine setups, use `path_name = "SMPLR/macFaux"` to namespace by machine.

### Notification channels (Notify mode)

Each `[[channel]]` block sends messages directly to a GitHub issue — no files, no git commits.

```toml
[[channel]]
path_name = "status"          # required — unique channel ID
issue = 1                     # GitHub issue number to post to
emoji = "🟢"
mode = "dispatch"             # "dispatch" or "direct" (see below)
action = "notify"             # "notify" or "notify+push"
# push_dir = "~/logs"         # only used with action = "notify+push"
```

**Delivery modes:**

| Mode | How | Speed | Accounts needed |
|------|-----|-------|-----------------|
| `dispatch` | Triggers a GitHub Action → `github-actions[bot]` posts the comment | ~30-35s | 1 (your account) |
| `direct` | Posts the comment directly via API | ~20-30s | 2 (see below) |

**Why two modes?** GitHub suppresses notifications for your own actions. When *you* post a comment mentioning *yourself*, no notification fires. The `dispatch` mode works around this by having the Actions bot post the comment instead. The `direct` mode is faster but requires a second GitHub account to be the commenter.

**Setting up direct mode:**
1. Create a second GitHub account (e.g. `yourname-bot`)
2. Invite it as a collaborator on your fangit repo
3. Configure git credentials for the bot account (or use a PAT)
4. The bot posts comments mentioning your main account → notification fires

**The `notify+push` action:** For channels where you want both a notification and supporting files (e.g. crash logs):

```toml
[[channel]]
path_name = "errors"
issue = 2
emoji = "🔴"
mode = "dispatch"
action = "notify+push"
push_dir = "~/myapp/logs"       # this directory gets committed + pushed
```

---

## Modes explained

### Sync mode

```
File appears in ~/watched-folder/
  → fangit detects it (QFileSystemWatcher + periodic scan)
    → Debounce timer waits (batch_interval seconds)
      → File copied into local repo clone
        → git add + commit + push
          → GitHub Actions workflow fires
            → github-actions[bot] posts issue comment @mentioning you
              → iOS push notification
```

**Use cases:** Archiving generative outputs, collecting batch job results, backing up config changes, monitoring what a machine produces over time.

**Latency:** ~45-60 seconds (batch_interval + Actions runner ~15s + Apple push ~20s).

### Notify mode (dispatch)

```
fangit notify "status" "Stream healthy"
  → API call: POST workflow_dispatch event
    → GitHub Action runs (~15s)
      → github-actions[bot] posts issue comment @mentioning you
        → iOS push notification (~20s)
```

**Use cases:** Status heartbeats, error alerts, event notifications — anything where you want a ping without files.

**Latency:** ~30-35 seconds. Works with a single GitHub account.

### Notify mode (direct)

```
fangit notify "status" "Stream healthy"
  → API call: POST issue comment directly
    → iOS push notification (~20s)
```

**Use cases:** Same as dispatch, but faster. Requires a second GitHub account.

**Latency:** ~20-30 seconds.

---

## Menu bar

| Item | Description |
|------|-------------|
| **Status** | Current state: watching, pending, pushing, error |
| **Last push** | Time since last successful push |
| **Watch Directories** | List of monitored folders |
| **Notify** | Send a test notification to any configured channel |
| **Push now** | Force immediate commit + push (skip debounce timer) |
| **Pause/Resume** | Temporarily stop watching |
| **View on GitHub** | Open repo in browser |
| **Open config.toml** | Reveal config file (creates default if missing) |
| **Quit** | Stop watching and exit |

Tray icon colors: 🟢 idle, 🟡 pending, 🔵 pushing, 🔴 error.

---

## GitHub Actions workflow

Fangit automatically creates `.github/workflows/notify.yml` in your repo on first launch. This single workflow handles both sync mode (triggered by `push`) and dispatch notify mode (triggered by `workflow_dispatch`). You don't need to create or edit it manually.

If you need to recreate it, delete the file from your repo and restart fangit.

---

## Build from source

### Requirements

- macOS 12+ or Linux (Ubuntu 22.04+)
- Qt 6.7+ (Core, Widgets, Network)
- CMake 3.20+
- Ninja
- git CLI

### Build

```bash
./scripts/ninja.sh          # Debug build
./scripts/ninja.sh run      # Build + launch
./scripts/ninja.sh clean    # Clean build directory
```

### Release build (macOS, signed + notarized)

```bash
./scripts/ninja-release.sh
./scripts/ninja-release.sh universal   # arm64 + x86_64
```

---

## Architecture

```
main.cpp
├── ConfigManager        — TOML config (toml11), read/write, path resolution
├── GitManager           — git CLI wrapper (clone, add, commit, push, pull)
├── WatcherManager       — QFileSystemWatcher + configurable periodic scan
├── CommitBatcher        — Debounce timer, file copy to repo, commit formatting
├── WorkflowManager      — Auto-creates .github/workflows/notify.yml
├── NotifyManager        — GitHub API: dispatch (workflow_dispatch) + direct (issue comment)
└── TrayManager          — System tray icon, menu, status display
```

### Codebase

```
fangit/
├── CMakeLists.txt
├── config.toml                   # Reference config
├── macos/Info.plist              # LSUIElement (menu bar only, no dock icon)
├── fonts/FiraCode-VariableFont_wght.ttf
├── images/AppIcon.png|.icns
├── linux/AppIcon-512.png
├── scripts/
│   ├── ninja.sh                  # Debug build
│   ├── ninja-release.sh          # Signed release build
│   ├── dummyAGL.sh               # Legacy Qt compatibility
│   └── png2icns.sh               # Icon conversion
├── src/
│   ├── main.cpp
│   ├── resources.qrc
│   ├── ConfigManager.h/.cpp
│   ├── TrayManager.h/.cpp
│   ├── WatcherManager.h/.cpp
│   ├── GitManager.h/.cpp
│   ├── CommitBatcher.h/.cpp
│   ├── WorkflowManager.h/.cpp
│   └── NotifyManager.h/.cpp
├── third_party/toml11/           # Vendored, header-only
└── notes/                        # Design docs + test scripts
```

### Design decisions

- **Shells out to git CLI** — inherits system auth (Keychain, SSH agent, credential helpers). No libgit2 dependency.
- **toml11 vendored** — header-only C++ TOML parser. No runtime or build-time network dependency.
- **Append-only by design** — watches for new/modified files, ignores deletes.
- **Single workflow file** — handles both push-triggered and dispatch-triggered notifications.
- **LSUIElement** — menu bar only, no dock icon (macOS).
- **Cross-platform** — Qt6 with QFileSystemWatcher (fsevents on macOS, inotify on Linux), QSystemTrayIcon for tray.

---

## FAQ

**Do I need a second GitHub account?**
No. Dispatch mode works with a single account. Direct mode is faster but requires a second account because GitHub doesn't notify you about your own actions.

**Will this violate GitHub's terms of service?**
No. Automated commits with small files at reasonable intervals (30-300s) are within GitHub's acceptable use policy. Machine accounts are explicitly permitted.

**How much does this cost?**
Nothing. GitHub free tier includes private repos, Actions minutes, and the mobile app.

**Will the repo get huge?**
Eventually, if you push a lot. For text files and small images, a year of use is fine. For large files, consider Git LFS or periodic history cleanup.

**What if my machine goes offline?**
Fangit queues commits locally. When the network returns, the next push sends everything.

**Can I use this on Linux?**
Yes. Qt6 provides QFileSystemWatcher (inotify backend) and QSystemTrayIcon. Build as an AppImage for distribution.

---

## License

MIT
