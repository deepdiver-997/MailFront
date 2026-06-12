#include "smtpclient.h"
#include <QtConcurrent/QtConcurrent>
#include <vmime/message.hpp>
#include <vmime/messageBuilder.hpp>
#include <vmime/net/transport.hpp>
#include <vmime/security/cert/certificateVerifier.hpp>

// ============================================================
// Qt 知识点：QtConcurrent + QFutureWatcher 异步模式
//
// 模式模板：
//   void SomeClass::someAsyncMethod(params) {
//       QFuture<T> future = QtConcurrent::run(&workerFunction, params);
//       QFutureWatcher<T> *watcher = new QFutureWatcher<T>(this);
//       watcher->setFuture(future);
//       connect(watcher, &QFutureWatcher<T>::finished,
//               this, &SomeClass::onTaskFinished);
//   }
//
// 关键规则：
//   - QtConcurrent::run() 返回的是 QFuture，不等待
//   - UI 线程立即返回，保持响应
//   - 线程池中的工作线程执行阻塞 I/O
//   - 任务完成后 watcher emit finished() 信号
// ============================================================

SmtpClient::SmtpClient(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<bool>(this))
    , m_busy(false)
{
    connect(m_watcher, &QFutureWatcher<bool>::finished,
            this, &SmtpClient::onTaskFinished);
}

void SmtpClient::sendEmail(const Account &account, const Email &email)
{
    if (m_busy) {
        emit errorOccurred("正在发送中，请稍后");
        return;
    }
    m_busy = true;

    // 在线程池中执行阻塞发送——UI 线程不等待
    QFuture<bool> future = QtConcurrent::run(
        &SmtpClient::doSendEmail, this, account, email);
    m_watcher->setFuture(future);
}

bool SmtpClient::isBusy() const
{
    return m_busy;
}

// 测试用：接受所有证书的验证器
class DummyCertVerifier : public vmime::security::cert::certificateVerifier {
public:
    void verify(const vmime::shared_ptr<vmime::security::cert::certificateChain>&,
                const vmime::string&) override {
        // 无条件信任
    }
};

bool SmtpClient::doSendEmail(Account account, Email email)
{
    try {
        auto session = vmime::net::session::create();

        // 构建 SMTP 连接 URL
        QString urlStr = account.smtpUseTls
            ? QString("smtps://%1:%2").arg(account.smtpHost).arg(account.smtpPort)
            : QString("smtp://%1:%2").arg(account.smtpHost).arg(account.smtpPort);

        auto tr = session->getTransport(vmime::utility::url(urlStr.toStdString()));

        // 启用 AUTH 认证（VMime 默认关闭！）
        const char *prefix = account.smtpUseTls ? "transport.smtps." : "transport.smtp.";
        session->getProperties()[std::string(prefix) + "options.need-authentication"] = true;
        session->getProperties()[std::string(prefix) + "auth.username"] =
            account.smtpUser.toStdString();
        session->getProperties()[std::string(prefix) + "auth.password"] =
            account.smtpPassword.toStdString();

        // 跳过 TLS 证书验证（SMTPS 和 STARTTLS 都需要）
        tr->setCertificateVerifier(
            vmime::make_shared<DummyCertVerifier>());

        // 构建 MIME 消息
        vmime::messageBuilder mb;
        mb.setExpeditor(vmime::mailbox(account.email.toStdString()));

        vmime::addressList toList;
        for (const QString &recipient : email.to) {
            toList.appendAddress(
                vmime::make_shared<vmime::mailbox>(recipient.toStdString()));
        }
        mb.setRecipients(toList);
        mb.setSubject(vmime::text(email.subject.toStdString()));

        // 正文（支持 HTML）
        if (email.body.startsWith("<")) {
            mb.constructTextPart(vmime::mediaType("text", "html"));
        }
        mb.getTextPart()->setText(
            vmime::make_shared<vmime::stringContentHandler>(email.body.toStdString()));

        vmime::shared_ptr<vmime::message> msg = mb.construct();

        // 阻塞网络 I/O（在线程池中执行，不影响 UI）
        tr->connect();
        tr->send(msg);              // VMime 内部等待 DATA 后的 250 确认
        tr->disconnect();

        // 执行到此处 → 完整 SMTP 流程成功（connect → send → disconnect 无异常）
        return true;

    } catch (vmime::exception &e) {
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;   // 中途出错 → 不标记为已发送
    }
}

void SmtpClient::onTaskFinished()
{
    m_busy = false;

    const bool success = m_watcher->future().result();

    // doSendEmail 返回 true → 完整 SMTP 流程成功（服务器已确认 DATA）
    if (success) {
        emit emailSent();
    }
    // 返回 false → doSendEmail 已在 catch 块中 emit errorOccurred，此处不再重复发信号
}
