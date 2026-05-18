#include "settingsdialog.h"
#include "services/accountmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QSplitter>

SettingsDialog::SettingsDialog(AccountManager *accountMgr, QWidget *parent)
    : QDialog(parent)
    , m_mgr(accountMgr)
    , m_currentIndex(-1)
    , m_dirty(false)
    , m_isNewAccount(false)
{
    setWindowTitle("账户设置");
    setMinimumSize(750, 500);
    setupUi();
    loadAccountList();
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // 上部分：左右分栏
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // ---- 左侧：账户列表 ----
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_accountList = new QListWidget(leftWidget);
    m_accountList->setMinimumWidth(180);
    leftLayout->addWidget(m_accountList);

    auto *btnLayout = new QHBoxLayout();
    m_addBtn     = new QPushButton("添加", leftWidget);
    m_removeBtn  = new QPushButton("删除", leftWidget);
    m_defaultBtn = new QPushButton("设为默认", leftWidget);
    m_removeBtn->setEnabled(false);
    m_defaultBtn->setEnabled(false);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addWidget(m_defaultBtn);
    leftLayout->addLayout(btnLayout);

    // ---- 右侧：账户详情表单 ----
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    auto *form = new QFormLayout();

    // 基本信息
    m_emailEdit = new QLineEdit(rightWidget);
    m_emailEdit->setPlaceholderText("user@example.com");
    m_emailEdit->setMinimumWidth(280);
    m_displayNameEdit = new QLineEdit(rightWidget);
    m_displayNameEdit->setPlaceholderText("显示名称（可选）");
    m_displayNameEdit->setMinimumWidth(280);
    m_defaultBadge = new QLabel(rightWidget);
    m_defaultBadge->setStyleSheet("color: green; font-weight: bold;");
    m_defaultBadge->hide();

    form->addRow("邮箱地址:", m_emailEdit);
    form->addRow("显示名称:", m_displayNameEdit);
    form->addRow("", m_defaultBadge);

    // SMTP 分组
    auto *smtpGroup = new QGroupBox("SMTP 发信服务器", rightWidget);
    auto *smtpForm = new QFormLayout(smtpGroup);
    m_smtpHostEdit = new QLineEdit(smtpGroup);
    m_smtpHostEdit->setPlaceholderText("smtp.example.com");
    m_smtpHostEdit->setMinimumWidth(280);
    m_smtpPortSpin = new QSpinBox(smtpGroup);
    m_smtpPortSpin->setRange(1, 65535);
    m_smtpPortSpin->setValue(465);
    m_smtpTlsCheck = new QCheckBox("使用 TLS/SSL", smtpGroup);
    m_smtpTlsCheck->setChecked(true);
    m_smtpUserEdit = new QLineEdit(smtpGroup);
    m_smtpUserEdit->setMinimumWidth(280);
    m_smtpPassEdit = new QLineEdit(smtpGroup);
    m_smtpPassEdit->setEchoMode(QLineEdit::Password);
    m_smtpPassEdit->setMinimumWidth(280);

    smtpForm->addRow("服务器:", m_smtpHostEdit);
    smtpForm->addRow("端口:", m_smtpPortSpin);
    smtpForm->addRow("", m_smtpTlsCheck);
    smtpForm->addRow("用户名:", m_smtpUserEdit);
    smtpForm->addRow("密码:", m_smtpPassEdit);

    // IMAP 分组
    auto *imapGroup = new QGroupBox("IMAP 收信服务器", rightWidget);
    auto *imapForm = new QFormLayout(imapGroup);
    m_imapHostEdit = new QLineEdit(imapGroup);
    m_imapHostEdit->setPlaceholderText("imap.example.com");
    m_imapHostEdit->setMinimumWidth(280);
    m_imapPortSpin = new QSpinBox(imapGroup);
    m_imapPortSpin->setRange(1, 65535);
    m_imapPortSpin->setValue(993);
    m_imapTlsCheck = new QCheckBox("使用 TLS/SSL", imapGroup);
    m_imapTlsCheck->setChecked(true);
    m_imapUserEdit = new QLineEdit(imapGroup);
    m_imapUserEdit->setMinimumWidth(280);
    m_imapPassEdit = new QLineEdit(imapGroup);
    m_imapPassEdit->setEchoMode(QLineEdit::Password);
    m_imapPassEdit->setMinimumWidth(280);

    imapForm->addRow("服务器:", m_imapHostEdit);
    imapForm->addRow("端口:", m_imapPortSpin);
    imapForm->addRow("", m_imapTlsCheck);
    imapForm->addRow("用户名:", m_imapUserEdit);
    imapForm->addRow("密码:", m_imapPassEdit);

    rightLayout->addLayout(form);
    rightLayout->addWidget(smtpGroup);
    rightLayout->addWidget(imapGroup);
    rightLayout->addStretch();

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter);

    // ---- 底部按钮 ----
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // ---- 信号连接 ----
    connect(m_accountList, &QListWidget::currentRowChanged,
            this, &SettingsDialog::onAccountSelected);
    connect(m_addBtn, &QPushButton::clicked,
            this, &SettingsDialog::onAddAccount);
    connect(m_removeBtn, &QPushButton::clicked,
            this, &SettingsDialog::onRemoveAccount);
    connect(m_defaultBtn, &QPushButton::clicked,
            this, &SettingsDialog::onSetDefault);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 字段变更标记
    auto editors = {m_emailEdit, m_displayNameEdit, m_smtpHostEdit, m_smtpUserEdit,
                    m_smtpPassEdit, m_imapHostEdit, m_imapUserEdit, m_imapPassEdit};
    for (auto *e : editors)
        connect(e, &QLineEdit::textChanged, this, &SettingsDialog::onFieldChanged);
    connect(m_smtpTlsCheck, &QCheckBox::toggled, this, &SettingsDialog::onFieldChanged);
    connect(m_imapTlsCheck, &QCheckBox::toggled, this, &SettingsDialog::onFieldChanged);
    connect(m_smtpPortSpin, &QSpinBox::valueChanged, this, &SettingsDialog::onFieldChanged);
    connect(m_imapPortSpin, &QSpinBox::valueChanged, this, &SettingsDialog::onFieldChanged);
}

