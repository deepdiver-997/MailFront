#ifndef FOLDER_H
#define FOLDER_H

#include <QString>
#include <QMap>

struct Folder {
    int64_t id = 0;
    QString name;
    QString path;       // IMAP 路径, e.g. "INBOX/subfolder"
    int unreadCount = 0;
    int totalCount = 0;
    bool isSystem = false;
};

// 将 IMAP 文件夹路径/原始名映射为中文显示名
inline QString folderDisplayName(const QString &path, const QString &rawName)
{
    // 优先按路径匹配（case-insensitive）
    const QString lowerPath = path.toLower();

    if (lowerPath == "inbox")
        return QStringLiteral("\u6536\u4ef6\u7bb1");      // 收件箱

    if (lowerPath.contains("sent"))
        return QStringLiteral("\u5df2\u53d1\u9001");      // 已发送

    if (lowerPath.contains("draft"))
        return QStringLiteral("\u8349\u7a3f\u7bb1");      // 草稿箱

    if (lowerPath.contains("trash") || lowerPath.contains("deleted"))
        return QStringLiteral("\u5783\u573e\u7bb1");      // 垃圾箱

    if (lowerPath.contains("junk") || lowerPath.contains("spam"))
        return QStringLiteral("\u5783\u573e\u90ae\u4ef6"); // 垃圾邮件

    if (lowerPath.contains("archive"))
        return QStringLiteral("\u5b58\u6863");             // 存档

    return rawName;
}

#endif // FOLDER_H
