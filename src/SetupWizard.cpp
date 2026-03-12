#include "SetupWizard.h"
#include "SetupWizardTheme.h"
#include "ConfigManager.h"
#include "GitManager.h"
#include "WorkflowManager.h"

using namespace WizardTheme;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

// ════════════════════════════════════════════════════════════════════════════
// SetupWizard
// ════════════════════════════════════════════════════════════════════════════

SetupWizard::SetupWizard(ConfigManager &config, GitManager &git,
                         WorkflowManager &workflow, QWidget *parent)
    : QWizard(parent)
    , m_config(config)
    , m_git(git)
    , m_workflow(workflow)
{
    setWindowTitle("fangit Setup");
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(600, 520);

    setPage(Page_Welcome,        new WelcomePage);
    setPage(Page_GitCheck,       new GitCheckPage(git));
    setPage(Page_GitHub,         new GitHubPage);
    setPage(Page_Repo,           new RepoPage);
    setPage(Page_Auth,           new AuthPage);
    setPage(Page_TestConnection, new TestConnectionPage(git, config));
    setPage(Page_SetupRepo,      new SetupRepoPage(git, config, workflow));
    setPage(Page_WatchDir,       new WatchDirPage);
    setPage(Page_Done,           new DonePage);

    setStartId(Page_Welcome);
}

void SetupWizard::accept()
{
    // ── Write wizard data into ConfigManager ───────────────────────
    m_config.setGithubUser(field("github_user").toString().trimmed());
    m_config.setRepoUrl(field("repo_url").toString().trimmed());
    m_config.setRepoBranch(field("repo_branch").toString().trimmed());

    // Watch directory (if not skipped)
    if (!field("watch_skip").toBool() && !field("watch_path").toString().trimmed().isEmpty()) {
        WatchEntry entry;
        entry.pathName = field("watch_path_name").toString().trimmed();
        entry.path = field("watch_path").toString().trimmed();
        entry.action = "sync";
        QString exts = field("watch_extensions").toString().trimmed();
        if (!exts.isEmpty()) {
            for (const auto &e : exts.split(","))
                entry.extensions.append(e.trimmed());
        }
        m_config.addWatchEntry(entry);
    }

    // Default notification channels (if wizard created them)
    if (m_config.channels().isEmpty()) {
        NotifyChannel status;
        status.pathName = "status";
        status.issue    = 1;
        status.emoji    = "\xF0\x9F\x9F\xA2";    // 🟢
        status.mode     = NotifyChannel::Dispatch;
        status.action   = NotifyChannel::NotifyOnly;
        m_config.addChannel(status);

        NotifyChannel error;
        error.pathName = "error";
        error.issue    = 2;
        error.emoji    = "\xF0\x9F\x94\xB4";      // 🔴
        error.mode     = NotifyChannel::Dispatch;
        error.action   = NotifyChannel::NotifyOnly;
        m_config.addChannel(error);

        NotifyChannel log;
        log.pathName = "log";
        log.issue    = 3;
        log.emoji    = "\xE2\x9A\xAA";            // ⚪
        log.mode     = NotifyChannel::Dispatch;
        log.action   = NotifyChannel::NotifyOnly;
        m_config.addChannel(log);
    }

    m_config.save();
    m_completed = true;

    QWizard::accept();
}

// ════════════════════════════════════════════════════════════════════════════
// Welcome
// ════════════════════════════════════════════════════════════════════════════

WelcomePage::WelcomePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Welcome to fangit");
    setSubTitle("Let's get you set up in a few minutes.");

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        "<p><b>Fang it!</b> watches folders on your computer and sends push "
        "notifications to your phone via GitHub.</p>"
        "<p>To get started you'll need:</p>"
        "<ul style='margin-left: " + QString::number(Spacing::statusPad * 2) + "px;'>"
        "<li>A <b>GitHub account</b> (free)</li>"
        "<li>The <b>GitHub mobile app</b> on your phone with notifications enabled</li>"
        "<li><b>git</b> installed on this computer</li>"
        "</ul>"
        "<p>This wizard takes about 3 minutes.</p>"
    );
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════════════════
// Git Check
// ════════════════════════════════════════════════════════════════════════════

