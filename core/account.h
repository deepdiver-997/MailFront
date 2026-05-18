#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>

struct Account {
    int64_t id = 0;
    QString email;
    QString displayName;
    // SMTP 配置
    QString smtpHost;
    int smtpPort = 465;
    bool smtpUseTls = true;
    QString smtpUser;
    QString smtpPassword;
    // IMAP 配置
    QString imapHost;
    int imapPort = 993;
    bool imapUseTls = true;
    QString imapUser;
    QString imapPassword;

    bool isValid() const {
        return !email.isEmpty()
            && !smtpHost.isEmpty()
            && !imapHost.isEmpty();
    }
};

#endif // ACCOUNT_H
