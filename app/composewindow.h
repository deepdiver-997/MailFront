#ifndef COMPOSEWINDOW_H
#define COMPOSEWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QAction>
#include "core/email.h"

class AccountManager;

class ComposeWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ComposeWindow(AccountManager *accountMgr, QWidget *parent = nullptr);

    void setReplyMode(const Email &originalEmail);

signals:
    void sendRequested(const Email &email);

private slots:
    void onSendClicked();

private:
    void setupUi();
    void setupToolBar();

    AccountManager *m_mgr;

    QComboBox *m_fromCombo;
    QLineEdit *m_toEdit;
    QLineEdit *m_ccEdit;
    QLineEdit *m_subjectEdit;
    QTextEdit *m_bodyEdit;

    QAction *m_sendAction;
};

#endif // COMPOSEWINDOW_H
