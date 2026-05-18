#ifndef IMAPCLIENT_H
#define IMAPCLIENT_H

#include <QObject>
#include <QFutureWatcher>
#include <QList>
#include <vmime/vmime.hpp>
#include "core/email.h"
#include "core/account.h"
#include "core/folder.h"

// ============================================================
// Qt 知识点：异步 IMAP 客户端
// 和 SmtpClient 相同模式——阻塞 I/O 丢线程池，结果用信号通知 UI
// ============================================================
class ImapClient : public QObject
{
    Q_OBJECT

public:
    explicit ImapClient(QObject *parent = nullptr);

    // 连接并获取文件夹列表
    void fetchFolders(const Account &account);
    // 获取指定文件夹的邮件
    void fetchEmails(const Account &account, const QString &folderPath, int count = 50);
    // 获取指定邮件的完整内容
    void fetchEmailBody(const Account &account, const QString &folderPath, const QString &messageId);

    bool isBusy() const;

signals:
    void foldersFetched(const QList<Folder> &folders);
    void emailsFetched(const QString &folderPath, const QList<Email> &emails);
    void emailBodyFetched(const QString &messageId, const QString &body);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onFoldersTaskFinished();
    void onEmailsTaskFinished();
    void onEmailBodyTaskFinished();

private:
    QList<Folder> doFetchFolders(const Account &account);
    QList<Email> doFetchEmails(const Account &account, const QString &folderPath, int count);
    QString doFetchEmailBody(const Account &account, const QString &folderPath, const QString &messageId);

    QFutureWatcher<QList<Folder>> *m_folderWatcher;
    QFutureWatcher<QList<Email>> *m_emailWatcher;
    QFutureWatcher<QString> *m_bodyWatcher;
};

#endif // IMAPCLIENT_H
