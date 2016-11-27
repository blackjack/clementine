#include "googlemusicclient.h"
#include "core/closure.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QNetworkAccessManager>
#include <QUrlQuery>
#include <QRegExp>

namespace {
const char* kDownloadAllUrl = "https://www.googleapis.com/sj/v1.1/trackfeed";
const char* kGetSongUrl = "https://play.google.com/music/play?u=0&pt=e&songid=%1&format=jsarray";
//const char* kCreateInstantMixUrl = "https://play.google.com/music/services/getmixentries?u=0&xt=%1";
const char* kGetAllPlaylistsUrl = "https://www.googleapis.com/sj/v1.1/playlistfeed";
//const char* kAddPlaylistUrl = "https://play.google.com/music/services/addplaylist?u=0&xt=%1";
//const char* kRenamePlaylistUrl = "https://play.google.com/music/services/modifyplaylist?u=0&xt=%1";
//const char* kDeletePlaylistUrl = "https://play.google.com/music/services/deleteplaylist?u=0&xt=%1";
//const char* kAddToPlaylistUrl = "https://play.google.com/music/services/addtoplaylist?u=0&xt=%1";
//const char* kDeleteSongUrl = "https://play.google.com/music/services/deletesong?u=0&xt=%1";

//Auth URLs
const char* kGoogleAuthUrl = "https://www.google.com/accounts/ClientLogin";
const char* kGoogleMusicAuthUrl = "https://play.google.com/music/listen?hl=en&u=0";
} //namespace

GoogleMusicClient::GoogleMusicClient(QObject *parent):
  QObject(parent),
  network_(new QNetworkAccessManager(this))
{
}

GoogleMusicClient::~GoogleMusicClient()
{
}

void GoogleMusicClient::setAuthInfo(const QByteArray &auth, const QByteArray &xt, const QByteArray &sjsaid)
{
  auth_ = auth;
  xt_ = xt;
  sjsaid_ = sjsaid;

  network_->setCookieJar(new QNetworkCookieJar);
  QList<QNetworkCookie> cookies;
  cookies << QNetworkCookie("xt",xt)
          << QNetworkCookie("sjsaid",sjsaid);
  network_->cookieJar()->setCookiesFromUrl(cookies,QUrl("https://play.google.com"));
}

void GoogleMusicClient::resetAuth()
{
  auth_.clear();
  xt_.clear();
  sjsaid_.clear();
  network_->setCookieJar(new QNetworkCookieJar);
}

void GoogleMusicClient::GetAllSongs()
{
  RequestAllTracks();
}

void GoogleMusicClient::GetAllPlaylists()
{
  RequestAllPlaylists();
}

void GoogleMusicClient::GetSongUrl(const QUrl &url) const
{
  QString songID = url.host();
  QUrl reqUrl = QUrl(QString(kGetSongUrl).arg(songID));
  QNetworkRequest req(reqUrl);
  req.setRawHeader("Authorization","GoogleLogin auth="+auth_);

  QNetworkReply* reply = network_->get(req);
  connect(reply, SIGNAL(finished()), SLOT(GetSongUrlFinished()));
}

void GoogleMusicClient::GetPlaylistSongs(const QString &id)
{
//  QUrl url = QString(kGetAllPlaylistsUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;
//  QVariantMap json;
//  json.insert("id",id);
//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);
//  connect(reply, SIGNAL(finished()), SLOT(GetPlaylistSongsFinished()));
}

void GoogleMusicClient::CreatePlaylist(const QString &title)
{
//  QString url = QString(kAddPlaylistUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;
//  QVariantMap json;
//  json.insert("title",title);
//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);
//  connect(reply, SIGNAL(finished()), SLOT(CreatePlaylistFinished()));
}

void GoogleMusicClient::RenamePlaylist(const QString &id, const QString &title)
{
//  QString url = QString(kRenamePlaylistUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;
//  QVariantMap json;
//  json.insert("playlistId",id);
//  json.insert("playlistName",title);
//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);

//  NewClosure(reply, SIGNAL(finished()),
//             this,SLOT(RenamePlaylistFinished(QNetworkReply*,QString,QString)),
//             reply,id,title);
}

void GoogleMusicClient::DeletePlaylist(const QString &id)
{
//  QString url = QString(kDeletePlaylistUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;
//  QVariantMap json;
//  json.insert("id",id);
//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);
//  connect(reply, SIGNAL(finished()), SLOT(DeletePlaylistFinished()));
}

