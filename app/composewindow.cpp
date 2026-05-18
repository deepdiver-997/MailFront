#include "composewindow.h"
#include "services/accountmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>
#include <QMessageBox>
#include <QStatusBar>

ComposeWindow::ComposeWindow(AccountManager *accountMgr, QWidget *parent)
    : QMainWindow(parent)
    , m_mgr(accountMgr)
{
    setWindowTitle("新邮件");
    resize(700, 500);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
    setupToolBar();

    statusBar()->showMessage("就绪");
}

void ComposeWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // 发件人选择（多账户）
    auto *fromLayout = new QHBoxLayout();
    auto *fromLabel = new QLabel("发件人:", this);
    fromLabel->setFixedWidth(50);
    m_fromCombo = new QComboBox(this);
    QList<Account> accounts = m_mgr->accounts();
    if (accounts.isEmpty()) {
        m_fromCombo->addItem("(未配置账户)");
    } else {
        for (const Account &acc : accounts) {
            m_fromCombo->addItem(acc.email);
        }
        // 默认选中默认账户
        Account def = m_mgr->defaultAccount();
        int idx = -1;
        for (int i = 0; i < accounts.size(); ++i) {
            if (accounts[i].email == def.email) { idx = i; break; }
        }
        if (idx >= 0) m_fromCombo->setCurrentIndex(idx);
    }
    fromLayout->addWidget(fromLabel);
    fromLayout->addWidget(m_fromCombo);

    // 收件人
    auto *toLayout = new QHBoxLayout();
    auto *toLabel = new QLabel("收件人:", this);
    toLabel->setFixedWidth(50);
    m_toEdit = new QLineEdit(this);
    m_toEdit->setPlaceholderText("recipient@example.com");
    toLayout->addWidget(toLabel);
    toLayout->addWidget(m_toEdit);

    // 抄送
    auto *ccLayout = new QHBoxLayout();
    auto *ccLabel = new QLabel("抄送:", this);
    ccLabel->setFixedWidth(50);
    m_ccEdit = new QLineEdit(this);
    m_ccEdit->setPlaceholderText("cc@example.com (可选)");
    ccLayout->addWidget(ccLabel);
    ccLayout->addWidget(m_ccEdit);

    // 主题
    auto *subjectLayout = new QHBoxLayout();
    auto *subjectLabel = new QLabel("主题:", this);
    subjectLabel->setFixedWidth(50);
    m_subjectEdit = new QLineEdit(this);
    m_subjectEdit->setPlaceholderText("邮件主题");
    subjectLayout->addWidget(subjectLabel);
    subjectLayout->addWidget(m_subjectEdit);

    // 正文（富文本编辑器）
    m_bodyEdit = new QTextEdit(this);
    m_bodyEdit->setPlaceholderText("输入邮件正文...");
    m_bodyEdit->setAcceptRichText(true);

    layout->addLayout(fromLayout);
    layout->addLayout(toLayout);
    layout->addLayout(ccLayout);
    layout->addLayout(subjectLayout);
    layout->addWidget(m_bodyEdit, 1);

    setCentralWidget(centralWidget);
}

void ComposeWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar("发送工具栏");
    toolbar->setMovable(false);

    m_sendAction = toolbar->addAction("发送");
    m_sendAction->setShortcut(QKeySequence("Ctrl+Return"));

    connect(m_sendAction, &QAction::triggered, this, &ComposeWindow::onSendClicked);
}

void ComposeWindow::setReplyMode(const Email &originalEmail)
{
    setWindowTitle("回复: " + originalEmail.subject);
    m_toEdit->setText(originalEmail.from);
    m_subjectEdit->setText("Re: " + originalEmail.subject);

    QString quote = QString(
        "<br><br>-------- 原始邮件 --------<br>"
        "<b>发件人:</b> %1<br>"
        "<b>日期:</b> %2<br>"
        "<b>主题:</b> %3<br><br>"
        "%4"
    ).arg(originalEmail.from,
          originalEmail.date.toString("yyyy-MM-dd hh:mm"),
          originalEmail.subject,
          originalEmail.body);
    m_bodyEdit->setHtml(quote);

    QTextCursor cursor = m_bodyEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_bodyEdit->setTextCursor(cursor);
}

void ComposeWindow::onSendClicked()
{
    if (m_toEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入收件人地址");
        m_toEdit->setFocus();
        return;
    }
    if (m_subjectEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入邮件主题");
        m_subjectEdit->setFocus();
        return;
    }

    Email email;
    email.from    = m_fromCombo->currentText();  // 发件人
    email.to      = m_toEdit->text().split(";", Qt::SkipEmptyParts);
    email.subject = m_subjectEdit->text().trimmed();
    email.body    = m_bodyEdit->toHtml();
    email.date    = QDateTime::currentDateTime();
    email.folder  = "Sent";

    emit sendRequested(email);
    close();
}
