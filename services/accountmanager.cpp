#include "accountmanager.h"
#include "cryptohelper.h"
#include <QSettings>

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
    , m_defaultIndex(0)
{
    loadFromSettings();
}

void AccountManager::loadFromSettings()
{
    QSettings settings;

    int size = settings.beginReadArray("accounts");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        Account acc;
        acc.email         = settings.value("email").toString();
        acc.displayName   = settings.value("displayName").toString();
        acc.smtpHost      = settings.value("smtpHost").toString();
        acc.smtpPort      = settings.value("smtpPort", 465).toInt();
        acc.smtpUseTls    = settings.value("smtpUseTls", true).toBool();
        acc.smtpUser      = settings.value("smtpUser").toString();
        acc.smtpPassword  = CryptoHelper::decrypt(
                               settings.value("smtpPassword").toString());
        acc.imapHost      = settings.value("imapHost").toString();
        acc.imapPort      = settings.value("imapPort", 993).toInt();
        acc.imapUseTls    = settings.value("imapUseTls", true).toBool();
        acc.imapUser      = settings.value("imapUser").toString();
        acc.imapPassword  = CryptoHelper::decrypt(
                               settings.value("imapPassword").toString());

        if (acc.isValid()) {
            m_accounts.append(acc);
        }
    }
    settings.endArray();

    m_defaultIndex = settings.value("defaultAccount", 0).toInt();
}

void AccountManager::saveToSettings()
{
    QSettings settings;

    settings.beginWriteArray("accounts");
    for (int i = 0; i < m_accounts.size(); ++i) {
        settings.setArrayIndex(i);
        const Account &acc = m_accounts[i];

        settings.setValue("email",        acc.email);
        settings.setValue("displayName",  acc.displayName);
        settings.setValue("smtpHost",     acc.smtpHost);
        settings.setValue("smtpPort",     acc.smtpPort);
        settings.setValue("smtpUseTls",   acc.smtpUseTls);
        settings.setValue("smtpUser",     acc.smtpUser);
        settings.setValue("smtpPassword", CryptoHelper::encrypt(acc.smtpPassword));
        settings.setValue("imapHost",     acc.imapHost);
        settings.setValue("imapPort",     acc.imapPort);
        settings.setValue("imapUseTls",   acc.imapUseTls);
        settings.setValue("imapUser",     acc.imapUser);
        settings.setValue("imapPassword", CryptoHelper::encrypt(acc.imapPassword));
    }
    settings.endArray();

    settings.setValue("defaultAccount", m_defaultIndex);
    settings.sync();
}

QList<Account> AccountManager::accounts() const
{
    return m_accounts;
}

void AccountManager::addAccount(const Account &account)
{
    m_accounts.append(account);
    saveToSettings();
    emit accountsChanged();
}

void AccountManager::updateAccount(int index, const Account &account)
{
    if (index >= 0 && index < m_accounts.size()) {
        m_accounts[index] = account;
        saveToSettings();
        emit accountsChanged();
    }
}

void AccountManager::removeAccount(int index)
{
    if (index >= 0 && index < m_accounts.size()) {
        m_accounts.removeAt(index);
        if (m_defaultIndex >= m_accounts.size()) {
            m_defaultIndex = qMax(0, m_accounts.size() - 1);
        }
        saveToSettings();
        emit accountsChanged();
    }
}

int AccountManager::accountCount() const
{
    return m_accounts.size();
}

Account AccountManager::defaultAccount() const
{
    if (m_accounts.isEmpty()) return Account();
    if (m_defaultIndex < 0 || m_defaultIndex >= m_accounts.size()) return m_accounts.first();
    return m_accounts[m_defaultIndex];
}

void AccountManager::setDefaultAccountIndex(int index)
{
    if (index >= 0 && index < m_accounts.size()) {
        m_defaultIndex = index;
        QSettings settings;
        settings.setValue("defaultAccount", m_defaultIndex);
    }
}
