#include "throttlednetworkmanager.h"
#include <QTimer>
#include <QNetworkReply>
#include <QChildEvent>
#include <QMutex>
#include <QMutexLocker>

NetworkReply::NetworkReply(QObject *parent)
  : QObject(parent),data_(0)
{
}

void NetworkReply::replyFinished() {
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  Q_ASSERT(reply!=0);
  Q_ASSERT(reply==data_);
  emit finished(reply);
}

ThrottledNetworkManager::ThrottledNetworkManager(int max_requests_per_second, QObject *parent) :
  QNetworkAccessManager(parent)
{
  mutex_ = new QMutex();
  timer_ = new QTimer(this);
  timer_->setInterval(1000/max_requests_per_second);
  connect(timer_,SIGNAL(timeout()),SLOT(ProcessQueue()));
  connect(this,SIGNAL(TimeToProcessQueue()),SLOT(ProcessQueue()));
}


NetworkReply* ThrottledNetworkManager::throttledGet(const QNetworkRequest &request) {
  QMutexLocker locker(mutex_);

  NetworkReply* reply = new NetworkReply();
  QPair<NetworkReply*,QNetworkRequest> pair(reply,request);

  queue_.enqueue(pair);

  if (queue_.size()==1) {
    locker.unlock();
    emit TimeToProcessQueue();
    timer_->stop();
  } else if (!timer_->isActive())
    timer_->start();
  return reply;
}

void ThrottledNetworkManager::ProcessQueue() {
  QMutexLocker locker(mutex_);

  if (queue_.isEmpty()) {
    timer_->stop();
    return;
  }

  QPair<NetworkReply*,QNetworkRequest> pair = queue_.dequeue();
  QNetworkReply* reply = get(pair.second);
  pair.first->data_ = reply;
  connect(reply,SIGNAL(finished()),pair.first,SLOT(replyFinished()));

  if (queue_.isEmpty())
    timer_->stop();
  else
    timer_->start();
}
