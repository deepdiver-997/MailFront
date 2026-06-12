#include "mailapp.h"
#include "./ui_mailapp.h"

#include <QVBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QSet>

#include "services/mailstore.h"
#include "services/accountmanager.h"
#include "network/smtpclient.h"
#include "network/imapclient.h"
#include "app/composewindow.h"
#include "app/settingsdialog.h"
#include "core/folder.h"

mailApp::mailApp(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::mailApp)
    , m_mainSplitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_folderTree(nullptr)
    , m_mailListView(nullptr)
    , m_mailPreview(nullptr)
    , m_mailListModel(nullptr)
    , m_newMailAction(nullptr)
    , m_replyAction(nullptr)
    , m_deleteAction(nullptr)
    , m_mailStore(nullptr)
    , m_accountManager(nullptr)
    , m_smtpClient(nullptr)
    , m_imapClient(nullptr)
{
    ui->setupUi(this);
    initServices();
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();

    // 初始加载默认账号
    if (m_accountCombo->count() > 0) {
        onAccountSwitched(m_accountCombo->currentIndex());
    }

    statusBar()->showMessage("就绪 - MailFront 邮件客户端", 3000);
}

mailApp::~mailApp()
{
    delete ui;
}

// ============================================================
// 服务初始化：创建 MailStore / AccountManager / 网络客户端
// ============================================================
void mailApp::initServices()
{
    // 数据存储目录
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // MailStore：SQLite 邮件缓存
    m_mailStore = new MailStore(this);
    m_mailStore->init(dataDir + "/mails.db");

    // AccountManager：账户配置（QSettings 持久化）
    m_accountManager = new AccountManager(this);

    // 网络客户端
    m_smtpClient = new SmtpClient(this);
    m_imapClient = new ImapClient(this);

    // 初始填充账号列表（setupToolBar 中 m_accountCombo 已创建）
}

// ============================================================
// 布局管理：QSplitter 3 栏布局
// ============================================================
void mailApp::setupUi()
{
    m_folderTree    = new FolderTree(this);
    m_mailListView  = new QListView(this);
    m_mailPreview   = new MailPreview(this);
    m_mailListModel = new MailListModel(this);

    m_mailListView->setModel(m_mailListModel);
    m_mailListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mailListView->setSelectionMode(QAbstractItemView::SingleSelection);

    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_mailListView);
    m_rightSplitter->addWidget(m_mailPreview);
    m_rightSplitter->setStretchFactor(0, 2);
    m_rightSplitter->setStretchFactor(1, 3);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_folderTree);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);

    setCentralWidget(m_mainSplitter);
}

void mailApp::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");

    QAction *newMailAction = fileMenu->addAction("新邮件(&N)");
    newMailAction->setShortcut(QKeySequence("Ctrl+N"));
    connect(newMailAction, &QAction::triggered, this, &mailApp::onNewMailAction);

    fileMenu->addSeparator();
    fileMenu->addAction("设置(&S)", this, [this]() {
        SettingsDialog dlg(m_accountManager, this);
        dlg.exec();
    });
    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("退出(&Q)", QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void mailApp::setupToolBar()
{
    QToolBar *toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));

    // 账号切换下拉框
    m_accountCombo = new QComboBox(this);
    m_accountCombo->setMinimumWidth(200);
    m_accountCombo->setToolTip("切换邮件账户");
    toolbar->addWidget(m_accountCombo);
    toolbar->addSeparator();

    m_newMailAction = toolbar->addAction("新邮件");
    m_replyAction    = toolbar->addAction("回复");
    m_replyAction->setEnabled(false);
    m_deleteAction   = toolbar->addAction("删除");
    m_deleteAction->setEnabled(false);

    toolbar->addSeparator();

    QAction *refreshAction = toolbar->addAction("刷新");
    connect(refreshAction, &QAction::triggered, this, &mailApp::onRefreshAction);

    // 填充账号列表
    rebuildAccountCombo();
}

void mailApp::rebuildAccountCombo()
{
    m_accountCombo->blockSignals(true);
    m_accountCombo->clear();

    QList<Account> accounts = m_accountManager->accounts();
    for (const Account &acc : accounts) {
        m_accountCombo->addItem(acc.email);
    }

    // 恢复默认选中
    int defIdx = m_accountManager->defaultAccountIndex();
    if (defIdx >= 0 && defIdx < m_accountCombo->count()) {
        m_accountCombo->setCurrentIndex(defIdx);
    }

    m_accountCombo->blockSignals(false);
}

void mailApp::setupStatusBar()
{
    statusBar()->showMessage("欢迎使用 MailFront");
}