GitCheckPage::GitCheckPage(GitManager &git, QWidget *parent)
    : QWizardPage(parent), m_git(git)
{
    setTitle("Checking for git");
    setSubTitle("fangit needs git to push files and trigger notifications.");

    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Checking...");
    m_statusLabel->setStyleSheet(statusStyle(Spacing::statusPadLarge));
    layout->addWidget(m_statusLabel);

    m_helpLabel = new QLabel;
    m_helpLabel->setWordWrap(true);
    m_helpLabel->setOpenExternalLinks(true);
    m_helpLabel->setVisible(false);
    layout->addWidget(m_helpLabel);

    layout->addStretch();
}

void GitCheckPage::initializePage()
{
    m_gitFound = m_git.isGitInstalled();

    if (m_gitFound) {
        m_statusLabel->setText("\xe2\x9c\x93  " + m_git.gitVersion());
        m_statusLabel->setStyleSheet(statusStyle(Color::success, Spacing::statusPadLarge));
        m_helpLabel->setVisible(false);
    } else {
        m_statusLabel->setText("\xe2\x9c\x97  git not found");
        m_statusLabel->setStyleSheet(statusStyle(Color::error, Spacing::statusPadLarge));
        m_helpLabel->setText(
#ifdef Q_OS_MACOS
            "<p>Open <b>Terminal</b> and run:</p>"
            "<pre>xcode-select --install</pre>"
            "<p>This installs Apple's Command Line Tools (includes git). "
            "Restart fangit after installation.</p>"
#else
            "<p>Run:</p>"
            "<pre>sudo apt install git</pre>"
            "<p>Restart fangit after installation.</p>"
#endif
        );
        m_helpLabel->setVisible(true);
    }
    emit completeChanged();
}

bool GitCheckPage::isComplete() const { return m_gitFound; }

// ════════════════════════════════════════════════════════════════════════════
// GitHub Account
// ════════════════════════════════════════════════════════════════════════════

GitHubPage::GitHubPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("GitHub Account");
    setSubTitle("Your GitHub username is used for @mentions in notifications.");

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("GitHub username to notify:"));

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("e.g. yourname");
    layout->addWidget(m_usernameEdit);

    registerField("github_user*", m_usernameEdit);

    layout->addStretch();
    auto *help = new QLabel(
        mutedHtml(
            "Don't have a GitHub account? "
            "<a href='https://github.com/signup'>Create one here</a> — it's free."
        )
    );
    help->setOpenExternalLinks(true);
    help->setWordWrap(true);
    layout->addWidget(help);

    layout->addStretch();
}

bool GitHubPage::isComplete() const
{
    return !m_usernameEdit->text().trimmed().isEmpty();
}

// ════════════════════════════════════════════════════════════════════════════
// Repository
// ════════════════════════════════════════════════════════════════════════════

RepoPage::RepoPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("GitHub Repository");
    setSubTitle("fangit needs an empty repository to store files and route notifications.");

    auto *layout = new QVBoxLayout(this);

    auto *step1 = new QLabel(
        "<p><b>Step 1:</b> Create a new <b>private</b> repository on GitHub.</p>"
        "<p><a href='https://github.com/new'>Click here to create one</a></p>"
        + mutedHtml(
            "Name it anything (e.g. \"my-fangit\"). Make sure it's <b>Private</b>."
        )
    );
    step1->setOpenExternalLinks(true);
    step1->setWordWrap(true);
    layout->addWidget(step1);

    layout->addSpacing(10);
    layout->addWidget(new QLabel("<b>Step 2:</b> Paste the repository URL:"));

    m_repoUrlEdit = new QLineEdit;
    m_repoUrlEdit->setPlaceholderText("https://github.com/yourname/my-fangit.git");
    layout->addWidget(m_repoUrlEdit);
    registerField("repo_url*", m_repoUrlEdit);

    layout->addSpacing(6);
    layout->addWidget(new QLabel("Branch:"));

    m_branchCombo = new QComboBox;
    m_branchCombo->addItems({"main", "master"});
    m_branchCombo->setEditable(true);
    layout->addWidget(m_branchCombo);
    registerField("repo_branch", m_branchCombo, "currentText");

    layout->addStretch();
}

