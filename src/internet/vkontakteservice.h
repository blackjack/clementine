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


class VkontakteService : public InternetService {
  Q_OBJECT

public:
  VkontakteService(InternetModel* parent);
  ~VkontakteService();

  static const char* kServiceName;
  static const char* kSettingsGroup;
  static const char* kHomepage;
  static const char* kApplicationId;
  static const int kStreamsCacheDurationSecs;

  QStandardItem* CreateRootItem();
  void LazyPopulate(QStandardItem* item);
  void ShowContextMenu(const QModelIndex& index, const QPoint& global_pos);

//  PlaylistItem::Options playlistitem_options() const;
  void ReloadSettings();
protected:
  QModelIndex GetCurrentIndex();

private slots:
  void Homepage();
  void ShowConfig();
  void PopulateTracksForUserFinished(QStandardItem* item, QNetworkReply* reply);
  void PopulateFriendsFinished(QStandardItem* item, QNetworkReply* reply);
  void GetAlbumsFinished(QString user_id, QStandardItem *item, QNetworkReply *reply);
private:
  void PopulateTracksForUserAsync(QString user_id, QStandardItem* root);
  void PopulateFriendsAsync(QString user_id, QStandardItem* root);
  void GetAlbumsAsync(QString user_id, QStandardItem* root);
  void Relogin();
private:
//  VkontakteUrlHandler* url_handler_;

  QStandardItem* root_;
  QStandardItem* my_tracks_;
  QStandardItem* friends_;
  QMenu* context_menu_;
  QStandardItem* context_item_;
  QMap<QString,QString> albums_;

  QString access_token_;
  QDateTime expire_date_;
  QString user_id_;
  bool logged_in_;

  QNetworkAccessManager* network_;

};

#endif // VKONTAKTESERVICE_H