// ============================================================
// Qt 知识点：信号槽 (Signal & Slot)
//
// 这里展示了完整的信号槽连线图：
//
//   UI 事件  ──→  Service  ──→  Network（线程池）
//       ↑                          │
//       └──── signal callback ─────┘
//
// 信号从工作线程发出，Qt 自动投递到 UI 线程的槽函数
// ============================================================
void mailApp::setupConnections()
{
    // ---- UI 交互 ----
    connect(m_folderTree, &FolderTree::folderSelected,
            this, &mailApp::onFolderSelected);
    connect(m_mailListView, &QListView::clicked,
            this, &mailApp::onMailSelected);

    // ---- 账号切换 ----
    connect(m_accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &mailApp::onAccountSwitched);
    connect(m_accountManager, &AccountManager::accountsChanged,
            this, &mailApp::onAccountsChanged);

    // ---- 工具栏 ----
    connect(m_newMailAction, &QAction::triggered,
            this, &mailApp::onNewMailAction);
    connect(m_replyAction, &QAction::triggered,
            this, &mailApp::onReplyAction);
    connect(m_deleteAction, &QAction::triggered,
            this, &mailApp::onDeleteAction);

    // ---- 网络层信号 → UI 更新 ----
    // SMTP
    connect(m_smtpClient, &SmtpClient::emailSent,
            this, &mailApp::onEmailSent);
    connect(m_smtpClient, &SmtpClient::errorOccurred,
            this, &mailApp::onEmailError);

    // IMAP
    connect(m_imapClient, &ImapClient::foldersFetched,
            this, &mailApp::onFoldersFetched);
    connect(m_imapClient, &ImapClient::emailsFetched,
            this, &mailApp::onEmailsFetched);
    connect(m_imapClient, &ImapClient::emailBodyFetched,
            this, &mailApp::onEmailBodyFetched);
    connect(m_imapClient, &ImapClient::emailDeleted,
            this, &mailApp::onEmailDeleted);
    connect(m_imapClient, &ImapClient::errorOccurred,
            this, &mailApp::onEmailError);
}

// ============================================================
// 槽函数实现
// ============================================================

void mailApp::onFolderSelected(const QString &path)
{
    m_currentFolder = path;
    statusBar()->showMessage(QString("切换到文件夹: %1").arg(path));

    // 从本地 SQLite 加载邮件（按账号过滤）
    QList<Email> emails = m_mailStore->getEmails(path, m_currentAccountEmail, 50);
    m_mailListModel->setEmails(emails);

    bool hasMail = !emails.isEmpty();
    m_replyAction->setEnabled(hasMail);
    m_deleteAction->setEnabled(hasMail);

    // 更新未读数显示
    int unread = m_mailStore->getUnreadCount(path, m_currentAccountEmail);
    m_folderTree->updateUnreadCount(path, unread);
}

void mailApp::onMailSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;

    Email email = m_mailListModel->emailAt(index.row());
    m_mailPreview->showEmail(email);

    // 标记已读
    if (!email.isRead && email.id > 0) {
        m_mailStore->markAsRead(email.id);
        email.isRead = true;
        // 更新 Model 使字体从粗变正常
        m_mailListModel->setEmails(m_mailStore->getEmails(m_currentFolder, m_currentAccountEmail, 50));
    }

    // 正文为空时异步加载
    if (email.body.isEmpty() && !email.messageId.isEmpty()) {
        QList<Account> accounts = m_accountManager->accounts();
        Account acc;
        if (m_currentAccountIndex >= 0 && m_currentAccountIndex < accounts.size()) {
            acc = accounts[m_currentAccountIndex];
        }
        if (acc.isValid()) {
            m_pendingBodyMessageId = email.messageId;
            m_imapClient->fetchEmailBody(acc, email.folder, email.messageId);
        }
    }

    statusBar()->showMessage(QString("来自: %1  |  %2")
                             .arg(email.from, email.subject));
}

void mailApp::onRefreshAction()
{
    if (m_currentFolder.isEmpty()) {
        m_currentFolder = "INBOX";
    }

    QList<Account> accounts = m_accountManager->accounts();
    Account acc;
    if (m_currentAccountIndex >= 0 && m_currentAccountIndex < accounts.size()) {
        acc = accounts[m_currentAccountIndex];
    } else {
        acc = m_accountManager->defaultAccount();
    }
    if (!acc.isValid()) {
        statusBar()->showMessage("请先配置邮件账户（设置 → 账户）", 5000);
        return;
    }

    m_fetchAccountEmail = acc.email;
    statusBar()->showMessage(QString("正在刷新 %1...").arg(acc.email));
    m_imapClient->fetchFolders(acc);
    // 增量同步：只拉本地没有的邮件
    QSet<QString> knownUIDs = m_mailStore->getMessageIds(m_currentFolder, acc.email);
    m_imapClient->fetchNewEmails(acc, m_currentFolder, knownUIDs);
}