bool RepoPage::isComplete() const
{
    QString url = m_repoUrlEdit->text().trimmed();
    return url.contains("github.com") && url.contains("/");
}

// ════════════════════════════════════════════════════════════════════════════
// Authentication
// ════════════════════════════════════════════════════════════════════════════

AuthPage::AuthPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Authentication");
    setSubTitle("fangit needs permission to push to your repository.");

    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Checking for existing credentials...");
    m_statusLabel->setStyleSheet(statusStyle());
    layout->addWidget(m_statusLabel);

    // ── Privacy notice (always visible) ────────────────────────────
    m_privacyLabel = new QLabel(
        "<p><b>Your credentials stay on this computer.\n\n</b> "
        "git stores them in the system keychain "
#ifdef Q_OS_MACOS
        "(macOS Keychain). "
#else
        "(GNOME Keyring / KDE Wallet / credential-store). "
#endif
        "fangit never stores passwords in its config file and never sends "
        "credentials anywhere other than github.com.</p>"
    );
    m_privacyLabel->setStyleSheet(privacyBoxStyle());
    m_privacyLabel->setWordWrap(true);
    layout->addWidget(m_privacyLabel);

    layout->addSpacing(8);

    // ── PAT instructions (hidden when credentials exist) ───────────
    m_patInstructions = new QLabel(
        "<p><b>Create a Personal Access Token (PAT):</b></p>"
        "<ol>"
        "<li>Open <a href='https://github.com/settings/tokens'>GitHub \xe2\x86\x92 Settings \xe2\x86\x92 Tokens</a></li>"
        "<li>Click <b>Generate new token (classic)</b></li>"
        "<li>Name: <b>fangit</b></li>"
        "<li>Expiration: <b>No expiration</b></li>"
        "<li>Scopes: tick <b>repo</b> and <b>workflow</b></li>"
        "<li>Click <b>Generate token</b>, copy the <code>ghp_...</code> string</li>"
        "</ol>"
        "<p>On the next step, git will ask for your credentials. "
        "Enter your <b>GitHub username</b> and paste the <b>token as the password</b>. "
        "The system keychain stores it automatically — you only enter it once.</p>"
    );
    m_patInstructions->setWordWrap(true);
    m_patInstructions->setOpenExternalLinks(true);
    layout->addWidget(m_patInstructions);

    // ── Action buttons ─────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;

    m_openGitHubBtn = new QPushButton("Open GitHub Token Page");
    connect(m_openGitHubBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/settings/tokens"));
    });
    btnRow->addWidget(m_openGitHubBtn);

#ifdef Q_OS_MACOS
    m_openKeychainBtn = new QPushButton("Open Keychain Access");
    connect(m_openKeychainBtn, &QPushButton::clicked, this, []() {
        QProcess::startDetached("open", {"-a", "Keychain Access"});
    });
    btnRow->addWidget(m_openKeychainBtn);
#else
    m_openKeychainBtn = nullptr;
#endif

    btnRow->addStretch();
    layout->addLayout(btnRow);
    layout->addStretch();
}

void AuthPage::initializePage()
{
    detectCredentials();
}

