#ifndef VKONTAKTEWORKER_H
#define VKONTAKTEWORKER_H

#include <QObject>
#include <QQueue>
#include <QUrl>
#include <QDomElement>
#include "core/song.h"

class ThrottledNetworkManager;
class QStandardItem;
class QNetworkReply;
class NetworkReply;
class Playlist;

//this class works only with network
class VkontakteApiWorker: public QObject {
  Q_OBJECT

public:

  explicit VkontakteApiWorker(QObject* parent = 0): QObject(parent) {}

  void SetAccessToken(const QString& access_token) { access_token_ = access_token; }
  void SetNetwork(ThrottledNetworkManager* network) { network_ = network; }

  void Stop() {}


signals:
  void TracksGot(const SongList& songs);
  void FriendsGot(const QList<QStringList>& list_of_user_full_name);
  void AlbumsGot(const QList<QStringList>& list_of_owner_album_title);
  void SearchResultsGot(const SongList& songs);
  void AlbumCreated(const QString& album_id);
  void AlbumDeleted();
  void AlbumNameEdited();
  void AddedToMyTracks(const QString& song_id);
  void RemovedFromMyTracks();
  void MovedToAlbum();
  void FullNameGot(const QString& full_name);

  //Errors
  void AuthError();
  void Error(const QString& err_str);

public slots:
  void GetTracksAsync(const QString& user_id);
  void GetFriendsAsync(const QString& user_id);
  void GetAlbumsAsync(const QString& user_id);
  void SearchAsync(const QString& text, int offset = 0, int count = 50);
  void CreateAlbumAsync(const QString& title);
  void DeleteAlbumAsync(const QString& album_id);
  void EditAlbumNameAsync(const QString& album_id, const QString& title);
  void AddToMyTracksAsync(const QString& song_id, const QString& owner_id);
  void RemoveFromMyTracksAsync(const QString& song_id, const QString& owner_id);
  void MoveToAlbumAsync(const QString &song_ids, const QString &album_id);
  void GetFullNameAsync(const QString& user_id);

private slots:
  void GetTracksFinished(QNetworkReply* reply);
  void GetFriendsFinished(QNetworkReply* reply);
  void GetAlbumsFinished(QNetworkReply* reply);
  void SearchFinished(QNetworkReply* reply);
  void CreateAlbumFinished(QNetworkReply* reply);
  void DeleteAlbumFinished(QNetworkReply* reply);
  void EditAlbumNameFinished(QNetworkReply* reply);
  void AddToMyTracksFinished(QNetworkReply* reply);
  void RemoveFromMyTracksFinished(QNetworkReply* reply);
  void MoveToAlbumFinished(QNetworkReply* reply);
  void GetFullNameFinished(QNetworkReply* reply);

private:
  QDomElement GetRootElement(QNetworkReply* data);
  Song ParseSong(QDomElement info);
  QString ReplaceXMLChars(QString s);

private:
  QString access_token_;
  ThrottledNetworkManager* network_;
};

//this class works with GUI
class VkontakteWorker: public VkontakteApiWorker {
    Q_OBJECT

public:
  VkontakteWorker(QObject* parent = 0);

public slots:
  void MultiAddToMyTracks(const QList<QUrl>& urls, const QString& user_id);
  void MultiRemoveFromMyTracks(const QList<QUrl>& urls, const QString& user_id);
  void MultiMoveToAlbum(const QList<QUrl>& urls, const QString& user_id, const QString& album_id);

signals:
  void MultiAddedToMyTracks();
  void MultiRemovedFromMyTracks();
  void MultiMovedToAlbum();

private slots:
  void ProcessAddQueue();
  void ProcessRemoveQueue();

  void AppendAddedSongToMove(const QString& song_id);
  void MoveAddedSongsToAlbum();

private:
  QTimer* timer_;
  QQueue<QUrl> songs_to_add_;
  QQueue<QUrl> songs_to_remove_;

  int left_to_append_;
  QString songs_ids_to_move_;
  QString target_album_id_;
};


//class for doing instant search
class VkontakteSearchWorker: public VkontakteApiWorker {
  Q_OBJECT

public:
  explicit VkontakteSearchWorker(QObject* parent = 0);

public slots:
  void Search(const QString& text, bool now = false);
private slots:
  void DoSearch();

private:
  QTimer* timer_;
  QString pending_search_;

};

#endif // VKONTAKTEWORKER_H
