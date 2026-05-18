#ifndef VSMIMENETWORKADAPTER_H
#define VSMIMENETWORKADAPTER_H
#include <QString>
#include <QObject>
#include <vmime/vmime.hpp>

class VSMimeNetworkAdapter : public QObject
{
public:
    VSMimeNetworkAdapter(QObject*);
    ~VSMimeNetworkAdapter() = default;

    void sendEmailAsync(const QString& recipient, const QString& subject, const QString& body);
private:
    std::shared_ptr<vmime::net::session> m_session;
};


#endif // VSMIMENETWORKADAPTER_H