void AuthPage::detectCredentials()
{
    // Probe git credential helper for existing github.com credentials
    QProcess process;
    process.start("git", {"credential", "fill"});
    if (!process.waitForStarted(3000)) {
        m_credentialsDetected = false;
        m_statusLabel->setText("\xe2\x9a\xa0\xef\xb8\x8f  Could not check credentials");
        m_statusLabel->setStyleSheet(statusStyle(Color::warning));
        return;
    }

    process.write("protocol=https\nhost=github.com\n\n");
    process.closeWriteChannel();

    if (!process.waitForFinished(5000)) {
        m_credentialsDetected = false;
        return;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    bool found = false;
    for (const auto &line : output.split('\n')) {
        if (line.startsWith("password=") && line.length() > 10) {
            found = true;
            break;
        }
    }

    // Also check for SSH key as a second path
    if (!found) {
        QStringList sshKeys = {"id_ed25519", "id_rsa", "id_ecdsa"};
        QString sshDir = QDir::homePath() + "/.ssh";
        for (const auto &key : sshKeys) {
            if (QFileInfo::exists(sshDir + "/" + key)) {
                found = true;
                break;
            }
        }
    }

    m_credentialsDetected = found;

    if (found) {
        m_statusLabel->setText(
            "\xe2\x9c\x93  Existing GitHub credentials found. fangit will use these automatically.");
        m_statusLabel->setStyleSheet(statusStyle(Color::success));
        m_patInstructions->setVisible(false);
        m_openGitHubBtn->setVisible(false);
        if (m_openKeychainBtn) m_openKeychainBtn->setVisible(false);
    } else {
        m_statusLabel->setText(
            "No existing GitHub credentials detected. Follow the steps below.");
        m_statusLabel->setStyleSheet(statusStyle(Color::warning));
        m_patInstructions->setVisible(true);
        m_openGitHubBtn->setVisible(true);
        if (m_openKeychainBtn) m_openKeychainBtn->setVisible(true);
    }
    emit completeChanged();
}

bool AuthPage::isComplete() const
{
    // Always allow proceeding — if no creds, git will prompt on first push
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Test Connection
// ════════════════════════════════════════════════════════════════════════════

TestConnectionPage::TestConnectionPage(GitManager &git, ConfigManager &config,
                                         QWidget *parent)
    : QWizardPage(parent), m_git(git), m_config(config)
{
    setTitle("Testing Connection");
    setSubTitle("Cloning your repository to verify everything works...");

    auto *layout = new QVBoxLayout(this);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 0);
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Preparing...");
    m_statusLabel->setStyleSheet(statusStyle());
    layout->addWidget(m_statusLabel);

    m_logOutput = new QTextEdit;
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(140);
    m_logOutput->setStyleSheet(logOutputStyle());
    layout->addWidget(m_logOutput);

    m_retryBtn = new QPushButton("Retry");
    m_retryBtn->setVisible(false);
    connect(m_retryBtn, &QPushButton::clicked, this, &TestConnectionPage::runTest);
    layout->addWidget(m_retryBtn);

    layout->addStretch();
}

void TestConnectionPage::initializePage()
{
    // Apply settings from wizard fields before cloning
    m_config.setRepoUrl(field("repo_url").toString().trimmed());
    m_config.setRepoBranch(field("repo_branch").toString().trimmed());
    m_config.setGithubUser(field("github_user").toString().trimmed());

    runTest();
}

void TestConnectionPage::runTest()
{
    m_success = false;
    m_retryBtn->setVisible(false);
    m_logOutput->clear();
    m_progressBar->setRange(0, 0);
    m_statusLabel->setText("Cloning repository...");
    m_statusLabel->setStyleSheet(statusStyle());
    QApplication::processEvents();

    m_logOutput->append("$ git clone " + m_config.repoUrl());

    if (m_git.isRepoCloned()) {
        m_logOutput->append("Repository already cloned locally.");
        m_logOutput->append("\xe2\x9c\x93 Connection verified.");
        m_success = true;
    } else if (m_git.cloneRepo()) {
        m_logOutput->append("\xe2\x9c\x93 Clone successful.");
        m_success = true;
    } else {
        m_logOutput->append("\xe2\x9c\x97 " + m_git.lastError());
        m_logOutput->append("");
        m_logOutput->append("Check that:");
        m_logOutput->append("  \xe2\x80\xa2 The repository URL is correct");
        m_logOutput->append("  \xe2\x80\xa2 The repository exists on GitHub");
        m_logOutput->append("  \xe2\x80\xa2 Your credentials are set up");
    }

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(m_success ? 1 : 0);

    if (m_success) {
        m_statusLabel->setText("\xe2\x9c\x93  Connected to repository");
        m_statusLabel->setStyleSheet(statusStyle(Color::success));
    } else {
        m_statusLabel->setText("\xe2\x9c\x97  Connection failed");
        m_statusLabel->setStyleSheet(statusStyle(Color::error));
        m_retryBtn->setVisible(true);
    }
    emit completeChanged();
}

bool TestConnectionPage::isComplete() const { return m_success; }

// ════════════════════════════════════════════════════════════════════════════
// Setup Repo (workflow + issues)
// ════════════════════════════════════════════════════════════════════════════

SetupRepoPage::SetupRepoPage(GitManager &git, ConfigManager &config,
                               WorkflowManager &workflow, QWidget *parent)
    : QWizardPage(parent), m_git(git), m_config(config), m_workflow(workflow)
{
    setTitle("Setting Up Repository");
    setSubTitle("Creating the notification workflow and issue channels...");

    auto *layout = new QVBoxLayout(this);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 0);   // indeterminate until first step completes
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Preparing...");
    m_statusLabel->setStyleSheet(statusStyle());
    layout->addWidget(m_statusLabel);

    m_logOutput = new QTextEdit;
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(180);
    m_logOutput->setStyleSheet(logOutputStyle());
    layout->addWidget(m_logOutput);

    layout->addStretch();

    m_issues = {{"Status", 1}, {"Errors", 2}, {"Log", 3}};
}