void mailApp::onNewMailAction()
{
    auto *composeWin = new ComposeWindow(m_accountManager, this);
    connect(composeWin, &ComposeWindow::sendRequested,
            this, &mailApp::onComposeSendRequested);
    composeWin->show();
}

void mailApp::onReplyAction()
{
    QModelIndex index = m_mailListView->currentIndex();
    if (!index.isValid()) return;

    Email original = m_mailListModel->emailAt(index.row());

    auto *composeWin = new ComposeWindow(m_accountManager, this);
    composeWin->setReplyMode(original);
    connect(composeWin, &ComposeWindow::sendRequested,
            this, &mailApp::onComposeSendRequested);
    composeWin->show();
}

void mailApp::onComposeSendRequested(const Email &email)
{
    // 根据发件人地址找到对应账户
    Account acc;
    QList<Account> accounts = m_accountManager->accounts();
    for (const Account &a : accounts) {
        if (a.email == email.from) {
            acc = a;
            break;
        }
    }
    if (!acc.isValid()) {
        acc = m_accountManager->defaultAccount();
    }
    if (!acc.isValid()) {
        QMessageBox::warning(this, "提示",
            "请先配置邮件账户（文件 → 设置）");
        return;
    }

    m_mailStore->saveEmail(email, acc.email);
    m_smtpClient->sendEmail(acc, email);
    statusBar()->showMessage("正在发送邮件...");
}

// ---- 网络回调 ----

void mailApp::onEmailSent()
{
    statusBar()->showMessage("邮件发送成功", 3000);
}

void mailApp::onEmailError(const QString &error)
{
    statusBar()->showMessage("错误: " + error, 10000);
}

void mailApp::onFoldersFetched(const QList<Folder> &folders)
{
    qDebug() << "[onFoldersFetched] folderCount=" << folders.size()
             << "fetchAccount=" << m_fetchAccountEmail
             << "currentAccount=" << m_currentAccountEmail;

    // 防止旧账号的响应覆盖当前账号
    if (m_fetchAccountEmail != m_currentAccountEmail) {
        qDebug() << "[onFoldersFetched] SKIPPED - account mismatch";
        return;
    }

    statusBar()->showMessage(QString("获取到 %1 个文件夹").arg(folders.size()), 3000);

    // 清除所有旧文件夹，以服务端数据为准重建
    m_folderTree->clear();

    // 按显示名去重（服务端可能对同一邮箱返回 INBOX + 中文名两个条目）
    QSet<QString> seenNames;
    for (const Folder &f : folders) {
        if (seenNames.contains(f.name))
            continue;
        seenNames.insert(f.name);

        m_folderTree->addFolder(f.name, f.path, f.unreadCount);
        int unread = m_mailStore->getUnreadCount(f.path, m_currentAccountEmail);
        if (unread > 0) {
            m_folderTree->updateUnreadCount(f.path, unread);
        }
    }

    // 如果当前无选中文件夹，自动选第一个
    if (m_currentFolder.isEmpty() && !folders.isEmpty()) {
        m_currentFolder = folders.first().path;
        QList<Email> emails = m_mailStore->getEmails(m_currentFolder, m_currentAccountEmail, 50);
        m_mailListModel->setEmails(emails);
        m_replyAction->setEnabled(!emails.isEmpty());
        m_deleteAction->setEnabled(!emails.isEmpty());
    }
}

void mailApp::onEmailsFetched(const QString &folderPath, const QList<Email> &emails)
{
    qDebug() << "[onEmailsFetched] folder=" << folderPath
             << "count=" << emails.size()
             << "fetchAccount=" << m_fetchAccountEmail
             << "currentAccount=" << m_currentAccountEmail
             << "currentFolder=" << m_currentFolder;

    // 防止旧账号的响应覆盖当前账号
    if (m_fetchAccountEmail != m_currentAccountEmail) {
        qDebug() << "[onEmailsFetched] SKIPPED - account mismatch";
        return;
    }

    statusBar()->showMessage(QString("收到 %1 封邮件").arg(emails.size()), 3000);

    // 保存到本地数据库（关联当前账号）
    m_mailStore->saveEmails(emails, m_currentAccountEmail);

    // 如果当前正在看这个文件夹，更新列表
    if (folderPath == m_currentFolder) {
        QList<Email> dbEmails = m_mailStore->getEmails(folderPath, m_currentAccountEmail, 50);
        m_mailListModel->setEmails(dbEmails);
        m_replyAction->setEnabled(!dbEmails.isEmpty());
        m_deleteAction->setEnabled(!dbEmails.isEmpty());
    }

    int unread = m_mailStore->getUnreadCount(folderPath, m_currentAccountEmail);
    m_folderTree->updateUnreadCount(folderPath, unread);
}

