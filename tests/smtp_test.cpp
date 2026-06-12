#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include <vmime/vmime.hpp>
#include <vmime/net/transport.hpp>
#include <vmime/message.hpp>
#include <vmime/messageBuilder.hpp>
#include <vmime/security/cert/certificateVerifier.hpp>

// ============================================================
// SMTP 服务器连通性测试
// ============================================================

class DummyCertVerifier : public vmime::security::cert::certificateVerifier {
public:
    void verify(const vmime::shared_ptr<vmime::security::cert::certificateChain>&,
                const vmime::string&) override {}
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // ---- 配置 ----
    const QString host     = "smtp.scut.email";
    const int     port     = 25;
    const bool    useTls   = false;  // 465=SSL
    const QString username = "test@scut.email";
    const QString password = "qtest123";

    qDebug() << "\n================================================";
    qDebug() << "          SMTP 连接测试";
    qDebug() << "================================================\n";
    qDebug() << "  服务器:" << host << ":" << port << (useTls ? "(SSL)" : "(明文)");
    qDebug() << "  用户:  " << username;
    qDebug() << "  密码:  " << QString(password.size(), '*');
    qDebug() << "";

    try {
        // ---- 1. 创建 Session ----
        qDebug() << "▶ [1/4] 创建 VMime session...";
        auto session = vmime::net::session::create();
        qDebug() << "     OK\n";

        // ---- 2. 设置认证 ----
        qDebug() << "▶ [2/4] 设置 SMTP 认证信息...";
        const char *prefix = useTls ? "transport.smtps." : "transport.smtp.";
        session->getProperties()[std::string(prefix) + "options.need-authentication"] = true;
        session->getProperties()[std::string(prefix) + "auth.username"] = username.toStdString();
        session->getProperties()[std::string(prefix) + "auth.password"] = password.toStdString();
        if (!useTls) {
            // 端口 25 / 587：让 VMime 根据 EHLO 自动 STARTTLS
            session->getProperties()[std::string(prefix) + "connection.tls"] = true;
            session->getProperties()[std::string(prefix) + "connection.tls.required"] = false;
        }
        qDebug() << "     OK\n";

        // ---- 3. 连接 + 发送测试邮件 ----
        qDebug() << "▶ [3/4] 连接 SMTP...";
        QString urlStr = useTls
            ? QString("smtps://%1:%2").arg(host).arg(port)
            : QString("smtp://%1:%2").arg(host).arg(port);

        auto tr = session->getTransport(vmime::utility::url(urlStr.toStdString()));
        tr->setCertificateVerifier(
            vmime::make_shared<DummyCertVerifier>());

        tr->connect();
        qDebug() << "     ✓ 连接成功\n";

        // 构建测试邮件
        qDebug() << "▶ [4/4] 发送测试邮件...";
        vmime::messageBuilder mb;
        mb.setExpeditor(vmime::mailbox(username.toStdString()));

        vmime::addressList toList;
        toList.appendAddress(
            vmime::make_shared<vmime::mailbox>(username.toStdString()));
        mb.setRecipients(toList);
        mb.setSubject(vmime::text("SMTP Test from mail_front", vmime::charsets::UTF_8));

        mb.constructTextPart(vmime::mediaType("text", "plain"));
        mb.getTextPart()->setText(
            vmime::make_shared<vmime::stringContentHandler>(
                "This is a test email sent from the mail_front SMTP test tool.\r\n"
                "If you receive this, SMTP is working correctly.\r\n"));

        auto msg = mb.construct();
        tr->send(msg);
        qDebug() << "     ✓ 邮件发送成功\n";

        tr->disconnect();
        qDebug() << "     已断开连接\n";

    } catch (vmime::exception &e) {
        qCritical() << "\n❌ VMime 异常:" << e.what();
        qCritical() << "   类型:" << typeid(e).name();
        return 1;
    } catch (std::exception &e) {
        qCritical() << "\n❌ 标准异常:" << e.what();
        return 1;
    }

    qDebug() << "================================================";
    qDebug() << "  测试完成 ✓";
    qDebug() << "================================================\n";
    return 0;
}
