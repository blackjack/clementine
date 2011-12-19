#ifndef THROTTLEDNETWORKMANAGER_H
#define THROTTLEDNETWORKMANAGER_H

#include <QObject>
#include <QQueue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPair>

class QTimer;
class QNetworkReply;
class QMutex;

class NetworkReply: public QObject
{
    Q_OBJECT

    NetworkReply(QObject* parent = 0);
    friend class ThrottledNetworkManager;

public:
    QNetworkReply* data() { return data_; } //don't use until finished signal emitted!
signals:
    void finished(QNetworkReply*);
private slots:
    void replyFinished();
    void replyDeleted() { deleteLater(); } //delete class on reply's death
private:
    void setData(QNetworkReply* data)
    { data_ = data; connect(data_,SIGNAL(destroyed()),SLOT(replyDeleted())); }
    QNetworkReply* data_;
};


class ThrottledNetworkManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    explicit ThrottledNetworkManager(int max_requests_per_second, QObject *parent = 0);
    NetworkReply* throttledGet(const QNetworkRequest& request);

private slots:
    void ProcessQueue();

signals:
    void TimeToProcessQueue(); //for multithreading purposes, so get() creates child in parent's thread

private:
    QMutex* mutex_;
    QTimer* timer_;
    QQueue< QPair<NetworkReply*,QNetworkRequest> > queue_;
};

#endif // THROTTLEDNETWORKMANAGER_H
