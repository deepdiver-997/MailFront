#ifndef MAILAPP_H
#define MAILAPP_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QSplitter>
#include <QToolBar>
#include <QAction>

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

    // 刷新收件箱
    void onRefreshAction();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void initServices();

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
};

#endif // MAILAPP_H
