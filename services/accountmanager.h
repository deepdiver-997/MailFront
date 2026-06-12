#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QObject>
#include <QList>
#include "core/account.h"

// ============================================================
// Qt 知识点：QSettings
//
// QSettings 是跨平台配置持久化方案：
//   - macOS: ~/Library/Preferences/com.xxx.plist
//   - Windows: 注册表 HKCU\Software\xxx
//   - Linux: ~/.config/xxx.ini
//
// 用法：
//   QSettings settings("org", "app");
//   settings.setValue("key", value);   // 写入
//   QString v = settings.value("key", "default").toString(); // 读取
//   settings.beginGroup("group");       // 分组读写
//   settings.endGroup();
// ============================================================
class AccountManager : public QObject
{
    Q_OBJECT

public:
    explicit AccountManager(QObject *parent = nullptr);

    QList<Account> accounts() const;
    void addAccount(const Account &account);
    void updateAccount(int index, const Account &account);
    void removeAccount(int index);
    int accountCount() const;

    Account defaultAccount() const;
    int defaultAccountIndex() const;
    void setDefaultAccountIndex(int index);

signals:
    void accountsChanged();

private:
    void loadFromSettings();
    void saveToSettings();

    QList<Account> m_accounts;
    int m_defaultIndex;
};

#endif // ACCOUNTMANAGER_H
