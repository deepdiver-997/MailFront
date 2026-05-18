#ifndef FOLDER_H
#define FOLDER_H

#include <QString>

struct Folder {
    int64_t id = 0;
    QString name;
    QString path;       // IMAP 路径, e.g. "INBOX/subfolder"
    int unreadCount = 0;
    int totalCount = 0;

    bool isSystem;  };  // 系统文件夹(inbox/sent/trash)还是自定义

#endif // FOLDER_H
