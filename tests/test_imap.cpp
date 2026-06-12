#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include <vmime/vmime.hpp>
#include <vmime/net/imap/IMAPStore.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

// ============================================================
// IMAP 服务器连通性测试
//
// 编译：cmake --build build --target test_imap
// 运行：build/test_imap
//
// 测试内容：
//   1. TCP 连接
//   2. IMAP 认证（LOGIN）
//   3. 获取文件夹列表
//   4. 打开 INBOX 并获取邮件概览
// ============================================================

static QString extractContent(const vmime::shared_ptr<const vmime::contentHandler> &ct)
{
    std::string buffer;
    vmime::utility::outputStreamStringAdapter adapter(buffer);
    ct->extract(adapter);
    return QString::fromStdString(buffer);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "\n================================================";
    qDebug() << "          IMAP 连接测试";
    qDebug() << "================================================\n";

    // ---- 配置 ----
    const QString host     = "localhost";
    const int     port     = 143;
    const QString username = "t1@mail.hgmail.xin";
    const QString password = "123456";

    qDebug() << "  服务器:" << host << ":" << port;
    qDebug() << "  加密:   无（明文）";
    qDebug() << "  用户:   " << username;
    qDebug() << "  密码:   " << QString(password.size(), '*');
    qDebug() << "";

    // ========== 主流程 ==========
    try {
        // ---- 1. 创建 Session ----
        qDebug() << "▶ [1/5] 创建 VMime session...";
        auto session = vmime::net::session::create();
        qDebug() << "     OK\n";

        // ---- 2. 设置认证 ----
        qDebug() << "▶ [2/5] 设置认证信息...";
        session->getProperties()["store.imap.auth.username"] = username.toStdString();
        session->getProperties()["store.imap.auth.password"] = password.toStdString();
        session->getProperties()["store.imap.options.need-authentication"] = true;
        session->getProperties()["store.imap.connection.tls.required"] = true;
        // 不设 connection.tls —— 让 VMime 根据服务器 CAPABILITY 自动协商 STARTTLS
        qDebug() << "     OK\n";

        // ---- 3. 连接 ----
        qDebug() << "▶ [3/5] 连接 imap://" << host << ":" << port;
        auto store = session->getStore(
            vmime::utility::url(QString("imap://%1:%2").arg(host).arg(port).toStdString()));
        store->connect();
        qDebug() << "     ✓ 连接成功\n";

        // ---- 4. 读取 INBOX 邮件 ----
        qDebug() << "▶ [4/5] 打开 INBOX...";

        auto inbox = store->getFolder(vmime::utility::path("INBOX"));
        inbox->open(vmime::net::folder::MODE_READ_ONLY);
        qDebug() << "     ✓ INBOX 打开成功\n";

        int total = static_cast<int>(inbox->getMessageCount());
        qDebug() << "     INBOX 共" << total << "封邮件\n";

        if (total > 0) {
            int fetchCount = qMin(5, total);
            int start = total - fetchCount + 1;
            qDebug() << "     取最近" << fetchCount << "封 (序号" << start << "-" << total << "):\n";

            vmime::net::messageSet msgSet = vmime::net::messageSet::byNumber(
                static_cast<size_t>(start), static_cast<size_t>(total));

            // 一次 FETCH 拿到 FLAGS + ENVELOPE + SIZE + UID
            vmime::net::fetchAttributes attrs;
            attrs.add(vmime::net::fetchAttributes::FLAGS);
            attrs.add(vmime::net::fetchAttributes::ENVELOPE);
            attrs.add(vmime::net::fetchAttributes::SIZE);
            auto msgs = inbox->getAndFetchMessages(msgSet, attrs);

            // 倒序——最新的先显示
            for (int i = static_cast<int>(msgs.size()) - 1; i >= 0; --i) {
                const auto &vm = msgs[i];
                qDebug() << "  ────────────────────────────────────────";
                qDebug() << "  序号:" << vm->getNumber()
                         << "  大小:" << vm->getSize() << "bytes";
                int flags = static_cast<int>(vm->getFlags());
                qDebug() << "  标记:" << flags
                         << (flags & 1 ? "[已读]" : "[未读]");

                try {
                    auto hdr = vm->getHeader();
                    if (hdr) {
                        try {
                            qDebug() << "  主题:"
                                     << QString::fromStdString(
                                         hdr->Subject()->getValue<vmime::text>()->getWholeBuffer());
                        } catch (...) {
                            qDebug() << "  主题: (无)";
                        }

                        try {
                            qDebug() << "  发件人:"
                                     << QString::fromStdString(
                                         hdr->From()->getValue<vmime::mailbox>()->generate());
                        } catch (...) {
                            qDebug() << "  发件人: (未知)";
                        }

                        try {
                            auto dateField = hdr->Date();
                            if (dateField)
                                qDebug() << "  日期:" << QString::fromStdString(
                                    dateField->getValue<vmime::datetime>()->generate());
                        } catch (...) { }
                    }
                } catch (...) {
                    qDebug() << "  头部解析失败";
                }
                qDebug() << "";
            }
        } else {
            qDebug() << "     INBOX 为空，没有邮件可展示。\n";
        }

        // 关闭 INBOX 后才能打开其他文件夹
        inbox->close(false);
        qDebug() << "     INBOX 已关闭\n";

        // ---- 5. 文件夹列表（在关闭 INBOX 之后做） ----
        qDebug() << "▶ [5/5] 获取文件夹列表...";
        auto root = store->getRootFolder();
        auto folders = root->getFolders(false);

        qDebug() << "     找到" << folders.size() << "个顶级文件夹:\n";
        for (const auto &f : folders) {
            QString name = QString::fromStdString(f->getName().getBuffer());
            QString path = QString::fromStdString(
                f->getFullPath().toString("/", vmime::charset::getLocalCharset()));
            qDebug() << "   ── " << path;

            try {
                auto subs = f->getFolders(false);
                if (!subs.empty()) {
                    for (const auto &sub : subs) {
                        QString subName = QString::fromStdString(sub->getName().getBuffer());
                        qDebug() << "        └ " << subName;
                    }
                }
            } catch (...) { }
        }
        qDebug() << "";

        // ---- 清理 ----
        store->disconnect();
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
