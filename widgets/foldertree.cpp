#include "foldertree.h"
#include <QStandardItem>
#include <QFont>

FolderTree::FolderTree(QWidget *parent)
    : QTreeView(parent)
    , m_model(new QStandardItemModel(this))
{
    m_model->setHorizontalHeaderLabels({"文件夹"});
    setModel(m_model);
    setHeaderHidden(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setExpandsOnDoubleClick(true);

    // 连接点击信号
    connect(this, &QTreeView::clicked, this, &FolderTree::onItemClicked);
}

void FolderTree::clear()
{
    m_model->removeRows(0, m_model->rowCount());
}

void FolderTree::addFolder(const QString &name, const QString &path, int unreadCount)
{
    // 检查是否已存在同路径文件夹（大小写不敏感），存在则更新未读数
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem *existing = m_model->item(i);
        if (existing && existing->data(Qt::UserRole).toString().compare(path, Qt::CaseInsensitive) == 0) {
            updateUnreadCount(path, unreadCount);
            return;
        }
    }

    auto *item = new QStandardItem(name);
    item->setData(path, Qt::UserRole);          // 存 IMAP 路径
    item->setData(name, Qt::DisplayRole);
    item->setEditable(false);

    // 有未读邮件时加粗
    if (unreadCount > 0) {
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        item->setText(QString("%1 (%2)").arg(name).arg(unreadCount));
    }

    m_model->appendRow(item);
}

void FolderTree::updateUnreadCount(const QString &path, int unreadCount)
{
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem *item = m_model->item(i);
        if (item->data(Qt::UserRole).toString().compare(path, Qt::CaseInsensitive) == 0) {
            QString name = item->data(Qt::DisplayRole).toString();
            if (unreadCount > 0) {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                item->setText(QString("%1 (%2)").arg(name.section(" (", 0, 0)).arg(unreadCount));
            } else {
                QFont font = item->font();
                font.setBold(false);
                item->setFont(font);
                item->setText(name.section(" (", 0, 0));
            }
            break;
        }
    }
}

void FolderTree::onItemClicked(const QModelIndex &index)
{
    QString path = index.data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit folderSelected(path);
    }
}