void SetupRepoPage::initializePage()
{
    startSetup();
}

// ── Step 1: Push workflow (synchronous — local git, fast) ──────────

void SetupRepoPage::startSetup()
{
    m_success = false;
    m_logOutput->clear();
    m_progressBar->setRange(0, 0);  // indeterminate spinner

    m_statusLabel->setText("Creating notification workflow...");
    m_logOutput->append("Creating .github/workflows/notify.yml...");

    // Workflow push is local git — fast enough to stay synchronous
    m_workflowOk = m_workflow.ensureWorkflowExists();
    m_logOutput->append(m_workflowOk
        ? "\xe2\x9c\x93 Workflow created and pushed."
        : "\xe2\x9c\x97 Workflow creation failed (you can create it manually later).");

    // Now switch to stepped progress: workflow(done) + 3 issues + test = 5 steps
    m_progressBar->setRange(0, 5);
    m_progressBar->setValue(1);

    stepRetrievePat();
}

// ── Step 2: Retrieve PAT (synchronous — local credential helper) ───

void SetupRepoPage::stepRetrievePat()
{
    m_statusLabel->setText("Checking credentials...");
    m_logOutput->append("");
    m_logOutput->append("Retrieving credentials from keychain...");

    QProcess process;
    process.start("git", {"credential", "fill"});
    m_pat.clear();
    if (process.waitForStarted(3000)) {
        process.write("protocol=https\nhost=github.com\n\n");
        process.closeWriteChannel();
        if (process.waitForFinished(5000)) {
            for (const auto &line : QString::fromUtf8(process.readAllStandardOutput()).split('\n')) {
                if (line.startsWith("password=")) {
                    m_pat = line.mid(9).trimmed();
                    break;
                }
            }
        }
    }

    // Parse owner/repo
    QString url = m_config.repoUrl();
    m_owner.clear();
    m_repo.clear();
    if (url.startsWith("https://github.com/")) {
        QStringList parts = url.mid(19).split('/');
        if (parts.size() >= 2) {
            m_owner = parts[0];
            m_repo = parts[1];
            if (m_repo.endsWith(".git")) m_repo.chop(4);
        }
    }

    if (m_pat.isEmpty() || m_owner.isEmpty()) {
        m_logOutput->append("  \xe2\x9a\xa0\xef\xb8\x8f Could not create issues (no PAT in keychain yet).");
        m_logOutput->append("  Create them manually in your GitHub repo:");
        m_logOutput->append("    Issue #1: \"Status\"");
        m_logOutput->append("    Issue #2: \"Errors\"");
        m_logOutput->append("    Issue #3: \"Log\"");

        // Skip issue creation and test dispatch — jump to done
        m_progressBar->setValue(5);
        finishSetup();
        return;
    }

    m_logOutput->append("\xe2\x9c\x93 Credentials found.");

    // Create the network manager for all subsequent async calls
    if (!m_net) {
        m_net = new QNetworkAccessManager(this);
    }

    // Start issue creation chain
    m_statusLabel->setText("Creating notification issues...");
    m_logOutput->append("");
    m_logOutput->append("Creating GitHub issues...");
    m_issueIndex = 0;
    stepCheckNextIssue();
}

// ── Step 3: Check/create issues (async chain) ─────────────────────

