#pragma once

#include <QWizard>
#include <QWizardPage>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

class QNetworkAccessManager;
class QNetworkReply;
class ConfigManager;
class GitManager;
class WorkflowManager;

// ── The Wizard ─────────────────────────────────────────────────────────────

class SetupWizard : public QWizard
{
    Q_OBJECT

public:
    explicit SetupWizard(ConfigManager &config, GitManager &git,
                         WorkflowManager &workflow, QWidget *parent = nullptr);

    enum PageId {
        Page_Welcome,
        Page_GitCheck,
        Page_GitHub,
        Page_Repo,
        Page_Auth,
        Page_TestConnection,
        Page_SetupRepo,
        Page_WatchDir,
        Page_Done
    };

    bool wasCompleted() const { return m_completed; }

private:
    void accept() override;

    ConfigManager &m_config;
    GitManager &m_git;
    WorkflowManager &m_workflow;
    bool m_completed = false;
};

// ── Individual Pages ───────────────────────────────────────────────────────

class WelcomePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
};

class GitCheckPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit GitCheckPage(GitManager &git, QWidget *parent = nullptr);
    void initializePage() override;
    bool isComplete() const override;
private:
    GitManager &m_git;
    QLabel *m_statusLabel;
    QLabel *m_helpLabel;
    bool m_gitFound = false;
};

class GitHubPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit GitHubPage(QWidget *parent = nullptr);
    bool isComplete() const override;
private:
    QLineEdit *m_usernameEdit;
};

class RepoPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit RepoPage(QWidget *parent = nullptr);
    bool isComplete() const override;
private:
    QLineEdit *m_repoUrlEdit;
    QComboBox *m_branchCombo;
};

class AuthPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit AuthPage(QWidget *parent = nullptr);
    void initializePage() override;
    bool isComplete() const override;
private:
    void detectCredentials();
    QLabel *m_statusLabel;
    QLabel *m_privacyLabel;
    QLabel *m_patInstructions;
    QPushButton *m_openGitHubBtn;
    QPushButton *m_openKeychainBtn;
    bool m_credentialsDetected = false;
};

class TestConnectionPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit TestConnectionPage(GitManager &git, ConfigManager &config,
                                 QWidget *parent = nullptr);
    void initializePage() override;
    bool isComplete() const override;
private:
    void runTest();
    GitManager &m_git;
    ConfigManager &m_config;
    QLabel *m_statusLabel;
    QTextEdit *m_logOutput;
    QProgressBar *m_progressBar;
    QPushButton *m_retryBtn;
    bool m_success = false;
};

class SetupRepoPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SetupRepoPage(GitManager &git, ConfigManager &config,
                            WorkflowManager &workflow, QWidget *parent = nullptr);
    void initializePage() override;
    bool isComplete() const override;
private:
    void startSetup();
    void stepRetrievePat();
    void stepCheckNextIssue();
    void stepCreateIssue(int issueNum, const QString &title);
    void stepDispatchTest();
    void finishSetup();

    GitManager &m_git;
    ConfigManager &m_config;
    WorkflowManager &m_workflow;
    QLabel *m_statusLabel;
    QTextEdit *m_logOutput;
    QProgressBar *m_progressBar;

    QNetworkAccessManager *m_net = nullptr;
    QString m_pat;
    QString m_owner;
    QString m_repo;
    bool m_workflowOk = false;
    int m_issueIndex = 0;

    struct IssueTemplate { QString title; int num; };
    QList<IssueTemplate> m_issues;

    bool m_success = false;
};

class WatchDirPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WatchDirPage(QWidget *parent = nullptr);
    bool isComplete() const override;
private:
    QLineEdit *m_pathNameEdit;
    QLineEdit *m_pathEdit;
    QPushButton *m_browseBtn;
    QLineEdit *m_extensionsEdit;
    QCheckBox *m_skipCheck;
};

class DonePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit DonePage(QWidget *parent = nullptr);
    void initializePage() override;
private:
    QLabel *m_summaryLabel;
};
