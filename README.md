# fangit

**Fang it!** — a lightweight system tray app that turns a GitHub repo into a notification and file archiving system. No server, no extra apps, no infrastructure. Just a GitHub account, one private repo to relay monitoring data, and the GitHub iOS/Android app.

Fast, zero-config local file watching → auto git commit → push → GitHub as the auth/notification/data layer.

<img src="./images/we-fangit.png" style="width: 50vw; max-width: 1000px;">

<b>War Boy: "Shall we run them around into our backup?"<br>
Furiosa: "No, we're good. We fang it!"</b>

## Modes

Fangit has three modes:

| Mode | What it does | Latency | Best for |
|------|-------------|---------|----------|
| **Sync** | Watches folders, auto-commits + pushes files to GitHub | ~45-60s | Archiving outputs, logs, generated files |
| **Notify (dispatch)** | Triggers a GitHub Action that posts an issue comment | ~30-35s | Alerts with one GitHub account |
| **Notify (direct)** | Posts an issue comment via API | ~20-30s | Fastest alerts (needs a second GitHub account) |

All three deliver push notifications to your phone via the GitHub mobile app.

Watches can be routed to either sync or notify mode per-entry — the same folder can archive some files to git while sending lightweight notifications for others.

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

Add `[[watch]]` blocks to monitor folders and/or `[[channel]]` blocks for notifications. See [Configuration](#configuration) below.

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

# Tray icon style: "dot" | "logo" | "tint"
#   dot  = colored circle (default, minimal)
#   logo = app icon with colored status dot in corner
#   tint = app icon tinted entirely in status color
tray_style = "dot"
```

### Repository

```toml
[repo]
url = "https://github.com/yourname/my-fangit.git"
branch = "main"
auth = "https"    # "https" or "ssh"
```

### Watch directories

Each `[[watch]]` block monitors a local folder. What happens when files change depends on the `action` field.

```toml
[[watch]]
path_name = "SMPLR"                # required — unique ID + repo subdirectory name
path = "~/SMPLR/recipes"           # local folder to watch
emoji = "🎵"                       # prefix for commit/notification messages
extensions = ["txt", "json"]       # optional — only watch these file types
action = "sync"                    # optional — see routing below
```

`path_name` is important: it becomes the subdirectory in the git repo (for sync mode). Files from `~/SMPLR/recipes/` appear in the repo under `SMPLR/`.

For multi-machine setups, use `path_name = "SMPLR/macFaux"` to namespace by machine.

#### Watch routing — the `action` field

The `action` field controls what happens when files change in the watched directory:

| `action` value | Behaviour |
|----------------|-----------|
| `"sync"` or omitted | Git commit + push (default). Files archived to repo, notification via Actions. |
| A `[[channel]]` path_name | Skip git. Send a notification via that channel with the filenames. |

This means you can watch the **same folder** with different extensions routed to different behaviours:

```toml
# .txt recipe files → archive to git repo
[[watch]]
path_name = "SMPLR-recipes"
path = "/Volumes/Chainsaw/SMPLR/Stream"
emoji = "🎵"
extensions = ["txt"]
action = "sync"

# .log files → notify only, no git
[[watch]]
path_name = "SMPLR-errors"
path = "/Volumes/Chainsaw/SMPLR/Stream"
emoji = "🔴"
extensions = ["log"]
action = "errors"

# The channel that "errors" routes to
[[channel]]
path_name = "errors"
issue = 2
emoji = "🔴"
mode = "dispatch"
action = "notify"
```

When a `.txt` file appears, it gets committed and pushed to git. When a `.log` file appears, fangit sends a notification listing the filename(s) — no git, no file upload, just a ping on your phone.

If `action` is set to a string that doesn't match any channel, it falls through to sync mode as a safe default.

### Notification channels

Each `[[channel]]` block defines a notification destination — a specific GitHub issue.

```toml
[[channel]]
path_name = "status"          # required — unique channel ID
issue = 1                     # GitHub issue number to post to
emoji = "🟢"
mode = "dispatch"             # "dispatch" or "direct" (see below)
action = "notify"             # "notify" or "notify+push"
# push_dir = "~/logs"         # only used with action = "notify+push"
```

Channels can be triggered in two ways:
1. **From a `[[watch]]`** — set the watch's `action` to the channel's `path_name`
2. **From the tray menu** — click Notify → channel name to send a test message

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

### Sync mode (watch → git → notification)

```
File appears in ~/watched-folder/
  → fangit detects change (QFileSystemWatcher + periodic scan)
    → Debounce timer waits (batch_interval seconds)
      → File copied into local repo clone
        → git add + commit + push
          → GitHub Actions workflow fires
            → github-actions[bot] posts issue comment @mentioning you
              → iOS push notification
```

**Use cases:** Archiving generative outputs, collecting batch job results, backing up config changes, monitoring what a machine produces over time.

**Latency:** ~45-60 seconds (batch_interval + Actions runner ~15s + Apple push ~20s).

### Notify via watch (watch → channel → notification)

```
File appears in ~/watched-folder/
  → fangit detects change
    → Debounce timer waits (batch_interval seconds)
      → Notification sent via channel with filename(s)
        → iOS push notification
```

**Use cases:** Lightweight monitoring — you want to know a file appeared without archiving it. Error log detection, render completion alerts.

**Latency:** ~30-35s (dispatch) or ~20-30s (direct) + batch_interval.

### Notify via tray menu or API

```
Tray menu → Notify → channel name
  → Notification sent via channel
    → iOS push notification
```

**Use cases:** Manual test notifications, status heartbeats, integration with other apps (future CLI).

**Latency:** ~30-35s (dispatch) or ~20-30s (direct).

---

## Menu bar

| Item | Description |
|------|-------------|
| **Status** | Current state: watching, pending, pushing, error |
| **Last push** | Time since last successful push |
| **Watch Directories** | List of monitored folders with action type |
| **Notify** | Send a test notification to any configured channel |
| **Push now** | Force immediate commit + push (skip debounce timer) |
| **Pause/Resume** | Temporarily stop watching |
| **Open git repo** | Open monitoring repo in browser |
| **Open config.toml** | Reveal config file (creates default if missing) |
| **Quit** | Stop watching and exit |

Tray icon states: 🟢 idle, 🟡 pending, 🔵 pushing, 🔴 error.

Tray icon style is configurable via `tray_style` in config: `"dot"` (colored circle), `"logo"` (app icon with status dot), or `"tint"` (app icon tinted in status color).

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
├── CommitBatcher        — Debounce + routing: sync (git) or notify (channel)
├── WorkflowManager      — Auto-creates .github/workflows/notify.yml
├── NotifyManager        — GitHub API: dispatch (workflow_dispatch) + direct (issue comment)
└── TrayManager          — System tray icon, menu, status display
```

### Codebase

```
fangit/
├── CMakeLists.txt
├── config.toml                   # Reference config (bundled into app)
├── macos/Info.plist              # LSUIElement (menu bar only, no dock icon)
├── fonts/FiraCode-VariableFont_wght.ttf
├── images/
│   ├── AppIcon.png|.icns
│   └── TrayIcon.png              # White silhouette for tray (dark/light mode)
├── linux/AppIcon-512.png
├── scripts/
│   ├── ninja.sh                  # Debug build
│   ├── ninja-release.sh          # Signed release build
│   ├── dummyAGL.sh               # Legacy Qt compatibility
│   └── png2icns.sh               # Icon conversion
├── src/
│   ├── main.cpp                  # Entry point, version, component wiring
│   ├── resources.qrc             # Embedded fonts, icons, default config
│   ├── ConfigManager.h/.cpp      # TOML config, tray style, watch/channel parsing
│   ├── TrayManager.h/.cpp        # System tray: dot/logo/tint icon styles
│   ├── WatcherManager.h/.cpp     # QFileSystemWatcher + periodic scan
│   ├── GitManager.h/.cpp         # git CLI wrapper
│   ├── CommitBatcher.h/.cpp      # Debounce, sync/notify routing, commit formatting
│   ├── WorkflowManager.h/.cpp    # Auto-creates GitHub Actions workflow
│   └── NotifyManager.h/.cpp      # GitHub API: dispatch + direct modes
├── third_party/toml11/           # Vendored, header-only
└── notes/                        # Design docs + test scripts
```

### Design decisions

- **Shells out to git CLI** — inherits system auth (Keychain, SSH agent, credential helpers). No libgit2 dependency.
- **toml11 vendored** — header-only C++ TOML parser. No runtime or build-time network dependency.
- **Append-only by design** — watches for new/modified files, ignores deletes.
- **Single workflow file** — handles both push-triggered and dispatch-triggered notifications.
- **Watch→channel routing** — `action` field on watches connects to channels by name. No special syntax, just matching strings.
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
Sync mode queues commits locally. When the network returns, the next push sends everything. Notify mode requires network — missed notifications are not queued.

**Can I watch the same folder with different actions?**
Yes. Use multiple `[[watch]]` blocks with the same `path` but different `extensions` and `action` values. For example, `.txt` files sync to git while `.log` files trigger a notification.

**Can I use this on Linux?**
Yes. Qt6 provides QFileSystemWatcher (inotify backend) and QSystemTrayIcon. Build as an AppImage for distribution.

---

## TODO

- fangit CLI with hooks for external apps & processes
- onboarding 'wizard' dialogs for people who have a github account but otherwise no clue
- various levels of 'courtesy' git options to keep monitoring repo clean
- GitLab, Bitbucket, and Codeberg integration (soon / urgent)

---

## License

MIT