void SetupRepoPage::stepCheckNextIssue()
{
    if (m_issueIndex >= m_issues.size()) {
        // All issues processed — move to test dispatch
        stepDispatchTest();
        return;
    }

    const auto &tmpl = m_issues[m_issueIndex];

    QNetworkRequest req(QUrl(QString(
        "https://api.github.com/repos/%1/%2/issues/%3")
        .arg(m_owner, m_repo).arg(tmpl.num)));
    req.setRawHeader("Authorization", ("token " + m_pat).toUtf8());
    req.setRawHeader("User-Agent", "fangit");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tmpl]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (status == 200) {
            m_logOutput->append(QString("  \xe2\x9c\x93 Issue #%1 \"%2\" already exists")
                .arg(tmpl.num).arg(tmpl.title));
            m_progressBar->setValue(2 + m_issueIndex);
            m_issueIndex++;
            stepCheckNextIssue();
        } else {
            // Issue doesn't exist — create it
            stepCreateIssue(tmpl.num, tmpl.title);
        }
    });
}

void SetupRepoPage::stepCreateIssue(int issueNum, const QString &title)
{
    QNetworkRequest createReq(QUrl(QString(
        "https://api.github.com/repos/%1/%2/issues").arg(m_owner, m_repo)));
    createReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    createReq.setRawHeader("Authorization", ("token " + m_pat).toUtf8());
    createReq.setRawHeader("User-Agent", "fangit");

    QJsonObject body;
    body["title"] = title;
    body["body"]  = "fangit notification channel. Do not close this issue.";

    QNetworkReply *reply = m_net->post(createReq, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, issueNum, title]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        m_logOutput->append(status >= 200 && status < 300
            ? QString("  \xe2\x9c\x93 Created issue #%1 \"%2\"").arg(issueNum).arg(title)
            : QString("  \xe2\x9c\x97 Failed to create \"%1\" (HTTP %2)").arg(title).arg(status));

        m_progressBar->setValue(2 + m_issueIndex);
        m_issueIndex++;
        stepCheckNextIssue();
    });
}

// ── Step 4: Dispatch test notification (async) ────────────────────

void SetupRepoPage::stepDispatchTest()
{
    m_progressBar->setValue(4);

    if (!m_workflowOk) {
        // Can't dispatch without workflow
        finishSetup();
        return;
    }

    m_statusLabel->setText("Sending test notification...");
    m_logOutput->append("");
    m_logOutput->append("Dispatching test notification via GitHub Actions...");

    QString user = field("github_user").toString().trimmed();

    QNetworkRequest dispReq(QUrl(QString(
        "https://api.github.com/repos/%1/%2/actions/workflows/notify.yml/dispatches")
        .arg(m_owner, m_repo)));
    dispReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    dispReq.setRawHeader("Authorization", ("token " + m_pat).toUtf8());
    dispReq.setRawHeader("User-Agent", "fangit");

    QJsonObject inputs;
    inputs["channel"]     = "status";
    inputs["issue"]       = "1";
    inputs["message"]     = "\xF0\x9F\x8E\x89 fangit setup complete!";
    inputs["github_user"] = user;

    QJsonObject dispBody;
    dispBody["ref"]    = m_config.repoBranch();
    dispBody["inputs"] = inputs;

    QNetworkReply *reply = m_net->post(dispReq, QJsonDocument(dispBody).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (status >= 200 && status < 300) {
            m_logOutput->append("\xe2\x9c\x93 Test notification dispatched!");
            m_logOutput->append("  Check your phone in ~30-60 seconds.");
        } else {
            m_logOutput->append(QString("\xe2\x9c\x97 Dispatch failed (HTTP %1). "
                "You can test later from the tray menu.").arg(status));
        }

        finishSetup();
    });
}

// ── Done ──────────────────────────────────────────────────────────

void SetupRepoPage::finishSetup()
{
    m_progressBar->setValue(5);
    m_success = true;
    m_statusLabel->setText("\xe2\x9c\x93  Repository setup complete");
    m_statusLabel->setStyleSheet(statusStyle(Color::success));
    emit completeChanged();
}

bool SetupRepoPage::isComplete() const { return m_success; }

