#ifndef FOLDERTREE_H
#define FOLDERTREE_H

#include <QTreeView>
#include <QStandardItemModel>

class FolderTree : public QTreeView
{
    Q_OBJECT
public:
    explicit FolderTree(QWidget *parent = nullptr);

    void clear();
    void addFolder(const QString &name, const QString &path, int unreadCount);
    void updateUnreadCount(const QString &path, int unreadCount);

signals:
    void folderSelected(const QString &path);

private slots:
    void onItemClicked(const QModelIndex &index);

private:
    QStandardItemModel *m_model;
};

#endif // FOLDERTREE_H
