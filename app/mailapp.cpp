#include "mailapp.h"
#include "./ui_mailapp.h"

#include <QVBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>

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

    m_newMailAction = toolbar->addAction("新邮件");
    m_replyAction    = toolbar->addAction("回复");
    m_replyAction->setEnabled(false);
    m_deleteAction   = toolbar->addAction("删除");
    m_deleteAction->setEnabled(false);

    toolbar->addSeparator();

    QAction *refreshAction = toolbar->addAction("刷新");
    connect(refreshAction, &QAction::triggered, this, &mailApp::onRefreshAction);
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

    // ---- 工具栏 ----
    connect(m_newMailAction, &QAction::triggered,
            this, &mailApp::onNewMailAction);
    connect(m_replyAction, &QAction::triggered,
            this, &mailApp::onReplyAction);

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

    // 从本地 SQLite 加载邮件
    QList<Email> emails = m_mailStore->getEmails(path, 50);
    m_mailListModel->setEmails(emails);

    bool hasMail = !emails.isEmpty();
    m_replyAction->setEnabled(hasMail);
    m_deleteAction->setEnabled(hasMail);

    // 更新未读数显示
    int unread = m_mailStore->getUnreadCount(path);
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
        m_mailListModel->setEmails(m_mailStore->getEmails(m_currentFolder, 50));
    }

    statusBar()->showMessage(QString("来自: %1  |  %2")
                             .arg(email.from, email.subject));
}

void mailApp::onRefreshAction()
{
    if (m_currentFolder.isEmpty()) {
        m_currentFolder = "INBOX";
    }

    Account acc = m_accountManager->defaultAccount();
    if (!acc.isValid()) {
        statusBar()->showMessage("请先配置邮件账户（设置 → 账户）", 5000);
        return;
    }

    statusBar()->showMessage("正在刷新...");
    m_imapClient->fetchEmails(acc, m_currentFolder, 50);
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

    m_mailStore->saveEmail(email);
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
    statusBar()->showMessage(QString("获取到 %1 个文件夹").arg(folders.size()), 3000);

    for (const Folder &f : folders) {
        m_folderTree->addFolder(f.name, f.path, f.unreadCount);
    }
}

void mailApp::onEmailsFetched(const QString &folderPath, const QList<Email> &emails)
{
    statusBar()->showMessage(QString("收到 %1 封邮件").arg(emails.size()), 3000);

    // 保存到本地数据库
    m_mailStore->saveEmails(emails);

    // 如果当前正在看这个文件夹，更新列表
    if (folderPath == m_currentFolder) {
        m_mailListModel->setEmails(emails);
        m_replyAction->setEnabled(!emails.isEmpty());
        m_deleteAction->setEnabled(!emails.isEmpty());
    }

    int unread = m_mailStore->getUnreadCount(folderPath);
    m_folderTree->updateUnreadCount(folderPath, unread);
}
