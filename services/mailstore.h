#ifndef MAILSTORE_H
#define MAILSTORE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QSet>
#include "core/email.h"

// ============================================================
// Qt 知识点：QSqlDatabase + SQLite
//
// Qt 的 SQL 模块封装了各种数据库驱动：
//   - QSqlDatabase::addDatabase("QSQLITE") 创建 SQLite 连接
//   - QSqlQuery 执行 SQL 语句
//   - 支持 prepared statement（参数化查询）
//   - 同一个连接可跨线程（需注意事务安全）
// ============================================================
class MailStore : public QObject
{
    Q_OBJECT

public:
    explicit MailStore(QObject *parent = nullptr);
    ~MailStore();

    // 初始化数据库（创建表）
    bool init(const QString &dbPath);

    // 邮件 CRUD
    bool saveEmail(const Email &email, const QString &accountEmail = QString());
    bool saveEmails(const QList<Email> &emails, const QString &accountEmail = QString());
    bool markAsRead(int64_t id);
    bool deleteEmail(int64_t id);
    bool deleteEmailsInFolder(const QString &folderPath, const QString &accountEmail = QString());

    // 查询
    QList<Email> getEmails(const QString &folder, const QString &accountEmail = QString(),
                           int limit = 50, int offset = 0) const;
    Email getEmailById(int64_t id) const;
    int getUnreadCount(const QString &folder, const QString &accountEmail = QString()) const;
    QStringList getDistinctFolders(const QString &accountEmail) const;
    QSet<QString> getMessageIds(const QString &folder, const QString &accountEmail) const;

private:
    bool createTables();

    QSqlDatabase m_db;
};

#endif // MAILSTORE_H
