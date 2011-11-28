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
#include "core/cachedlist.h"

//class VkontakteUrlHandler;

class QNetworkAccessManager;
class QMenu;
class Playlist;

class VkontakteService : public InternetService {
  Q_OBJECT

public:
  VkontakteService(InternetModel* parent);
  ~VkontakteService();

  static const char* kServiceName;
  static const char* kSettingsGroup;
  static const char* kHomepage;
  static const char* kApplicationId;
  static const int kMaxAlbumsPerRequest;
  static const int kMaxSearchResults;
  static const int kSearchDelayMsec;


  QStandardItem* CreateRootItem();
  void LazyPopulate(QStandardItem* item);
  void ShowContextMenu(const QModelIndex& index, const QPoint& global_pos);
  void ItemDoubleClicked(QStandardItem* item);

//  PlaylistItem::Options playlistitem_options() const;
  void ReloadSettings();

  void Search(const QString& text, Playlist* playlist, bool now = false);
  void GetFullNameAsync(const QString& user_id);
signals:
  void FullNameReceived(const QString& user_id, const QString& full_name);
protected:
  QModelIndex GetCurrentIndex();

private slots:
  void Homepage();
  void ShowConfig();
  void ReloadItems();
  void PopulateTracksForUserFinished(QStandardItem* item, QNetworkReply* reply);
  void PopulateFriendsFinished(QStandardItem* item, QNetworkReply* reply);
  void GetAlbumsFinished(const QString& user_id, QStandardItem *item, QNetworkReply *reply);
  void GetFullNameFinished(const QString& user_id, QNetworkReply* reply);

  void DoSearch();
  void SearchFinished(QNetworkReply* reply);

private:
  void PopulateTracksForUserAsync(const QString& user_id, QStandardItem* root);
  void PopulateFriendsAsync(const QString& user_id, QStandardItem* root);
  void GetAlbumsAsync(const QString& user_id, QStandardItem* root);
  void SearchAsync(const QString& text, int offset = 0);

  void Relogin();
  void HandleApiError(int error);
private:
//  VkontakteUrlHandler* url_handler_;

  QStandardItem* root_;
  QStandardItem* my_tracks_;
  QStandardItem* friends_;
  QStandardItem* search_;
  QMenu* context_menu_;
  QStandardItem* context_item_;
  QMap<QString,QString> albums_;

  QString access_token_;
  QDateTime expire_date_;
  QString user_id_;
  bool logged_in_;

  QTimer* search_delay_;
  QString pending_search_;
  Playlist* pending_search_playlist_;

  QNetworkAccessManager* network_;
};

#endif // VKONTAKTESERVICE_H
