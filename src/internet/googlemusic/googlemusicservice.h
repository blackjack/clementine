#ifndef GOOGLEMUSICSERVICE_H
#define GOOGLEMUSICSERVICE_H

#include "internet/core/internetservice.h"
#include "internet/core/internetmodel.h"
#include <QNetworkRequest>

class QNetworkAccessManager;
class QSortFilterProxyModel;
class QMenu;
class QNetworkReply;

class LibraryBackend;
class LibraryModel;
class GoogleMusicUrlHandler;
class GoogleMusicClient;

class GoogleMusicService : public InternetService {
  Q_OBJECT

 public:
  GoogleMusicService(Application* app, InternetModel* parent);
  ~GoogleMusicService();

  static const char* kServiceName;
  static const char* kSettingsGroup;
  static const char* kSongsTable;
  static const char* kFtsTable;
  static const char* kHomepage;

  enum Role {
     Role_PlaylistID = InternetModel::RoleCount+1,
     Role_PlaylistEntryID
  };

  enum Type {
    Type_Library = InternetModel::TypeCount+1,
    Type_PlaylistList
  };

  QStandardItem* CreateRootItem();
  void LazyPopulate(QStandardItem* item);

  void ShowContextMenu(const QPoint& global_pos);
  QWidget* HeaderWidget() const;
  void DropMimeData(const QMimeData *data, const QModelIndex &index);
  void ReloadSettings();

  bool IsLoggedIn() const;
  void ForgetCredentials();
  //GoogleMusic specific stuff
  void GetSongUrl(const QUrl& url) const;
  QNetworkRequest createRequest(const QString& url);

 public slots:
  void ShowConfig();

 private slots:
  void UpdateTotalSongCount(int count);

  void ReloadDatabase();
  void SongsGot(QVariantList songsJson);
  void ReloadDatabaseFinished(int error);

  void GetAllPlaylists();
  void PlaylistsGot(QVariantList playlistsJson);
  void GetAllPlaylistsFinished(int error);

  void GetSongUrlFinished(QUrl url, int error);

  void CreatePlaylist();
  void PlaylistCreated(QString id, QString name, int error);

  void RenamePlaylist();
  void PlaylistRenamed(QString id, QString newTitle, int error);

  void DeletePlaylist();
  void PlaylistDeleted(QString id, int error);

  void SongsAddedToPlaylist(QString id, QVariantList songJson, int error);
  void PlaylistSongsGot(QString id, QVariantMap playlistJson, int error);

  void DeleteSongsFromPlaylist();
  void SongsDeletedFromPlaylist(QString id, QVariantList songJson, int error);

  void Homepage();

  void LoggedIn(QByteArray auth, QByteArray xt, QByteArray sjsaid);
 private:
  void EnsureMenuCreated();
  void FinishTask(int& task_id) const;

  Song ReadTrack(const QVariantMap& json);
  void PopulatePlaylist(const QVariantMap& json, QStandardItem *playlistItem);
  QStandardItem* GetPlaylistByID(const QString& id);
 private:
  GoogleMusicUrlHandler* url_handler_;
  GoogleMusicClient* client_;

  QMenu* context_menu_;
  QStandardItem* library_item_;
  QStandardItem* playlist_item_;

  QAction* reload_database_action_;
  QAction* reload_playlists_action_;
  QAction* create_playlist_action_;
  QAction* delete_playlist_action_;
  QAction* rename_playlist_action_;
  QAction* delete_song_action_;

  LibraryBackend* library_backend_;
  LibraryModel* library_model_;
  LibraryFilterWidget* library_filter_;
  QSortFilterProxyModel* library_sort_model_;
  int load_database_task_id_;
  int load_playlists_task_id_;

  int total_song_count_;

  QString current_playlist_id_;
  QString current_song_id_;
};

#endif // GOOGLEMUSICSERVICE_H