void SettingsDialog::loadAccountList()
{
    m_accountList->blockSignals(true);
    m_accountList->clear();

    QList<Account> accounts = m_mgr->accounts();
    for (const Account &acc : accounts) {
        QString label = acc.email;
        if (acc.email == m_mgr->defaultAccount().email)
            label += "  ★";
        m_accountList->addItem(label);
    }

    m_accountList->blockSignals(false);

    if (m_accountList->count() > 0) {
        m_accountList->setCurrentRow(0);
    } else {
        clearFields();
    }
}

void SettingsDialog::showAccount(int index)
{
    if (index < 0 || index >= m_mgr->accountCount()) {
        clearFields();
        return;
    }

    Account acc = m_mgr->accounts().at(index);
    m_isNewAccount = false;
    m_dirty = false;

    m_emailEdit->setText(acc.email);
    m_displayNameEdit->setText(acc.displayName);
    m_smtpHostEdit->setText(acc.smtpHost);
    m_smtpPortSpin->setValue(acc.smtpPort);
    m_smtpTlsCheck->setChecked(acc.smtpUseTls);
    m_smtpUserEdit->setText(acc.smtpUser);
    m_smtpPassEdit->setText(acc.smtpPassword);
    m_imapHostEdit->setText(acc.imapHost);
    m_imapPortSpin->setValue(acc.imapPort);
    m_imapTlsCheck->setChecked(acc.imapUseTls);
    m_imapUserEdit->setText(acc.imapUser);
    m_imapPassEdit->setText(acc.imapPassword);

    bool isDefault = (acc.email == m_mgr->defaultAccount().email);
    m_defaultBadge->setVisible(isDefault);
    if (isDefault)
        m_defaultBadge->setText("★ 当前默认账户");

    // 表单可编辑
    for (auto *w : {m_emailEdit, m_smtpHostEdit})
        w->setReadOnly(false);

    m_dirty = false;
}

