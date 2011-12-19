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

#ifndef ICECASTSERVICE_H
#define ICECASTSERVICE_H

#include "icecastbackend.h"
#include "internetservice.h"

#include <QXmlStreamReader>

class IcecastFilterWidget;
class IcecastModel;
class NetworkAccessManager;

class QAction;
class QMenu;

class IcecastService : public InternetService {
  Q_OBJECT
public:
  IcecastService(InternetModel* parent);
  ~IcecastService();

  static const char* kServiceName;
  static const char* kDirectoryUrl;
  static const char* kHomepage;

  enum ItemType {
    Type_Stream = 3000,
    Type_Genre,
  };

  QStandardItem* CreateRootItem();
  void LazyPopulate(QStandardItem* item);

  void ShowContextMenu(const QModelIndex& index, const QPoint& global_pos, const QModelIndexList& selection);

  QWidget* HeaderWidget() const;

protected:
  QModelIndex GetCurrentIndex();

private slots:
  void LoadDirectory();
  void Homepage();
  void DownloadDirectoryFinished();
  void ParseDirectoryFinished();

private:
  void RequestDirectory(const QUrl& url);
  void EnsureMenuCreated();
  IcecastBackend::StationList ParseDirectory(QIODevice* device) const;
  IcecastBackend::Station ReadStation(QXmlStreamReader* reader) const;

  QStandardItem* root_;
  NetworkAccessManager* network_;
  QMenu* context_menu_;
  QModelIndex context_item_;

  IcecastBackend* backend_;
  IcecastModel* model_;
  IcecastFilterWidget* filter_;

  int load_directory_task_id_;
};

#endif