void mailApp::onEmailBodyFetched(const QString &messageId, const QString &body)
{
    // 在 model 中查找并更新正文
    for (int i = 0; i < m_mailListModel->rowCount(); ++i) {
        Email e = m_mailListModel->emailAt(i);
        if (e.messageId == messageId) {
            // 更新 SQLite 缓存
            e.body = body;
            m_mailStore->saveEmail(e, m_currentAccountEmail);
            // 更新 model 显示
            m_mailListModel->updateEmailBody(i, body);
            // 如果当前预览正是这封邮件，刷新预览
            QModelIndex curIdx = m_mailListView->currentIndex();
            if (curIdx.isValid() && curIdx.row() == i) {
                m_mailPreview->showEmail(e);
            }
            break;
        }
    }
}

void mailApp::onDeleteAction()
{
    QModelIndex index = m_mailListView->currentIndex();
    if (!index.isValid()) return;

    Email email = m_mailListModel->emailAt(index.row());
    if (email.id <= 0) return;

    // 本地删除
    m_mailStore->deleteEmail(email.id);

    // IMAP 删除
    QList<Account> accounts = m_accountManager->accounts();
    Account acc;
    if (m_currentAccountIndex >= 0 && m_currentAccountIndex < accounts.size()) {
        acc = accounts[m_currentAccountIndex];
    }
    if (acc.isValid() && !email.messageId.isEmpty()) {
        m_imapClient->deleteEmail(acc, email.folder, email.messageId);
    }

    // 刷新列表
    QList<Email> emails = m_mailStore->getEmails(m_currentFolder, m_currentAccountEmail, 50);
    m_mailListModel->setEmails(emails);
    m_mailPreview->clear();

    bool hasMail = !emails.isEmpty();
    m_replyAction->setEnabled(hasMail);
    m_deleteAction->setEnabled(hasMail);

    statusBar()->showMessage("邮件已删除", 3000);
}

void mailApp::onEmailDeleted(const QString &uid)
{
    Q_UNUSED(uid);
    int unread = m_mailStore->getUnreadCount(m_currentFolder, m_currentAccountEmail);
    m_folderTree->updateUnreadCount(m_currentFolder, unread);
}

void mailApp::onAccountSwitched(int index)
{
    if (index < 0 || index >= m_accountManager->accountCount()) {
        // 无账号
        m_currentAccountIndex = -1;
        m_currentAccountEmail.clear();
        m_currentFolder.clear();
        m_folderTree->clear();
        m_mailListModel->clear();
        m_mailPreview->clear();
        m_newMailAction->setEnabled(false);
        m_replyAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        setWindowTitle("MailFront - 邮件客户端");
        statusBar()->showMessage("请先配置邮件账户（文件 → 设置）", 0);
        return;
    }

    m_currentAccountIndex = index;
    Account acc = m_accountManager->accounts().at(index);
    m_currentAccountEmail = acc.email;

    // 清除当前视图
    m_folderTree->clear();
    m_mailListModel->clear();
    m_mailPreview->clear();
    m_currentFolder.clear();

    setWindowTitle(QString("MailFront - %1").arg(acc.email));
    m_newMailAction->setEnabled(true);

    if (!acc.isValid()) return;

    // 先从缓存加载文件夹（离线回退），IMAP 成功后会覆盖
    QStringList cachedFolders = m_mailStore->getDistinctFolders(acc.email);
    if (!cachedFolders.isEmpty()) {
        for (const QString &f : cachedFolders) {
            QString displayName = folderDisplayName(f, f);
            m_folderTree->addFolder(displayName, f, 0);
        }
    }

    // 始终从 INBOX 开始——IMAP 服务器只认 "INBOX" 这个英文名
    m_currentFolder = "INBOX";

    // 加载 INBOX 缓存邮件
    QList<Email> cachedEmails = m_mailStore->getEmails("INBOX", acc.email, 50);
    m_mailListModel->setEmails(cachedEmails);

    // 尝试从 IMAP 获取最新数据
    m_fetchAccountEmail = acc.email;
    statusBar()->showMessage(QString("正在连接 %1 ...").arg(acc.email));
    m_imapClient->fetchFolders(acc);
    m_imapClient->fetchEmails(acc, "INBOX", 50);
}

void mailApp::onAccountsChanged()
{
    rebuildAccountCombo();

    // 如果当前无账号，清空视图
    if (m_accountManager->accountCount() == 0) {
        onAccountSwitched(-1);
    }
}
