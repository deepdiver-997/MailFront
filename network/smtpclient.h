#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <QObject>
#include <QFutureWatcher>
#include <vmime/vmime.hpp>
#include "core/email.h"
#include "core/account.h"

// ============================================================
// Qt 知识点：异步网络 + 信号槽跨线程
//
// 设计思路：
//   1. sendEmail() 被调用时，用 QtConcurrent::run() 把 VMime
//      的阻塞网络 I/O 丢到线程池执行
//   2. 工作线程完成后，emit 信号通知 UI 线程结果
//   3. Qt 信号槽系统会自动处理跨线程的消息传递
//
// 关键：QFutureWatcher 监控异步任务状态
//   - QFutureWatcher::finished 信号在任务完成时触发
//   - 可以在任何线程安全地连接这个信号
// ============================================================
class SmtpClient : public QObject
{
    Q_OBJECT

public:
    explicit SmtpClient(QObject *parent = nullptr);

    // 异步发送邮件（不阻塞 UI）
    void sendEmail(const Account &account, const Email &email);

    // 是否有正在进行的发送任务
    bool isBusy() const;

signals:
    // 发送成功
    void emailSent();
    // 发送失败（跨线程安全）
    void errorOccurred(const QString &errorMessage);

private slots:
    void onTaskFinished();

private:
    // 实际阻塞发送逻辑（在线程池中执行）
    // 返回 true 表示 SMTP 完整流程成功（connect → send → disconnect 无异常）
    // 返回 false 表示中途出错
    bool doSendEmail(Account account, Email email);

    QFutureWatcher<bool> *m_watcher;
    bool m_busy;
};

#endif // SMTPCLIENT_H
