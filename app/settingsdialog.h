#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

class AccountManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AccountManager *accountMgr, QWidget *parent = nullptr);

private slots:
    void onAccountSelected(int row);
    void onAddAccount();
    void onRemoveAccount();
    void onSetDefault();
    void onSave();
    void onFieldChanged();

private:
    void setupUi();
    void loadAccountList();
    void showAccount(int index);
    void clearFields();
    bool validateFields();

    AccountManager *m_mgr;

    // 左侧账户列表
    QListWidget *m_accountList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_defaultBtn;

    // 右侧表单
    QLineEdit *m_emailEdit;
    QLineEdit *m_displayNameEdit;

    // SMTP
    QLineEdit *m_smtpHostEdit;
    QSpinBox  *m_smtpPortSpin;
    QCheckBox *m_smtpTlsCheck;
    QLineEdit *m_smtpUserEdit;
    QLineEdit *m_smtpPassEdit;

    // IMAP
    QLineEdit *m_imapHostEdit;
    QSpinBox  *m_imapPortSpin;
    QCheckBox *m_imapTlsCheck;
    QLineEdit *m_imapUserEdit;
    QLineEdit *m_imapPassEdit;

    QLabel *m_defaultBadge;

    int m_currentIndex;
    bool m_dirty;
    bool m_isNewAccount;
};

#endif // SETTINGSDIALOG_H