// ════════════════════════════════════════════════════════════════════════════
// Watch Directory
// ════════════════════════════════════════════════════════════════════════════

WatchDirPage::WatchDirPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Watch a Directory");
    setSubTitle("Choose a folder for fangit to monitor. You can add more later in config.toml.");

    auto *layout = new QVBoxLayout(this);

    m_skipCheck = new QCheckBox("Skip — I'll add watch directories later");
    layout->addWidget(m_skipCheck);
    registerField("watch_skip", m_skipCheck);

    layout->addSpacing(12);

    layout->addWidget(new QLabel("Name (used as repo subdirectory):"));
    m_pathNameEdit = new QLineEdit;
    m_pathNameEdit->setPlaceholderText("e.g. MyProject");
    layout->addWidget(m_pathNameEdit);
    registerField("watch_path_name", m_pathNameEdit);

    layout->addWidget(new QLabel("Folder to watch:"));
    auto *pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText("~/path/to/folder");
    pathRow->addWidget(m_pathEdit);
    m_browseBtn = new QPushButton("Browse...");
    connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Watch Directory",
                          QDir::homePath());
        if (!dir.isEmpty()) {
            m_pathEdit->setText(dir);
            if (m_pathNameEdit->text().isEmpty())
                m_pathNameEdit->setText(QDir(dir).dirName());
        }
    });
    pathRow->addWidget(m_browseBtn);
    layout->addLayout(pathRow);
    registerField("watch_path", m_pathEdit);

    layout->addWidget(new QLabel("File extensions (comma-separated, blank for all):"));
    m_extensionsEdit = new QLineEdit;
    m_extensionsEdit->setPlaceholderText("e.g. txt, json, log");
    layout->addWidget(m_extensionsEdit);
    registerField("watch_extensions", m_extensionsEdit);

    connect(m_pathNameEdit, &QLineEdit::textChanged, this, &WatchDirPage::completeChanged);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &WatchDirPage::completeChanged);

    connect(m_skipCheck, &QCheckBox::toggled, this, [this](bool skip) {
        m_pathNameEdit->setEnabled(!skip);
        m_pathEdit->setEnabled(!skip);
        m_browseBtn->setEnabled(!skip);
        m_extensionsEdit->setEnabled(!skip);
        emit completeChanged();
    });

    layout->addStretch();
}

bool WatchDirPage::isComplete() const
{
    if (m_skipCheck->isChecked()) return true;
    return !m_pathNameEdit->text().trimmed().isEmpty()
        && !m_pathEdit->text().trimmed().isEmpty();
}

// ════════════════════════════════════════════════════════════════════════════
// Done
// ════════════════════════════════════════════════════════════════════════════

DonePage::DonePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("You're all set!");

    auto *layout = new QVBoxLayout(this);
    m_summaryLabel = new QLabel;
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setOpenExternalLinks(true);
    layout->addWidget(m_summaryLabel);
    layout->addStretch();
}

void DonePage::initializePage()
{
    QString user = field("github_user").toString();
    QString repo = field("repo_url").toString();
    bool hasWatch = !field("watch_skip").toBool()
                 && !field("watch_path").toString().trimmed().isEmpty();

    QString s =
        "<h3>fangit is ready!</h3>"
        "<p><b>GitHub user:</b> @" + user + "</p>"
        "<p><b>Repository:</b> " + repo + "</p>";

    if (hasWatch)
        s += "<p><b>Watching:</b> " + field("watch_path").toString() + "</p>";

    s +=
        "<hr>"
        "<p><b>Next steps:</b></p>"
        "<ol>"
        "<li>fangit will appear in your menu bar — look for the status dot</li>"
        "<li>A test notification was sent — check your phone in ~30-60 seconds</li>"
        "<li>Edit <b>config.toml</b> from the tray menu to add more watch directories and channels</li>"
        "<li>And make sure you install the <a href='https://apps.apple.com/app/github/id1477376905'>GitHub mobile app</a> "
        "on your phone and enable notifications both in app and phone settings!</li>"
        "</ol>"
        + mutedHtml(
            "To run this wizard again, move or delete config.toml and restart fangit."
        );

    m_summaryLabel->setText(s);
}
