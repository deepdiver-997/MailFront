#include "imapclient.h"
#include <QtConcurrent/QtConcurrent>
#include <vmime/net/imap/IMAPStore.hpp>
#include <vmime/message.hpp>
#include <vmime/messageParser.hpp>
#include <vmime/net/message.hpp>
#include <vmime/utility/outputStreamStringAdapter.hpp>

ImapClient::ImapClient(QObject *parent)
    : QObject(parent)
    , m_folderWatcher(new QFutureWatcher<QList<Folder>>(this))
    , m_emailWatcher(new QFutureWatcher<QList<Email>>(this))
    , m_bodyWatcher(new QFutureWatcher<QString>(this))
{
    connect(m_folderWatcher, &QFutureWatcher<QList<Folder>>::finished,
            this, &ImapClient::onFoldersTaskFinished);
    connect(m_emailWatcher, &QFutureWatcher<QList<Email>>::finished,
            this, &ImapClient::onEmailsTaskFinished);
    connect(m_bodyWatcher, &QFutureWatcher<QString>::finished,
            this, &ImapClient::onEmailBodyTaskFinished);
}

bool ImapClient::isBusy() const
{
    return m_folderWatcher->isRunning()
        || m_emailWatcher->isRunning()
        || m_bodyWatcher->isRunning();
}

void ImapClient::fetchFolders(const Account &account)
{
    QFuture<QList<Folder>> future = QtConcurrent::run(
        &ImapClient::doFetchFolders, this, account);
    m_folderWatcher->setFuture(future);
}

void ImapClient::fetchEmails(const Account &account, const QString &folderPath, int count)
{
    QFuture<QList<Email>> future = QtConcurrent::run(
        &ImapClient::doFetchEmails, this, account, folderPath, count);
    m_emailWatcher->setFuture(future);
}

void ImapClient::fetchEmailBody(const Account &account, const QString &folderPath,
                                 const QString &messageId)
{
    QFuture<QString> future = QtConcurrent::run(
        &ImapClient::doFetchEmailBody, this, account, folderPath, messageId);
    m_bodyWatcher->setFuture(future);
}

// ---- 槽：任务完成 ----

void ImapClient::onFoldersTaskFinished()
{
    QList<Folder> folders = m_folderWatcher->result();
    if (!folders.isEmpty()) {
        emit foldersFetched(folders);
    }
}

void ImapClient::onEmailsTaskFinished()
{
    QList<Email> emails = m_emailWatcher->result();
    if (!emails.isEmpty()) {
        QString folderPath = emails.first().folder;
        emit emailsFetched(folderPath, emails);
    }
}

void ImapClient::onEmailBodyTaskFinished()
{
    QString body = m_bodyWatcher->result();
    if (!body.isEmpty()) {
        emit emailBodyFetched("", body);
    }
}

// ---- 阻塞 I/O（线程池中执行） ----