void SettingsDialog::clearFields()
{
    m_currentIndex = -1;
    m_dirty = false;
    for (auto *e : {m_emailEdit, m_displayNameEdit, m_smtpHostEdit, m_smtpUserEdit,
                    m_smtpPassEdit, m_imapHostEdit, m_imapUserEdit, m_imapPassEdit})
        e->clear();
    m_defaultBadge->hide();
}

void SettingsDialog::onAccountSelected(int row)
{
    // 如果有未保存更改，先保存
    if (m_dirty && m_currentIndex >= 0 && !m_isNewAccount) {
        onSave();
    }
    m_currentIndex = row;
    showAccount(row);
    m_removeBtn->setEnabled(true);
    m_defaultBtn->setEnabled(true);
}

void SettingsDialog::onAddAccount()
{
    m_currentIndex = -1;
    m_isNewAccount = true;
    m_dirty = true;
    clearFields();
    m_emailEdit->setReadOnly(false);
    m_emailEdit->setFocus();
    m_removeBtn->setEnabled(false);
    m_defaultBtn->setEnabled(false);
}

void SettingsDialog::onRemoveAccount()
{
    int row = m_accountList->currentRow();
    if (row < 0) return;

    Account acc = m_mgr->accounts().at(row);
    auto reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除账户 %1 吗？").arg(acc.email));
    if (reply != QMessageBox::Yes) return;

    m_mgr->removeAccount(row);
    loadAccountList();
    clearFields();
}

void SettingsDialog::onSetDefault()
{
    int row = m_accountList->currentRow();
    if (row >= 0) {
        m_mgr->setDefaultAccountIndex(row);
        loadAccountList();
        m_accountList->setCurrentRow(row);
    }
}

bool SettingsDialog::validateFields()
{
    if (m_emailEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入邮箱地址");
        m_emailEdit->setFocus();
        return false;
    }
    if (m_smtpHostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 SMTP 服务器地址");
        m_smtpHostEdit->setFocus();
        return false;
    }
    if (m_imapHostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 IMAP 服务器地址");
        m_imapHostEdit->setFocus();
        return false;
    }
    return true;
}

void SettingsDialog::onSave()
{
    if (!m_dirty && !m_isNewAccount) return;
    if (!validateFields()) return;

    Account acc;
    acc.email        = m_emailEdit->text().trimmed();
    acc.displayName  = m_displayNameEdit->text().trimmed();
    acc.smtpHost     = m_smtpHostEdit->text().trimmed();
    acc.smtpPort     = m_smtpPortSpin->value();
    acc.smtpUseTls   = m_smtpTlsCheck->isChecked();
    acc.smtpUser     = m_smtpUserEdit->text().trimmed();
    acc.smtpPassword = m_smtpPassEdit->text();
    acc.imapHost     = m_imapHostEdit->text().trimmed();
    acc.imapPort     = m_imapPortSpin->value();
    acc.imapUseTls   = m_imapTlsCheck->isChecked();
    acc.imapUser     = m_imapUserEdit->text().trimmed();
    acc.imapPassword = m_imapPassEdit->text();

    if (m_isNewAccount) {
        m_mgr->addAccount(acc);
    } else if (m_currentIndex >= 0) {
        m_mgr->updateAccount(m_currentIndex, acc);
    }

    m_dirty = false;
    m_isNewAccount = false;
    loadAccountList();

    if (m_currentIndex >= 0 && m_currentIndex < m_accountList->count())
        m_accountList->setCurrentRow(m_currentIndex);
}

void SettingsDialog::onFieldChanged()
{
    m_dirty = true;
}
