#include "mailstore.h"
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

MailStore::MailStore(QObject *parent)
    : QObject(parent)
{
}

MailStore::~MailStore()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    // 删除连接名避免下次 addDatabase 冲突
    QSqlDatabase::removeDatabase(m_db.connectionName());
}

bool MailStore::init(const QString &dbPath)
{
    // Qt 知识点：QSqlDatabase 使用
    // 1. 用唯一连接名 addDatabase
    // 2. 设置数据库文件路径
    // 3. open() 打开（SQLite 会自动创建文件）

    QString connName = "mailstore_" + QString::number(reinterpret_cast<quintptr>(this));
    m_db = QSqlDatabase::addDatabase("QSQLITE", connName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool MailStore::createTables()
{
    // Qt 知识点：QSqlQuery 执行 SQL
    // exec() 返回 bool 表示成功或失败

    QSqlQuery query(m_db);

    const QString createEmails = R"(
        CREATE TABLE IF NOT EXISTS emails (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id TEXT,
            sender TEXT NOT NULL,
            recipients TEXT NOT NULL,
            subject TEXT,
            body TEXT,
            date TEXT,
            is_read INTEGER DEFAULT 0,
            folder TEXT NOT NULL DEFAULT 'INBOX',
            attachments TEXT
        )
    )";

    if (!query.exec(createEmails)) {
        qWarning() << "创建 emails 表失败:" << query.lastError().text();
        return false;
    }

    // 索引加速查询
    query.exec("CREATE INDEX IF NOT EXISTS idx_emails_folder ON emails(folder)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_emails_message_id ON emails(message_id)");

    return true;
}

// ---- 邮件 CRUD ----

bool MailStore::saveEmail(const Email &email)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO emails
            (message_id, sender, recipients, subject, body, date, is_read, folder, attachments)
        VALUES (:mid, :sender, :recipients, :subject, :body, :date, :is_read, :folder, :attachments)
    )");

    query.bindValue(":mid", email.messageId);
    query.bindValue(":sender", email.from);
    query.bindValue(":recipients", email.to.join(", "));
    query.bindValue(":subject", email.subject);
    query.bindValue(":body", email.body);
    query.bindValue(":date", email.date.toString(Qt::ISODate));
    query.bindValue(":is_read", email.isRead ? 1 : 0);
    query.bindValue(":folder", email.folder);
    query.bindValue(":attachments", email.attachments.join(", "));

    if (!query.exec()) {
        qWarning() << "saveEmail 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool MailStore::saveEmails(const QList<Email> &emails)
{
    // Qt 知识点：用事务批量插入，大幅提升 SQLite 写入性能
    m_db.transaction();
    for (const Email &email : emails) {
        if (!saveEmail(email)) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

bool MailStore::markAsRead(int64_t id)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE emails SET is_read = 1 WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

bool MailStore::deleteEmail(int64_t id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM emails WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

bool MailStore::deleteEmailsInFolder(const QString &folderPath)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM emails WHERE folder = :folder");
    query.bindValue(":folder", folderPath);
    return query.exec();
}

// ---- 查询 ----

QList<Email> MailStore::getEmails(const QString &folder, int limit, int offset) const
{
    QList<Email> result;
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, message_id, sender, recipients, subject, body, date, is_read, folder, attachments
        FROM emails
        WHERE folder = :folder
        ORDER BY date DESC
        LIMIT :limit OFFSET :offset
    )");
    query.bindValue(":folder", folder);
    query.bindValue(":limit", limit);
    query.bindValue(":offset", offset);

    if (!query.exec()) return result;

    while (query.next()) {
        Email e;
        e.id          = query.value(0).toLongLong();
        e.messageId   = query.value(1).toString();
        e.from        = query.value(2).toString();
        e.to          = query.value(3).toString().split(", ", Qt::SkipEmptyParts);
        e.subject     = query.value(4).toString();
        e.body        = query.value(5).toString();
        e.date        = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
        e.isRead      = query.value(7).toBool();
        e.folder      = query.value(8).toString();
        e.attachments = query.value(9).toString().split(", ", Qt::SkipEmptyParts);
        result.append(e);
    }
    return result;
}

Email MailStore::getEmailById(int64_t id) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id, message_id, sender, recipients, subject, body, date, is_read, folder, attachments FROM emails WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next()) return Email();

    Email e;
    e.id          = query.value(0).toLongLong();
    e.messageId   = query.value(1).toString();
    e.from        = query.value(2).toString();
    e.to          = query.value(3).toString().split(", ", Qt::SkipEmptyParts);
    e.subject     = query.value(4).toString();
    e.body        = query.value(5).toString();
    e.date        = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
    e.isRead      = query.value(7).toBool();
    e.folder      = query.value(8).toString();
    e.attachments = query.value(9).toString().split(", ", Qt::SkipEmptyParts);
    return e;
}

int MailStore::getUnreadCount(const QString &folder) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM emails WHERE folder = :folder AND is_read = 0");
    query.bindValue(":folder", folder);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}
