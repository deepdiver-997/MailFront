#ifndef MAILAPP_H
#define MAILAPP_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QComboBox>

#include "widgets/foldertree.h"
#include "widgets/mailpreview.h"
#include "models/maillistmodel.h"
#include "core/email.h"
#include "core/folder.h"

// 前向声明（减少头文件依赖）
class SmtpClient;
class ImapClient;
class MailStore;
class AccountManager;
class ComposeWindow;

QT_BEGIN_NAMESPACE
namespace Ui {
class mailApp;
}
QT_END_NAMESPACE

class mailApp : public QMainWindow
{
    Q_OBJECT

public:
    mailApp(QWidget *parent = nullptr);
    ~mailApp();

private slots:
    void onFolderSelected(const QString &path);
    void onMailSelected(const QModelIndex &index);
    void onNewMailAction();
    void onReplyAction();
    void onComposeSendRequested(const Email &email);

    // 网络回调
    void onEmailSent();
    void onEmailError(const QString &error);
    void onFoldersFetched(const QList<Folder> &folders);
    void onEmailsFetched(const QString &folderPath, const QList<Email> &emails);
    void onEmailBodyFetched(const QString &messageId, const QString &body);
    void onDeleteAction();
    void onEmailDeleted(const QString &uid);
    void onAccountSwitched(int index);
    void onAccountsChanged();

    // 刷新收件箱
    void onRefreshAction();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void initServices();
    void rebuildAccountCombo();

    Ui::mailApp *ui;

    // 3 栏布局
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    FolderTree *m_folderTree;
    QListView *m_mailListView;
    MailPreview *m_mailPreview;

    MailListModel *m_mailListModel;

    // 工具栏动作
    QAction *m_newMailAction;
    QAction *m_replyAction;
    QAction *m_deleteAction;

    // 服务层
    MailStore *m_mailStore;
    AccountManager *m_accountManager;

    // 网络层
    SmtpClient *m_smtpClient;
    ImapClient *m_imapClient;

    // 当前选中的文件夹
    QString m_currentFolder;

    // 正文加载跟踪
    QString m_pendingBodyMessageId;

    // 账号切换
    QComboBox *m_accountCombo;
    int m_currentAccountIndex = -1;
    QString m_currentAccountEmail;
    QString m_fetchAccountEmail;  // 防止旧请求覆盖新账号数据
};

#endif // MAILAPP_H
