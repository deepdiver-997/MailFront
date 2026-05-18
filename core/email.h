#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QDateTime>
#include <QStringList>

struct Email {
    int64_t id = 0;
    QString messageId;      // RFC 消息ID
    QString from;           // 发件人
    QStringList to;         // 收件人
    QString subject;        // 主题
    QString body;           // 正文
    QDateTime date;         // 发送时间
    bool isRead = false;    // 已读/未读
    QString folder;         // 所属文件夹(inbox/sent/drafts/trash)
    QStringList attachments;// 附件文件名列表
};

#endif // EMAIL_H
