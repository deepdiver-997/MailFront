#include "maillistmodel.h"
#include <QFont>
#include <QColor>

MailListModel::MailListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MailListModel::rowCount(const QModelIndex &parent) const
{
    // 列表模型只需返回行数，parent 无效
    return parent.isValid() ? 0 : m_emails.size();
}

QVariant MailListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_emails.size())
        return QVariant();

    const Email &email = m_emails.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        // 默认显示角色 —— 返回可读的摘要文本
        return QString("%1 - %2").arg(email.subject, email.from);

    case FromRole:
        return email.from;
    case SubjectRole:
        return email.subject;
    case DateRole:
        return email.date;
    case IsReadRole:
        return email.isRead;
    case FolderRole:
        return email.folder;
    case EmailIdRole:
        return static_cast<qint64>(email.id);

    // Qt::FontRole —— 未读邮件加粗显示
    case Qt::FontRole:
        if (!email.isRead) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return QVariant();

    // Qt::ForegroundRole —— 已读邮件灰色
    case Qt::ForegroundRole:
        if (email.isRead) {
            return QColor(Qt::gray);
        }
        return QVariant();

    default:
        return QVariant();
    }
}

void MailListModel::setEmails(const QList<Email> &emails)
{
    beginResetModel();
    m_emails = emails;
    endResetModel();
}

void MailListModel::clear()
{
    beginResetModel();
    m_emails.clear();
    endResetModel();
}

Email MailListModel::emailAt(int row) const
{
    if (row >= 0 && row < m_emails.size())
        return m_emails.at(row);
    return Email();
}

void MailListModel::updateEmailBody(int row, const QString &body)
{
    if (row < 0 || row >= m_emails.size())
        return;
    m_emails[row].body = body;
    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}
