#ifndef MAILPREVIEW_H
#define MAILPREVIEW_H

#include <QWidget>
#include <QTextBrowser>
#include <QLabel>
#include "core/email.h"

class MailPreview : public QWidget
{
    Q_OBJECT
public:
    explicit MailPreview(QWidget *parent = nullptr);

    void showEmail(const Email &email);
    void clear();

private:
    QLabel *m_subjectLabel;
    QLabel *m_fromLabel;
    QLabel *m_dateLabel;
    QTextBrowser *m_bodyView;
    QWidget *m_headerWidget;
};

#endif // MAILPREVIEW_H