void GoogleMusicClient::AddSongsToPlaylist(const QString &id, const QStringList &song_list)
{
//  QString url = QString(kAddToPlaylistUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;
//  QVariantMap json;
//  json.insert("playlistId",id);

//  QVariantList songsJson;
//  foreach (const QString& song, song_list) {
//    QVariantMap s;
//    s.insert("id",song);
//    s.insert("type",1);
//    songsJson << s;
//  }
//  json.insert("songRefs",songsJson);

//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);
//  connect(reply, SIGNAL(finished()), SLOT(AddSongsToPlaylistFinished()));
}

void GoogleMusicClient::DeleteSongsFromPlaylist(const QString &id, const QStringList &song_list, const QStringList& album_entry_ids)
{
//  QString url = QString(kDeleteSongUrl).arg(QString::fromAscii(xt_));
//  QNetworkRequest req = createRequest(url);

//  QJson::Serializer writer;

//  QVariantMap json;
//  json.insert("listId",id);
//  json.insert("songIds",song_list);
//  json.insert("entryIds",album_entry_ids);

//  QByteArray postData= "json="+QUrl::toPercentEncoding(writer.serialize(json));
//  QNetworkReply* reply = network_->post(req,postData);
//  connect(reply, SIGNAL(finished()), SLOT(DeleteSongsFromPlaylistFinished()));
}

void GoogleMusicClient::RequestAllTracks(const QByteArray &continuationToken)
{
  QUrlQuery q;
  q.addQueryItem("alt","json");
  q.addQueryItem("updated-min","0");
  q.addQueryItem("include-tracks","true");

  QJsonObject json;
  if (!continuationToken.isEmpty()) {
    json["start-token"] = QString(continuationToken);
  }

  json["max-results"] = 1000;
  QJsonDocument doc(json);
  QByteArray postData = doc.toJson();

  QUrl url(kDownloadAllUrl);
  url.setQuery(q);
  QNetworkRequest req = createRequest(url);
  QNetworkReply* reply = network_->post(req,postData);
  connect(reply, SIGNAL(finished()), SLOT(RequestAllTracksFinished()));
}

void GoogleMusicClient::RequestAllPlaylists()
{
  QUrlQuery q;
  q.addQueryItem("alt","json");
  q.addQueryItem("updated-min","0");
  q.addQueryItem("include-tracks","true");

  QJsonObject json;
  json["max-results"] = 1000;

  QByteArray postData = QJsonDocument(json).toJson();
  QUrl url(kGetAllPlaylistsUrl);
  url.setQuery(q);
  QNetworkRequest req = createRequest(url);
  QNetworkReply* reply = network_->post(req,postData);
  connect(reply, SIGNAL(finished()), SLOT(GetAllPlaylistsFinished()));
}

void GoogleMusicClient::RequestAllTracksFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);

  if (error) {
    emit TracksLoaded(error);
    return;
  }

  QVariantList songsJson = root["data"].toMap()["items"].toList();
  emit TracksGot(songsJson);

  if (root.contains("nextPageToken"))
    RequestAllTracks(root["nextPageToken"].toByteArray());
  else
    emit TracksLoaded(NoError);
}

void GoogleMusicClient::GetAllPlaylistsFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);

  if (error) {
    emit PlaylistsLoaded(error);
    return;
  }

  QVariantList list = root["data"].toMap()["items"].toList();

  QStringList nonDeletedList;
  for (QVariant item : list) {
    QVariantMap playlist = item.toMap();
    if (playlist["deleted"].toBool()==true) {
      nonDeletedList << playlist["id"].toString();
    }
  }
}

void GoogleMusicClient::GetSongUrlFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit SongUrlGot(QUrl(),error);
  else
    emit SongUrlGot(root["url"].toUrl(),NoError);
}

void GoogleMusicClient::GetPlaylistSongsFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit PlaylistSongsGot(QString(),QVariantMap(),error);
  else
    emit PlaylistSongsGot(root["playlistId"].toString(),root,NoError);
}

void GoogleMusicClient::CreatePlaylistFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit PlaylistCreated(QString(),QString(),error);
  else
    emit PlaylistCreated(root["id"].toString(),root["title"].toString(),NoError);
}

void GoogleMusicClient::RenamePlaylistFinished(QNetworkReply *reply, QString id, QString title)
{
  reply->deleteLater();

  int error;
  getRootJson(reply,&error);
  emit PlaylistRenamed(id,title,error);
}

void GoogleMusicClient::DeletePlaylistFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit PlaylistDeleted(QString(),error);
  else
    emit PlaylistDeleted(root["deleteId"].toString(),NoError);
}

