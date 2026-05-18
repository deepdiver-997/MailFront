#include "VSMimeNetWorkAdapter.h"
#include <QThreadPool>
#include <qerrormessage.h>
#include <QtConcurrent/QtConcurrent>
#include <vmime/message.hpp>
#include <vmime/net/transport.hpp>

VSMimeNetworkAdapter::VSMimeNetworkAdapter(QObject* parent) : QObject(parent) {
    // 初始化 VMime 会话
    m_session = vmime::net::session::create();
}

void VSMimeNetworkAdapter::sendEmailAsync(const QString& recipient, const QString& subject, const QString& body) {
    // 在线程池中执行阻塞操作
    QtConcurrent::run([=]() {
        try {
            auto tr = m_session->getTransport(vmime::utility::url("smtps://smtp.example.com:465"));

            vmime::messageBuilder mb;
            mb.setSubject(vmime::text("Hello, World!"));
            mb.setExpeditor(vmime::mailbox("sender@example.com"));
            vmime::addressList to;
            to.appendAddress(vmime::make_shared <vmime::mailbox>("2466245103@11.com"));
            mb.setRecipients(to);
            mb.setSubject(vmime::text("My first message generated with vmime::messageBuilder"));
            mb.getTextPart()->setText(vmime::make_shared <vmime::stringContentHandler>("This is a test message."));

            vmime::shared_ptr<vmime::message> msg = mb.construct();


            tr->connect();
            tr->send(msg);
            tr->disconnect();
            // emit emailSent(true);  // 成功信号
        } catch (vmime::exception& e) {
            // emit errorOccurred(QString::fromStdString(e.what()));
        }
    });
}