QList<Folder> ImapClient::doFetchFolders(const Account &account)
{
    QList<Folder> folders;
    try {
        auto session = vmime::net::session::create();

        // IMAP 认证属性（使用命名空间前缀确保 VMime 正确识别）
        session->getProperties()["store.imap.auth.username"] = account.imapUser.toStdString();
        session->getProperties()["store.imap.auth.password"] = account.imapPassword.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;

        // TLS 配置：根据 account 设置开关
        if (!account.imapUseTls) {
            session->getProperties()["store.imap.options.connection.tls"] = false;
            session->getProperties()["store.imap.options.connection.tls-starttls"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto rootFolder = store->getRootFolder();
        auto subFolders = rootFolder->getFolders(true);

        int id = 1;
        for (const auto &f : subFolders) {
            Folder folder;
            folder.id = id++;
            folder.name = QString::fromStdString(f->getName().getBuffer());
            folder.path = QString::fromStdString(
                f->getFullPath().toString("/", vmime::charset::getLocalCharset()));
            try {
                folder.totalCount = static_cast<int>(f->getMessageCount());
            } catch (...) {
                folder.totalCount = 0;
            }
            folder.isSystem = (folder.path == "INBOX"
                            || folder.path.contains("Sent")
                            || folder.path.contains("Drafts")
                            || folder.path.contains("Trash"));
            folders.append(folder);
        }

        store->disconnect();
    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 获取文件夹失败: %1")
                           .arg(QString::fromStdString(e.what())));
    }
    return folders;
}

// 辅助函数：从 contentHandler 提取字符串
static QString extractContent(const vmime::shared_ptr<const vmime::contentHandler> &ct)
{
    std::string buffer;
    vmime::utility::outputStreamStringAdapter adapter(buffer);
    ct->extract(adapter);
    return QString::fromStdString(buffer);
}

QList<Email> ImapClient::doFetchEmails(const Account &account, const QString &folderPath, int count)
{
    QList<Email> emails;
    try {
        auto session = vmime::net::session::create();

        session->getProperties()["store.imap.auth.username"] = account.imapUser.toStdString();
        session->getProperties()["store.imap.auth.password"] = account.imapPassword.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;

        if (!account.imapUseTls) {
            session->getProperties()["store.imap.options.connection.tls"] = false;
            session->getProperties()["store.imap.options.connection.tls-starttls"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto folder = store->getFolder(
            vmime::utility::path(folderPath.toStdString()));
        folder->open(vmime::net::folder::MODE_READ_ONLY);

        int totalMsgs = static_cast<int>(folder->getMessageCount());
        if (totalMsgs == 0) {
            folder->close(false);
            store->disconnect();
            return emails;
        }

        int start = qMax(1, totalMsgs - count + 1);
        vmime::net::messageSet msgSet = vmime::net::messageSet::byNumber(start, -1);
        auto msgs = folder->getMessages(msgSet);

        for (int i = static_cast<int>(msgs.size()) - 1; i >= 0; --i) {
            const auto &msg = msgs[i];
            Email email;
            // UID 直接转字符串
            email.messageId = QString::number(
                static_cast<qint64>(msg->getNumber()));
            email.folder = folderPath;

            auto parsedMsg = msg->getParsedMessage();
            vmime::messageParser parser(parsedMsg);

            try {
                email.subject = QString::fromStdString(
                    parser.getSubject().getWholeBuffer());
            } catch (...) {
                email.subject = "(无主题)";
            }
            try {
                email.from = QString::fromStdString(
                    parser.getExpeditor().generate());
            } catch (...) {
                email.from = "(未知)";
            }

            try {
                // 提取正文
                if (parser.getTextPartCount() > 0) {
                    email.body = extractContent(
                        parser.getTextPartAt(0)->getText());
                }
            } catch (...) {
                email.body = "";
            }

            int flags = msg->getFlags();
            email.isRead = (flags & 1) != 0;
            emails.append(email);
        }

        folder->close(false);
        store->disconnect();

    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 获取邮件失败: %1")
                           .arg(QString::fromStdString(e.what())));
    }
    return emails;
}

QString ImapClient::doFetchEmailBody(const Account &account, const QString &folderPath,
                                      const QString &messageId)
{
    QString bodyHtml;
    try {
        auto session = vmime::net::session::create();

        session->getProperties()["store.imap.auth.username"] = account.imapUser.toStdString();
        session->getProperties()["store.imap.auth.password"] = account.imapPassword.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;

        if (!account.imapUseTls) {
            session->getProperties()["store.imap.options.connection.tls"] = false;
            session->getProperties()["store.imap.options.connection.tls-starttls"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto folder = store->getFolder(
            vmime::utility::path(folderPath.toStdString()));
        folder->open(vmime::net::folder::MODE_READ_ONLY);

        size_t num = static_cast<size_t>(messageId.toULongLong());
        auto msg = folder->getMessage(num);
        auto parsedMsg = msg->getParsedMessage();

        vmime::messageParser parser(parsedMsg);
        if (parser.getTextPartCount() > 0) {
            bodyHtml = extractContent(
                parser.getTextPartAt(0)->getText());
        }

        folder->close(false);
        store->disconnect();

    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 获取正文失败: %1")
                           .arg(QString::fromStdString(e.what())));
    }
    return bodyHtml;
}