void GoogleMusicClient::AddSongsToPlaylistFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit SongsAddedToPlaylist(QString(),QVariantList(),error);
  else
    emit SongsAddedToPlaylist(root["playlistId"].toString(),root["songIds"].toList(),NoError);
}

void GoogleMusicClient::DeleteSongsFromPlaylistFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  int error;
  QVariantMap root = getRootJson(reply,&error);
  if (error)
    emit SongsDeletedFromPlaylist(QString(),QVariantList(),error);
  else
    emit SongsDeletedFromPlaylist(root["listId"].toString(),root["deleteIds"].toList(),NoError);
}

int GoogleMusicClient::networkError(QNetworkReply *reply)
{
    if (reply->error()!=QNetworkReply::NoError) {
      int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (code>=400 && code<600) {
        //TODO
        return AuthorizationError;
      }
      return NetworkError;
    }
    return NoError;
}

QVariantMap GoogleMusicClient::getRootJson(QNetworkReply *reply, int *error)
{
  if (int e = networkError(reply)) {
    if (error) *error = e;
    return QVariantMap();
  }

  QJsonParseError parseError;
  QByteArray data = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson( data, &parseError);

  if (error) 
    *error = (parseError.error ? ProtocolError : NoError);
  return doc.toVariant().toMap();
}

QNetworkRequest GoogleMusicClient::createRequest(const QUrl &url) const
{
  QNetworkRequest req(url);
  req.setRawHeader("Authorization","GoogleLogin auth="+auth_);
  req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
  return req;
}

QByteArray GoogleMusicClient::continuationPostData(const QByteArray &continuationToken)
{
  QJsonObject json;
  if (!continuationToken.isEmpty())
    json["continuationToken"] = QString(continuationToken);
  return "json="+QUrl::toPercentEncoding(QJsonDocument(json).toJson());
}


GoogleAuthorizer::GoogleAuthorizer(QObject *parent):
    network_(new QNetworkAccessManager(this))
{
}

void GoogleAuthorizer::Authorize(const QString &username, const QString &password)
{
  QUrlQuery q;
  q.addQueryItem("accountType","HOSTED_OR_GOOGLE");
  q.addQueryItem("Email",username);
  q.addQueryItem("Passwd",password);
  q.addQueryItem("service","sj");
  q.addQueryItem("source","Clementine");

  QUrl url(kGoogleAuthUrl,QUrl::StrictMode);
  QNetworkRequest req(url);

  req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
  QNetworkReply* reply = network_->post(req,q.query(QUrl::FullyEncoded).toUtf8());
  connect(reply, SIGNAL(finished()), SLOT(LoginFinished()));
}

void GoogleAuthorizer::LoginFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  Q_ASSERT(reply);
  reply->deleteLater();

  if (!GoogleMusicClient::networkError(reply)) {
    QByteArray arr = reply->readAll();

    QRegExp regexp("Auth\\=(.*)\n");
    if (regexp.indexIn(arr) != -1) {
      auth_ = regexp.cap(1).toLatin1();

      if (!auth_.isEmpty())
        GetCookies();
      else
        emit Error();
    }
  } else
    emit Error();
}

void GoogleAuthorizer::GetCookies()
{
  QUrl url(kGoogleMusicAuthUrl,QUrl::StrictMode);
  QNetworkRequest req(url);
  req.setRawHeader("Authorization","GoogleLogin auth="+auth_);
  req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
  QNetworkReply* reply = network_->post(req,QByteArray());
  connect(reply, SIGNAL(finished()), SLOT(CookiesGot()));
}

void GoogleAuthorizer::CookiesGot()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
  if(!redirectUrl.isEmpty()) {
    QNetworkRequest request;
    request.setUrl(redirectUrl);
    QNetworkReply* redirect = network_->get(request);
    connect(redirect, SIGNAL(finished()), SLOT(CookiesGot()));
    return;
  }

  if (!GoogleMusicClient::networkError(reply)) {
    QList<QNetworkCookie> cookies = network_->cookieJar()->cookiesForUrl(reply->url());

    QByteArray xt, sjsaid;
    foreach (const QNetworkCookie& cookie, cookies) {
      if (cookie.name()=="xt")
        xt = cookie.value();
      else if (cookie.name()=="sjsaid")
        sjsaid = cookie.value();
    }

    if (!xt.isEmpty() && !sjsaid.isEmpty())
      emit Authorized(auth_,xt,sjsaid);
    else
      emit Error();
  } else
    emit Error();
}

