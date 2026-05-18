#include "mailpreview.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QFrame>

MailPreview::MailPreview(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // 邮件头区域
    m_headerWidget = new QWidget(this);
    auto *headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 8, 8, 8);

    m_subjectLabel = new QLabel("", m_headerWidget);
    QFont subjectFont = m_subjectLabel->font();
    subjectFont.setPointSize(14);
    subjectFont.setBold(true);
    m_subjectLabel->setFont(subjectFont);
    m_subjectLabel->setWordWrap(true);

    m_fromLabel = new QLabel("", m_headerWidget);
    m_dateLabel = new QLabel("", m_headerWidget);
    m_dateLabel->setStyleSheet("color: gray;");

    auto *fromDateLayout = new QHBoxLayout();
    fromDateLayout->addWidget(m_fromLabel);
    fromDateLayout->addStretch();
    fromDateLayout->addWidget(m_dateLabel);

    headerLayout->addWidget(m_subjectLabel);
    headerLayout->addLayout(fromDateLayout);

    // 分隔线
    auto *line = new QFrame(m_headerWidget);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    headerLayout->addWidget(line);

    // 正文区域
    m_bodyView = new QTextBrowser(this);
    m_bodyView->setOpenExternalLinks(true);
    m_bodyView->setReadOnly(true);

    layout->addWidget(m_headerWidget);
    layout->addWidget(m_bodyView, 1);

    clear();
}

void MailPreview::showEmail(const Email &email)
{
    m_subjectLabel->setText(email.subject);
    m_fromLabel->setText(QString("发件人: %1").arg(email.from));
    m_dateLabel->setText(email.date.toString("yyyy-MM-dd hh:mm:ss"));
    m_bodyView->setHtml(email.body);
    m_headerWidget->setVisible(true);
    m_bodyView->setVisible(true);
}

void MailPreview::clear()
{
    m_subjectLabel->clear();
    m_fromLabel->clear();
    m_dateLabel->clear();
    m_bodyView->clear();
    m_headerWidget->setVisible(false);
    m_bodyView->setVisible(false);
}
