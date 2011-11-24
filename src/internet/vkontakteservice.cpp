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

#include "vkontakteservice.h"
#include "internetmodel.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/player.h"
#include "core/taskmanager.h"
#include "globalsearch/globalsearch.h"
#include "globalsearch/somafmsearchprovider.h"
#include "ui/iconloader.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomElement>
#include <QtDebug>

const char* VkontakteService::kServiceName = "Vkontakte";
const char* VkontakteService::kSettingsGroup = "Vkontakte";
const char* VkontakteService::kHomepage = "http://vkontakte.ru";
const char* VkontakteService::kApplicationId = "2677518";

VkontakteService::VkontakteService(InternetModel* parent)
  : InternetService(kServiceName, parent, parent),
    root_(NULL),
    context_menu_(NULL),
    logged_in_(false),
    network_(new NetworkAccessManager(this))
//    streams_(kSettingsGroup, "streams", kStreamsCacheDurationSecs)
{

//  model()->player()->RegisterUrlHandler(url_handler_);
//  model()->global_search()->AddProvider(new SomaFMSearchProvider(this, this));
}

VkontakteService::~VkontakteService() {
  delete context_menu_;
}

QStandardItem* VkontakteService::CreateRootItem() {
  root_ = new QStandardItem(QIcon(":/providers/vkontakte.png"), kServiceName);
  my_tracks_ = new QStandardItem(QIcon(":last.fm/personal_radio.png"),tr("My own tracks"));
  my_tracks_->setData(true, InternetModel::Role_CanLazyLoad);
  friends_ = new QStandardItem(QIcon(":last.fm/neighbour_radio.png"),tr("My friends' tracks"));
  friends_->setData(true, InternetModel::Role_CanLazyLoad);
  root_->appendRow(my_tracks_);
  root_->appendRow(friends_);
  return root_;
}

void VkontakteService::LazyPopulate(QStandardItem* item) {
  if (!logged_in_) {
    ShowConfig();
    return;
  }
  if (item->text()==tr("My own tracks"))
    GetAlbumsAsync(user_id_,item);
  else if (item->text()==tr("My friends' tracks"))
    PopulateFriendsAsync(user_id_,item);
}

void VkontakteService::ReloadSettings() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  user_id_ = s.value("user_id").toString();
  expire_date_ = s.value("expire_date").toDateTime();
  access_token_ = s.value("access_token").toString();

  logged_in_ = (QDateTime::currentDateTimeUtc()<expire_date_ && !user_id_.isEmpty() );
}

QModelIndex VkontakteService::GetCurrentIndex() {
  return context_item_->index();
}

void VkontakteService::ShowContextMenu(const QModelIndex& index, const QPoint& global_pos) {
  if (!context_menu_) {
    context_menu_ = new QMenu;
    context_menu_->addActions(GetPlaylistActions());
    context_menu_->addAction(IconLoader::Load("download"), tr("Open vkontakte.ru in browser"), this, SLOT(Homepage()));
    context_menu_->addAction(IconLoader::Load("configure"), tr("Configure Vkontakte"), this, SLOT(ShowConfig()));
  }

  context_item_ = model()->itemFromIndex(index);
  context_menu_->popup(global_pos);
}

void VkontakteService::PopulateTracksForUserAsync(QString user_id, QStandardItem *root) {
  QUrl request("https://api.vkontakte.ru/method/audio.get.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("access_token",access_token_);

  QNetworkReply* reply = network_->get(QNetworkRequest(request));

  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(PopulateTracksForUserFinished(QStandardItem*,QNetworkReply*)),
             root, reply);
}

void VkontakteService::PopulateTracksForUserFinished(QStandardItem* item,QNetworkReply* reply) {
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << reply->errorString();
    return;
  }

  if (item->hasChildren())
    item->removeRows(0, item->rowCount());

  QDomDocument doc;
  doc.setContent(reply->readAll());
  QDomElement root = doc.documentElement();
  if (root.tagName()=="response") {
    QDomNodeList tracks = root.elementsByTagName("audio");
    for(int i = 0; i< tracks.size(); ++i) {
      QDomElement info = tracks.at(i).firstChildElement();
      Song song;
      while (!info.isNull()) {
        if (info.tagName() == "artist") {
          song.set_artist(info.text());
        } else if (info.tagName() == "title") {
          song.set_title(info.text());
        } else if (info.tagName() == "url") {
          song.set_url(QUrl(info.text()));
        } else if (info.tagName() == "album") {
          song.set_album(albums_[info.text()]);
        }
        info = info.nextSiblingElement();
      }
      QStandardItem* row = new QStandardItem(song.PrettyTitleWithArtist());
      row->setData(QVariant::fromValue(song), InternetModel::Role_SongMetadata);
      row->setData(InternetModel::PlayBehaviour_SingleItem, InternetModel::Role_PlayBehaviour);
      item->appendRow(row);
    }

  } else if (root.tagName()=="error") {
    QDomElement error_code = root.elementsByTagName("error_code").at(0).toElement();
    int ec = error_code.text().toInt();
    switch (ec) {
    case 4:
    case 5:
      Relogin();
      break;
    default:
      break;
    }
  }
}

