/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VKONTAKTESERVICE_H
#define VKONTAKTESERVICE_H

#include "internetservice.h"
#include "internetmodel.h"
#include "core/backgroundthread.h"

class ThrottledNetworkManager;
class QMenu;
class Playlist;
class QDomElement;
class Song;
class NetworkReply;
class VkontakteApiWorker;
class VkontakteSearchWorker;
class VkontakteWorker;

typedef BackgroundThreadImplementation<VkontakteApiWorker,VkontakteApiWorker> BgApiWorker;
typedef BackgroundThreadImplementation<VkontakteSearchWorker,VkontakteSearchWorker> BgSearchWorker;
typedef BackgroundThreadImplementation<VkontakteWorker,VkontakteWorker> BgWorker;

class VkontakteService : public InternetService {
  Q_OBJECT

public:

  enum Role {
    Role_AlbumID = InternetModel::RoleCount+1,
    Role_UserID,
  };
  enum Type {
    Type_MyTracks = InternetModel::TypeCount+1,
    Type_Friends,
    Type_Friend,
    Type_Album,
    Type_Song,
    Type_Search,
  };

  static const char* kServiceName;
  static const char* kSettingsGroup;
  static const char* kHomepage;
  static const char* kApplicationId;
  static const int kMaxAlbumsPerRequest;
  static const int kMaxSearchResults;
  static const int kMaxRequestsPerSecond;
  static const int kSearchDelayMsec;
  static const char* kCommentInfoRegexp;


  VkontakteService(InternetModel* parent);
  ~VkontakteService();

  QStandardItem* CreateRootItem();
  void LazyPopulate(QStandardItem* item);
  void ShowContextMenu(const QModelIndex& index, const QPoint& global_pos);
  void ItemDoubleClicked(QStandardItem* item);
  QList<QAction*> playlistitem_actions(const Song& song);
  void DropMimeData(const QMimeData* data, const QModelIndex& index);
  void ReloadSettings();

  void Search(const QString& text, Playlist* playlist, bool now = false);
  void GetFullNameAsync(const QString& user_id, const QString& access_token);
signals:
  void FullNameReceived(const QString& full_name);

protected:
  QModelIndex GetCurrentIndex();

private slots:
  void Homepage();
  void ShowConfig();
  void ReloadItems();
  void Relogin();
  void Log(QString error);

  void CreateAlbum();
  void RenameAlbum();
  void DeleteAlbum();

  void AddToMyTracks();
  void RemoveFromMyTracks();

  void SearchResultsGot(const SongList& songs);
  void PlaylistDestroyed(QObject* object);
  void SearchWorkerFinished();

  void RepopulateMyTracks();

  void InsertFriendItems(const QList<QStringList>& list_of_user_full_name);
  void InsertAlbumItems(const QList<QStringList>& list_of_owner_album_title);
  void InsertTrackItems(const SongList& songs);
private:
  QStandardItem* FindChild(QStandardItem* parent, const QString& text);

private:
  QStandardItem* root_;
  QStandardItem* my_tracks_;
  QStandardItem* friends_;
  QStandardItem* search_;
  QMenu* context_menu_;

  QAction* create_album_;
  QAction* delete_album_;
  QAction* rename_album_;
  QAction* add_to_my_tracks_model_;
  QAction* remove_from_my_tracks_model_;
  QAction* add_to_my_tracks_playlist_;
  QList<QUrl> urls_to_add_;
  QList<QUrl> urls_to_remove_;

  QMap<Playlist*,BgSearchWorker*> search_workers_;
  QList<QAction*> playlistitem_actions_;
  QStandardItem* context_item_;
  Song context_song_;

  QString access_token_;
  QDateTime expire_date_;
  QString user_id_;
  bool logged_in_;

  ThrottledNetworkManager* network_;
};

#endif // VKONTAKTESERVICE_H
