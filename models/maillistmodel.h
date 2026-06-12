#ifndef MAILLISTMODEL_H
#define MAILLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "core/email.h"

// ============================================================
// Qt 知识点：Model/View 架构
// QAbstractListModel 是 Qt 列表数据模型的基类
// 必须实现: rowCount() - 数据行数
//          data()     - 每个单元格的数据
// 可选实现: roleNames() - 定义数据角色名（供 QML/调试用）
// ============================================================
class MailListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // 自定义数据角色 —— Qt::UserRole 之后的值是用户自定义的
    enum MailRoles {
        FromRole    = Qt::UserRole + 1,
        SubjectRole,
        DateRole,
        IsReadRole,
        FolderRole,
        EmailIdRole
    };

    explicit MailListModel(QObject *parent = nullptr);

    // QAbstractListModel 必须实现的接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // 业务方法
    void setEmails(const QList<Email> &emails);
    void clear();
    Email emailAt(int row) const;
    void updateEmailBody(int row, const QString &body);

private:
    QList<Email> m_emails;
};

#endif // MAILLISTMODEL_H
