#ifndef GOOGLEMUSICCLIENT_H
#define GOOGLEMUSICCLIENT_H

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QNetworkRequest>

class QNetworkAccessManager;
class QNetworkReply;
class QIODevice;
class QJsonObject;

class GoogleMusicClient : public QObject {
  Q_OBJECT

 public:
  GoogleMusicClient(QObject* parent);
  ~GoogleMusicClient();

  void setAuthInfo(const QByteArray& auth, const QByteArray& xt, const QByteArray& sjsaid);
  void resetAuth();

  enum Error {
    NoError = 0,
    NetworkError,
    AuthorizationError,
    ProtocolError
  };

 public slots:
  void GetAllSongs();
  void GetAllPlaylists();
  void GetSongUrl(const QUrl& url) const;
  //Playlist routines
  void GetPlaylistSongs(const QString& id);
  void CreatePlaylist(const QString& title);
  void RenamePlaylist(const QString& id, const QString& title);
  void DeletePlaylist(const QString& id);
  void AddSongsToPlaylist(const QString& id, const QStringList& song_list);
  void DeleteSongsFromPlaylist(const QString& id, const QStringList& song_list, const QStringList &album_entry_ids);

signals:
  void TracksGot(QVariantList songJson);
  void TracksLoaded(int error);

  void PlaylistsGot(QVariantList playlistsJson);
  void PlaylistsLoaded(int error);

  void SongUrlGot(QUrl url, int error);

  void PlaylistSongsGot(QString id, QVariantMap playlistJson, int error);
  void PlaylistCreated(QString id, QString title, int error);
  void PlaylistRenamed(QString id, QString title, int error);
  void PlaylistDeleted(QString id, int error);
  void SongsAddedToPlaylist(QString id, QVariantList songJson, int error);
  void SongsDeletedFromPlaylist(QString id, QVariantList songJson, int error);

public:
  static int networkError(QNetworkReply* reply);
private:
  QNetworkRequest createRequest(const QUrl& url) const;
  static QVariantMap getRootJson(QNetworkReply* reply, int *error = NULL);
  static QByteArray continuationPostData(const QByteArray& continuationToken);

  void RequestAllTracks(const QByteArray& continuationToken = QByteArray());
  void RequestAllPlaylists();

private slots:
  void RequestAllTracksFinished();
  void GetAllPlaylistsFinished();
  void GetSongUrlFinished();

  void GetPlaylistSongsFinished();
  void CreatePlaylistFinished();
  void RenamePlaylistFinished(QNetworkReply* reply, QString id, QString title);
  void DeletePlaylistFinished();
  void AddSongsToPlaylistFinished();
  void DeleteSongsFromPlaylistFinished();

 private:
  QByteArray xt_;
  QByteArray sjsaid_;
  QByteArray auth_;
  QNetworkAccessManager* network_;
};

class GoogleAuthorizer: public QObject {
    Q_OBJECT
public:
  GoogleAuthorizer(QObject* parent);
public slots:
  void Authorize(const QString& username, const QString& password);
signals:
  void Authorized(QByteArray auth, QByteArray xt, QByteArray sjsaid);
  void Error();
private slots:
  void LoginFinished();
  void GetCookies();
  void CookiesGot();
private:
 QByteArray auth_;
 QNetworkAccessManager* network_;
};

#endif // GOOGLEMUSICCLIENT_H