void  VkontakteService::PopulateFriendsAsync(QString user_id, QStandardItem* root) {
  QUrl request("https://api.vkontakte.ru/method/friends.get.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("fields","uid,first_name,last_name");
  request.addQueryItem("access_token",access_token_);
  QNetworkReply* reply = network_->get(QNetworkRequest(request));

  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(PopulateFriendsFinished(QStandardItem*,QNetworkReply*)),
             root, reply);
}

void VkontakteService::PopulateFriendsFinished(QStandardItem* item, QNetworkReply* reply) {
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << reply->errorString();
    return;
  }

  if (item->hasChildren())
    item->removeRows(0, item->rowCount());

  QDomDocument doc;
  doc.setContent(reply->readAll());
  QDomElement root = doc.documentElement();
  if (root.tagName()=="response") {
    QDomNodeList tracks = root.elementsByTagName("user");
    for(int i = 0; i< tracks.size(); ++i) {
      QDomElement info = tracks.at(i).firstChildElement();
      QString first_name, last_name, uid;
      while (!info.isNull()) {
        if (info.tagName() == "uid") {
          uid = info.text();
        } else if (info.tagName() == "first_name") {
          first_name = info.text();
        } else if (info.tagName() == "last_name") {
          last_name = info.text();
        }
        info = info.nextSiblingElement();
      }
      QStandardItem* row = new QStandardItem(QIcon(":last.fm/icon_user.png"),first_name+" "+last_name);
      item->appendRow(row);
      GetAlbumsAsync(uid,row);
    }

  } else if (root.tagName()=="error") {
    QDomElement error_code = root.elementsByTagName("error_code").at(0).toElement();
    int ec = error_code.text().toInt();
    switch (ec) {
    case 4:
    case 5:
      Relogin();
      break;
    default:
      break;
    }
  }
}

void VkontakteService::GetAlbumsAsync(QString user_id, QStandardItem* root) {
  QUrl request("https://api.vkontakte.ru/method/audio.getAlbums.xml");
  request.addQueryItem("uid",user_id);
  request.addQueryItem("count","100");
  request.addQueryItem("access_token",access_token_);
  QNetworkReply* reply = network_->get(QNetworkRequest(request));

  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(GetAlbumsFinished(QString,QStandardItem*,QNetworkReply*)),
             user_id, root, reply);
}

void VkontakteService::GetAlbumsFinished(QString user_id, QStandardItem *item, QNetworkReply *reply) {
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << reply->errorString();
    return;
  }

  QDomDocument doc;
  doc.setContent(reply->readAll());
  QDomElement root = doc.documentElement();
  if (root.tagName()=="response") {
    QDomNodeList tracks = root.elementsByTagName("album");
    for(int i = 0; i< tracks.size(); ++i) {
      QDomElement info = tracks.at(i).firstChildElement();
      QString album_id,title;
      while (!info.isNull()) {
        if (info.tagName() == "album_id") {
          album_id=info.text();
        } else if (info.tagName() == "title") {
          title = info.text();
        }
        info = info.nextSiblingElement();
      }
      albums_.insert(album_id,title);
      PopulateTracksForUserAsync(user_id,item);
    }
  } else if (root.tagName()=="error") {
    QDomElement error_code = root.elementsByTagName("error_code").at(0).toElement();
    int ec = error_code.text().toInt();
    switch (ec) {
    case 4:
    case 5:
      Relogin();
      break;
    default:
      break;
    }
  }
}

void VkontakteService::Homepage() {
  QDesktopServices::openUrl(QUrl(kHomepage));
}

void VkontakteService::ShowConfig() {
  emit OpenSettingsAtPage(SettingsDialog::Page_Vkontakte);
}
void VkontakteService::Relogin() {
  access_token_.clear();
  expire_date_ = QDateTime();
  user_id_.clear();
  logged_in_ = false;

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.remove("user_id");
  s.remove("expire_date");
  s.remove("access_token");

  ShowConfig();
}
