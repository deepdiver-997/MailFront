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
    , m_deleteWatcher(new QFutureWatcher<bool>(this))
{
    connect(m_folderWatcher, &QFutureWatcher<QList<Folder>>::finished,
            this, &ImapClient::onFoldersTaskFinished);
    connect(m_emailWatcher, &QFutureWatcher<QList<Email>>::finished,
            this, &ImapClient::onEmailsTaskFinished);
    connect(m_bodyWatcher, &QFutureWatcher<QString>::finished,
            this, &ImapClient::onEmailBodyTaskFinished);
    connect(m_deleteWatcher, &QFutureWatcher<bool>::finished,
            this, &ImapClient::onDeleteTaskFinished);
}

bool ImapClient::isBusy() const
{
    return m_folderWatcher->isRunning()
        || m_emailWatcher->isRunning()
        || m_bodyWatcher->isRunning()
        || m_deleteWatcher->isRunning();
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

void ImapClient::fetchNewEmails(const Account &account, const QString &folderPath,
                                 const QSet<QString> &knownUIDs)
{
    QFuture<QList<Email>> future = QtConcurrent::run(
        &ImapClient::doFetchNewEmails, this, account, folderPath, knownUIDs);
    m_emailWatcher->setFuture(future);
}

void ImapClient::fetchEmailBody(const Account &account, const QString &folderPath,
                                 const QString &messageId)
{
    m_pendingBodyMessageId = messageId;
    QFuture<QString> future = QtConcurrent::run(
        &ImapClient::doFetchEmailBody, this, account, folderPath, messageId);
    m_bodyWatcher->setFuture(future);
}

void ImapClient::deleteEmail(const Account &account, const QString &folderPath,
                              const QString &uid)
{
    m_pendingDeleteUid = uid;
    QFuture<bool> future = QtConcurrent::run(
        &ImapClient::doDeleteEmail, this, account, folderPath, uid);
    m_deleteWatcher->setFuture(future);
}

// ---- 槽：任务完成 ----

void ImapClient::onFoldersTaskFinished()
{
    try {
        QList<Folder> folders = m_folderWatcher->result();
        if (!folders.isEmpty()) {
            emit foldersFetched(folders);
        }
    } catch (std::exception &e) {
        emit errorOccurred(QString("获取文件夹结果异常: %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("获取文件夹结果异常: 未知错误"));
    }
}

void ImapClient::onEmailsTaskFinished()
{
    try {
        QList<Email> emails = m_emailWatcher->result();
        qDebug() << "[onEmailsTaskFinished] got" << emails.size() << "emails";
        if (!emails.isEmpty()) {
            QString folderPath = emails.first().folder;
            emit emailsFetched(folderPath, emails);
        }
    } catch (std::exception &e) {
        qDebug() << "[onEmailsTaskFinished] exception:" << e.what();
        emit errorOccurred(QString("获取邮件结果异常: %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        qDebug() << "[onEmailsTaskFinished] unknown exception";
        emit errorOccurred(QString("获取邮件结果异常: 未知错误"));
    }
}

void ImapClient::onEmailBodyTaskFinished()
{
    try {
        QString body = m_bodyWatcher->result();
        if (!body.isEmpty()) {
            emit emailBodyFetched(m_pendingBodyMessageId, body);
        }
    } catch (std::exception &e) {
        emit errorOccurred(QString("获取正文结果异常: %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("获取正文结果异常: 未知错误"));
    }
}

void ImapClient::onDeleteTaskFinished()
{
    try {
        bool ok = m_deleteWatcher->result();
        if (ok) {
            emit emailDeleted(m_pendingDeleteUid);
        }
    } catch (std::exception &e) {
        emit errorOccurred(QString("删除邮件异常: %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("删除邮件异常: 未知错误"));
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

        // 禁用 TLS 和 STARTTLS（明文连接）
        session->getProperties()["store.imap.connection.tls"] = false;
        session->getProperties()["store.imap.connection.tls.required"] = false;

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
            QString rawName = QString::fromStdString(f->getName().getBuffer());
            folder.path = QString::fromStdString(
                f->getFullPath().toString("/", vmime::charset::getLocalCharset()));
            folder.name = folderDisplayName(folder.path, rawName);
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
    } catch (std::exception &e) {
        emit errorOccurred(QString("IMAP 获取文件夹失败(系统): %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("IMAP 获取文件夹失败: 未知异常"));
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
            // 不强制 TLS，但允许 VMime 根据服务器 CAPABILITY 自动协商 STARTTLS
            session->getProperties()["store.imap.connection.tls.required"] = false;
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
        qDebug() << "[doFetchEmails] folder=" << folderPath << "totalMsgs=" << totalMsgs;
        if (totalMsgs == 0) {
            folder->close(false);
            store->disconnect();
            return emails;
        }

        int start = qMax(1, totalMsgs - count + 1);
        vmime::net::messageSet msgSet = vmime::net::messageSet::byNumber(
            static_cast<size_t>(start), static_cast<size_t>(totalMsgs));

        vmime::net::fetchAttributes attrs;
        attrs.add(vmime::net::fetchAttributes::FLAGS);
        attrs.add(vmime::net::fetchAttributes::ENVELOPE);
        attrs.add(vmime::net::fetchAttributes::SIZE);
        auto msgs = folder->getAndFetchMessages(msgSet, attrs);
        qDebug() << "[doFetchEmails] fetched" << msgs.size() << "msgs";

        for (int i = static_cast<int>(msgs.size()) - 1; i >= 0; --i) {
            const auto &msg = msgs[i];
            Email email;
            email.messageId = QString::fromStdString(
                static_cast<std::string>(msg->getUID()));
            email.folder = folderPath;

            try {
                auto hdr = msg->getHeader();
                if (hdr) {
                    try {
                        email.subject = QString::fromStdString(
                            hdr->Subject()->getValue<vmime::text>()->getWholeBuffer());
                    } catch (...) {
                        email.subject = "(无主题)";
                    }
                    try {
                        email.from = QString::fromStdString(
                            hdr->From()->getValue<vmime::mailbox>()->generate());
                    } catch (...) {
                        try {
                            email.from = QString::fromStdString(
                                hdr->From()->getValue<vmime::addressList>()->generate());
                        } catch (...) {
                            email.from = "(未知)";
                        }
                    }
                    // 提取日期
                    try {
                        auto dateField = hdr->Date();
                        if (dateField) {
                            QString dateStr = QString::fromStdString(
                                dateField->getValue<vmime::datetime>()->generate());
                            email.date = QDateTime::fromString(dateStr, Qt::RFC2822Date);
                        }
                    } catch (...) {
                        // date stays invalid
                    }
                } else {
                    email.subject = "(无主题)";
                    email.from = "(未知)";
                }
            } catch (...) {
                email.subject = "(无主题)";
                email.from = "(未知)";
            }

            // IMAP ENVELOPE 不含收件人，用默认值满足 DB NOT NULL
            email.to = QStringList{"(IMAP)"};

            try {
                email.isRead = (msg->getFlags() & vmime::net::message::FLAG_SEEN) != 0;
            } catch (...) {
                email.isRead = false;
            }

            emails.append(email);
        }

        folder->close(false);
        store->disconnect();

    } catch (vmime::exception &e) {
        qDebug() << "[doFetchEmails] VMime exception:" << e.what();
        emit errorOccurred(QString("IMAP 获取邮件失败: %1")
                           .arg(QString::fromStdString(e.what())));
    } catch (std::exception &e) {
        qDebug() << "[doFetchEmails] std exception:" << e.what();
        emit errorOccurred(QString("IMAP 获取邮件失败(系统): %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        qDebug() << "[doFetchEmails] unknown exception";
        emit errorOccurred(QString("IMAP 获取邮件失败: 未知异常"));
    }
    qDebug() << "[doFetchEmails] returning" << emails.size() << "emails";
    return emails;
}

QList<Email> ImapClient::doFetchNewEmails(const Account &account, const QString &folderPath,
                                            QSet<QString> knownUIDs)
{
    QList<Email> emails;
    try {
        auto session = vmime::net::session::create();
        session->getProperties()["store.imap.auth.username"] = account.imapUser.toStdString();
        session->getProperties()["store.imap.auth.password"] = account.imapPassword.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;
        if (!account.imapUseTls) {
            // 不强制 TLS，但允许 VMime 根据服务器 CAPABILITY 自动协商 STARTTLS
            session->getProperties()["store.imap.connection.tls.required"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto folder = store->getFolder(vmime::utility::path(folderPath.toStdString()));
        folder->open(vmime::net::folder::MODE_READ_ONLY);

        int totalMsgs = static_cast<int>(folder->getMessageCount());
        qDebug() << "[doFetchEmails] folder=" << folderPath << "totalMsgs=" << totalMsgs;
        if (totalMsgs == 0) {
            folder->close(false);
            store->disconnect();
            return emails;
        }

        // Step 1: 轻量 UID 扫描，找出新邮件（字符串形式存UID，和 doFetchEmailBody 一致）
        struct NewInfo { size_t seq; QString uid; };
        std::vector<NewInfo> newInfos;
        {
            vmime::net::fetchAttributes uidAttrs;
            uidAttrs.add(vmime::net::fetchAttributes::UID);
            auto allMsgs = folder->getAndFetchMessages(
                vmime::net::messageSet::byNumber(1, static_cast<size_t>(totalMsgs)),
                uidAttrs);
            for (const auto &m : allMsgs) {
                QString uid = QString::fromStdString(static_cast<std::string>(m->getUID()));
                if (!knownUIDs.contains(uid)) {
                    newInfos.push_back({m->getNumber(), uid});
                }
            }
        }

        if (newInfos.empty()) {
            folder->close(false);
            store->disconnect();
            return emails;
        }

        // Step 2: 逐 UID FETCH 内容
        const int toFetch = qMin(static_cast<int>(newInfos.size()), 50);
        int skip = newInfos.size() - toFetch;
        vmime::net::fetchAttributes attrs;
        attrs.add(vmime::net::fetchAttributes::FLAGS);
        attrs.add(vmime::net::fetchAttributes::ENVELOPE);
        attrs.add(vmime::net::fetchAttributes::SIZE);

        for (int k = skip; k < static_cast<int>(newInfos.size()); ++k) {
            vmime::net::message::uid uid(newInfos[k].uid.toStdString());
            auto msgs = folder->getAndFetchMessages(
                vmime::net::messageSet::byUID(uid), attrs);
            for (const auto &msg : msgs) {

            Email email;
            email.messageId = QString::fromStdString(static_cast<std::string>(msg->getUID()));
            email.folder = folderPath;
            email.to = QStringList{"(IMAP)"};

            try {
                auto hdr = msg->getHeader();
                if (hdr) {
                    try {
                        email.subject = QString::fromStdString(
                            hdr->Subject()->getValue<vmime::text>()->getWholeBuffer());
                    } catch (...) { email.subject = "(无主题)"; }
                    try {
                        email.from = QString::fromStdString(
                            hdr->From()->getValue<vmime::mailbox>()->generate());
                    } catch (...) {
                        try {
                            email.from = QString::fromStdString(
                                hdr->From()->getValue<vmime::addressList>()->generate());
                        } catch (...) { email.from = "(未知)"; }
                    }
                    try {
                        auto dateField = hdr->Date();
                        if (dateField) {
                            email.date = QDateTime::fromString(
                                QString::fromStdString(dateField->getValue<vmime::datetime>()->generate()),
                                Qt::RFC2822Date);
                        }
                    } catch (...) {}
                } else {
                    email.subject = "(无主题)";
                    email.from = "(未知)";
                }
            } catch (...) {
                email.subject = "(无主题)";
                email.from = "(未知)";
            }

            try {
                email.isRead = (msg->getFlags() & vmime::net::message::FLAG_SEEN) != 0;
            } catch (...) { email.isRead = false; }

            emails.append(email);
            }  // inner for (messages)
        }  // outer for (UIDs)

        folder->close(false);
        store->disconnect();

    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 增量获取失败: %1").arg(QString::fromStdString(e.what())));
    } catch (std::exception &e) {
        emit errorOccurred(QString("IMAP 增量获取失败(系统): %1").arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("IMAP 增量获取失败: 未知异常"));
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
            // 不强制 TLS，但允许 VMime 根据服务器 CAPABILITY 自动协商 STARTTLS
            session->getProperties()["store.imap.connection.tls.required"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto folder = store->getFolder(
            vmime::utility::path(folderPath.toStdString()));
        folder->open(vmime::net::folder::MODE_READ_ONLY);

        // 先扫一遍序号找匹配 UID，再精准 FETCH
        int totalMsgs = static_cast<int>(folder->getMessageCount());
        if (totalMsgs == 0) {
            folder->close(false);
            store->disconnect();
            return bodyHtml;
        }
        size_t targetSeq = 0;
        {
            vmime::net::fetchAttributes lightAttrs;
            lightAttrs.add(vmime::net::fetchAttributes::UID);
            auto allMsgs = folder->getAndFetchMessages(
                vmime::net::messageSet::byNumber(1, static_cast<size_t>(totalMsgs)),
                lightAttrs);
            for (const auto &m : allMsgs) {
                if (QString::fromStdString(
                        static_cast<std::string>(m->getUID())) == messageId) {
                    targetSeq = m->getNumber();
                    break;
                }
            }
        }
        if (targetSeq == 0) {
            folder->close(false);
            store->disconnect();
            emit errorOccurred(QString("未找到邮件 UID: %1").arg(messageId));
            return bodyHtml;
        }

        // 提取完整 RFC822 再用 vmime::messageParser 解析正文
        auto targetMsg = folder->getMessage(targetSeq);
        std::string rawMsg;
        vmime::utility::outputStreamStringAdapter rawAdapter(rawMsg);
        targetMsg->extract(rawAdapter);

        auto parsedMsg = vmime::make_shared<vmime::message>();
        parsedMsg->parse(rawMsg);
        vmime::messageParser parser(parsedMsg);
        // 优先取 HTML 正文，fallback 到纯文本
        for (int i = 0; i < parser.getTextPartCount(); ++i) {
            auto part = parser.getTextPartAt(i);
            if (part->getType().getType() == vmime::mediaTypes::TEXT_HTML) {
                bodyHtml = extractContent(part->getText());
                break;
            }
        }
        if (bodyHtml.isEmpty() && parser.getTextPartCount() > 0) {
            bodyHtml = extractContent(parser.getTextPartAt(0)->getText());
        }

        folder->close(false);
        store->disconnect();

    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 获取正文失败: %1")
                           .arg(QString::fromStdString(e.what())));
    } catch (std::exception &e) {
        emit errorOccurred(QString("IMAP 获取正文失败(系统): %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("IMAP 获取正文失败: 未知异常"));
    }
    return bodyHtml;
}

bool ImapClient::doDeleteEmail(const Account &account, const QString &folderPath,
                                const QString &uid)
{
    try {
        auto session = vmime::net::session::create();

        session->getProperties()["store.imap.auth.username"] = account.imapUser.toStdString();
        session->getProperties()["store.imap.auth.password"] = account.imapPassword.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;

        if (!account.imapUseTls) {
            // 不强制 TLS，但允许 VMime 根据服务器 CAPABILITY 自动协商 STARTTLS
            session->getProperties()["store.imap.connection.tls.required"] = false;
        }

        QString urlStr = account.imapUseTls
            ? QString("imaps://%1:%2").arg(account.imapHost).arg(account.imapPort)
            : QString("imap://%1:%2").arg(account.imapHost).arg(account.imapPort);

        auto store = session->getStore(vmime::utility::url(urlStr.toStdString()));
        store->connect();

        auto folder = store->getFolder(
            vmime::utility::path(folderPath.toStdString()));
        folder->open(vmime::net::folder::MODE_READ_WRITE);

        // 按 UID 查找邮件并设置 \Deleted 标志
        int totalMsgs = static_cast<int>(folder->getMessageCount());
        if (totalMsgs > 0) {
            auto msgs = folder->getMessages(
                vmime::net::messageSet::byNumber(1, static_cast<size_t>(totalMsgs)));
            for (size_t i = 0; i < msgs.size(); ++i) {
                if (QString::fromStdString(
                        static_cast<std::string>(msgs[i]->getUID())) == uid) {
                    msgs[i]->setFlags(vmime::net::message::FLAG_DELETED,
                                      vmime::net::message::FLAG_MODE_ADD);
                    break;
                }
            }
        }

        folder->expunge();
        folder->close(false);
        store->disconnect();
        return true;

    } catch (vmime::exception &e) {
        emit errorOccurred(QString("IMAP 删除邮件失败: %1")
                           .arg(QString::fromStdString(e.what())));
    } catch (std::exception &e) {
        emit errorOccurred(QString("IMAP 删除邮件失败(系统): %1")
                           .arg(QString::fromLocal8Bit(e.what())));
    } catch (...) {
        emit errorOccurred(QString("IMAP 删除邮件失败: 未知异常"));
    }
    return false;
}
