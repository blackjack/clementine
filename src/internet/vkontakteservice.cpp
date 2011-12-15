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
#include "internetview.h"
#include "playlist/playlistview.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/player.h"
#include "core/throttlednetworkmanager.h"
#include "playlist/playlistmanager.h"
#include "core/taskmanager.h"
#include "playlist/playlist.h"
#include "ui/iconloader.h"
#include "vkontaktesearchplaylisttype.h"
#include "vkontakteworker.h"

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QMenu>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomElement>
#include <QTimer>
#include <QMimeData>
#include <QMetaType>
#include <QtDebug>


const char* VkontakteService::kServiceName = "Vkontakte";
const char* VkontakteService::kSettingsGroup = "Vkontakte";
const char* VkontakteService::kHomepage = "http://vkontakte.ru";
const char* VkontakteService::kApplicationId = "2677518";
const int VkontakteService::kMaxSearchResults = 200;
const int VkontakteService::kMaxAlbumsPerRequest = 100;
const int VkontakteService::kMaxRequestsPerSecond = 3;
const int VkontakteService::kSearchDelayMsec = 400;
const char* VkontakteService::kCommentInfoRegexp = "song_id\\=(.*)\\|owner_id\\=(.*)\\|album_id\\=(.*)";


VkontakteService::VkontakteService(InternetModel* parent)
  : InternetService(kServiceName, parent, parent),
    root_(NULL),
    context_menu_(NULL),
    logged_in_(false),
    network_(new ThrottledNetworkManager(kMaxRequestsPerSecond,this))
{

  qRegisterMetaType< QList<QStringList> >("QList<QStringList>");

  model()->player()->playlists()->RegisterSpecialPlaylistType(
        new VkontakteSearchPlaylistType(this));

  add_to_my_tracks_playlist_ = new QAction(QIcon(":/providers/vkontakte.png"),tr("Add to my tracks"), this);
  connect(add_to_my_tracks_playlist_,SIGNAL(triggered()),SLOT(AddToMyTracks()));

  QAction* separator = new QAction(this);
  separator->setSeparator(true);
  playlistitem_actions_ << separator << add_to_my_tracks_playlist_ << separator;

}

VkontakteService::~VkontakteService() {
  delete context_menu_;
}

QStandardItem* VkontakteService::CreateRootItem() {
  root_ = new QStandardItem(QIcon(":/providers/vkontakte.png"), kServiceName);
  root_->setData(false, InternetModel::Role_CanLazyLoad);  

  my_tracks_ = new QStandardItem(QIcon(":last.fm/personal_radio.png"),tr("My own tracks"));
  my_tracks_->setData(true, InternetModel::Role_CanLazyLoad);
  my_tracks_->setFlags(my_tracks_->flags() | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
  my_tracks_->setData(true, InternetModel::Role_CanBeModified);
  my_tracks_->setData(Type_MyTracks,InternetModel::Role_Type);

  friends_ = new QStandardItem(QIcon(":last.fm/neighbour_radio.png"),tr("My friends' tracks"));
  friends_->setData(true, InternetModel::Role_CanLazyLoad);
  friends_->setData(Type_Friends,InternetModel::Role_Type);

  search_ = new QStandardItem(IconLoader::Load("edit-find"),
                              tr("Search Vkontakte (opens a new tab)"));
  search_->setData(InternetModel::PlayBehaviour_DoubleClickAction,
                           InternetModel::Role_PlayBehaviour);
  search_->setData(Type_Search,InternetModel::Role_Type);

  root_->appendRow(my_tracks_);
  root_->appendRow(friends_);
  root_->appendRow(search_);
  return root_;
}

void VkontakteService::LazyPopulate(QStandardItem* item) {
  if (!logged_in_) {
    item->setData(true, InternetModel::Role_CanLazyLoad);
    ShowConfig();
    return;
  }
  if (item->hasChildren())
    item->removeRows(0, item->rowCount());

  VkontakteApiWorker* w = new VkontakteApiWorker(this);
  w->SetAccessToken(access_token_);
  w->SetNetwork(network_);

  connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
  connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
  connect(w,SIGNAL(Error(QString)),w,SLOT(deleteLater()));

  if (item == my_tracks_ || item->parent() == friends_) {
    QString user_id = item == my_tracks_ ? user_id_ : item->data(Role_UserID).toString();
    w->GetAlbumsAsync(user_id);
    connect(w,SIGNAL(AlbumsGot(QList<QStringList>)),this,SLOT(InsertAlbumItems(QList<QStringList>)));
    NewClosure(w,SIGNAL(AlbumsGot(QList<QStringList>)),w,SLOT(GetTracksAsync(QString)),user_id);
    connect(w,SIGNAL(TracksGot(SongList)),SLOT(InsertTrackItems(SongList)));
    connect(w,SIGNAL(TracksGot(SongList)),w,SLOT(deleteLater()));
  }
  else if (item == friends_) {
    w->GetFriendsAsync(user_id_);
    connect(w,SIGNAL(FriendsGot(QList<QStringList>)),this,SLOT(InsertFriendItems(QList<QStringList>)));
    connect(w,SIGNAL(FriendsGot(QList<QStringList>)),w,SLOT(deleteLater()));
  }
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
    context_menu_->addSeparator();
    add_to_my_tracks_model_= context_menu_->addAction(IconLoader::Load("list-add"),
                                                      tr("Add to my tracks"),
                                                      this, SLOT(AddToMyTracks()));
    remove_from_my_tracks_model_ = context_menu_->addAction(IconLoader::Load("list-remove"),
                                                            tr("Remove from my tracks"),
                                                            this, SLOT(RemoveFromMyTracks()));
    create_album_ = context_menu_->addAction(IconLoader::Load("document-new"),
                                             tr("Create new album"),
                                             this,SLOT(CreateAlbum()));
    delete_album_ = context_menu_->addAction(IconLoader::Load("edit-delete"),
                                             tr("Delete album"),
                                             this,SLOT(DeleteAlbum()));
    rename_album_ = context_menu_->addAction(IconLoader::Load("edit-rename"),
                                             tr("Rename album"),
                                             this,SLOT(RenameAlbum()));

    context_menu_->addSeparator();
    context_menu_->addAction(IconLoader::Load("download"), tr("Open vkontakte.ru in browser"), this, SLOT(Homepage()));
    context_menu_->addAction(IconLoader::Load("view-refresh"), tr("Refresh catalogue"), this, SLOT(ReloadItems()));
    context_menu_->addAction(IconLoader::Load("configure"), tr("Configure Vkontakte"), this, SLOT(ShowConfig()));
  }
  context_item_ = model()->itemFromIndex(index);

  QRegExp rx(kCommentInfoRegexp);

  InternetView* view = qobject_cast<InternetView*>(qApp->widgetAt(global_pos)->parentWidget());
  if (view) {
    urls_to_add_.clear();
    urls_to_remove_.clear();

    foreach (QModelIndex i, view->selectionModel()->selectedIndexes()) {
      QUrl url = i.data(InternetModel::Role_Url).toUrl();
      if (!url.isValid())
        continue;
      if (url.hasFragment() &&
          rx.indexIn(url.fragment()) >-1 ) {
        QString owner_id = rx.cap(2);
        if (owner_id==user_id_)
          urls_to_remove_ << url;
        else
          urls_to_add_ << url;
      }
    }
  }

  add_to_my_tracks_model_->setVisible(!urls_to_add_.isEmpty());
  remove_from_my_tracks_model_->setVisible(!urls_to_remove_.isEmpty());

  create_album_->setVisible( context_item_ == my_tracks_ );
  delete_album_->setVisible( context_item_->data(Role_AlbumID).isValid() );
  rename_album_->setVisible( context_item_->data(Role_AlbumID).isValid() );

  context_menu_->popup(global_pos);
}


QList<QAction*> VkontakteService::playlistitem_actions(const Song& song) {

    urls_to_add_.clear();
    QRegExp rx(kCommentInfoRegexp);
    QUrl url = song.url();
    if (url.isValid() && url.hasFragment() && rx.indexIn(url.fragment()) >-1 ) {
      urls_to_add_ << url;
      add_to_my_tracks_playlist_->setVisible(true);
    }
    else {
      add_to_my_tracks_playlist_->setVisible(false);
    }

    return playlistitem_actions_;
}


void VkontakteService::GetFullNameAsync(const QString &user_id, const QString& access_token) {
  BgApiWorker* worker = new BgApiWorker(this);
  worker->Start(true);
  VkontakteApiWorker* w = worker->Worker().get();
  w->SetAccessToken(access_token);
  w->SetNetwork(network_);

  connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
  connect(w,SIGNAL(FullNameGot(QString)),SIGNAL(FullNameReceived(QString)));
  connect(w,SIGNAL(FullNameGot(QString)),worker,SLOT(quit()));
  connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
  connect(w,SIGNAL(Error(QString)),worker,SLOT(quit()));

  w->GetFullNameAsync(user_id);
}


void VkontakteService::ReloadItems() {
  if (!logged_in_) {
    ShowConfig();
    return;
  }
  QStandardItem* item = context_item_;
  while (item) {
    if (item==my_tracks_ || item==friends_) {
      item->setData(false, InternetModel::Role_CanLazyLoad);
      LazyPopulate(item);
      return;
    } else if (item==root_) {
      if (!friends_->data(InternetModel::Role_CanLazyLoad).toBool())
        LazyPopulate(friends_);
      if (!my_tracks_->data(InternetModel::Role_CanLazyLoad).toBool())
        LazyPopulate(my_tracks_);
      return;
    } else if (item->parent() == friends_) {
      if (!item->data(InternetModel::Role_CanLazyLoad).toBool())
        LazyPopulate(item);
    }

    item = item->parent();
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

void VkontakteService::Search(const QString& text, Playlist* playlist, bool now) {
  if (!logged_in_) {
    ShowConfig();
    return;
  }

  BgSearchWorker* worker;
  if (!search_workers_.contains(playlist)) {
    worker = new BgSearchWorker(this);
    search_workers_.insert(playlist,worker);
    connect(playlist,SIGNAL(destroyed(QObject*)),SLOT(PlaylistDestroyed(QObject*)));

    worker->Start(true);
    VkontakteSearchWorker* w = worker->Worker().get();

    w->SetAccessToken(access_token_);
    w->SetNetwork(network_);

    connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
    connect(w,SIGNAL(Error(QString)),worker,SLOT(quit()));
    connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
    connect(w,SIGNAL(SearchResultsGot(SongList)),SLOT(SearchResultsGot(SongList)));
  } else {
    worker = search_workers_[playlist];
  }

  worker->Worker()->Search(text,now);
}

void VkontakteService::PlaylistDestroyed(QObject* object) {
  Playlist* playlist = static_cast<Playlist*>(object);
//use static cast instead of qobject_cast because object is not Playlist anymore
  if (search_workers_.contains(playlist)) {
    search_workers_[playlist]->quit();
    search_workers_.remove(playlist);
  }
}

void VkontakteService::SearchWorkerFinished() {
  BgSearchWorker* worker = static_cast<BgSearchWorker*>(sender());
  if (search_workers_.values().contains(worker)) {
    search_workers_.remove(search_workers_.key(worker));
  }
}

void VkontakteService::SearchResultsGot(const SongList& songs) {
  VkontakteSearchWorker* w = qobject_cast<VkontakteSearchWorker*>(sender());
  for (QMap<Playlist*,BgSearchWorker*>::iterator it=search_workers_.begin(); it!=search_workers_.end(); ++it) {
    if (it.value()->Worker().get() == w) {
      Playlist* playlist = it.key();
      playlist->Clear();
      playlist->InsertInternetItems(this,songs);
    }
  }
}


void VkontakteService::ItemDoubleClicked(QStandardItem* item) {
  if (item == search_)
    model()->player()->playlists()->New(tr("Search Vkontakte"), SongList(),
                                        VkontakteSearchPlaylistType::kName);
}

void VkontakteService::CreateAlbum() {
  if (context_item_!=my_tracks_) {
    return;
  }
  bool ok;
  QString title = QInputDialog::getText(NULL,
                                        tr("Create album"),
                                        tr("Album's title:"),
                                        QLineEdit::Normal,
                                        QString(),
                                        &ok);

  if (!ok || title.isEmpty()) {
    return;
  }
  VkontakteApiWorker* w = new VkontakteApiWorker(this);
  w->SetAccessToken(access_token_);
  w->SetNetwork(network_);
  connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
  connect(w,SIGNAL(Error(QString)),w,SLOT(deleteLater()));
  connect(w,SIGNAL(AlbumCreated(QString)),this,SLOT(ReloadItems()));
  connect(w,SIGNAL(AlbumCreated(QString)),w,SLOT(deleteLater()));
  w->CreateAlbumAsync(title);
}

void VkontakteService::RenameAlbum() {
  if (context_item_->data(Role_AlbumID).isValid()) {
    QString album_id = context_item_->data(Role_AlbumID).toString();
    bool ok;
    QString title = QInputDialog::getText(NULL,
                                          tr("Rename album"),
                                          tr("New title:"),
                                          QLineEdit::Normal,
                                          context_item_->text(),
                                          &ok);
    if (!ok || title.isEmpty()) {
      return;
    }
    VkontakteApiWorker* w = new VkontakteApiWorker(this);
    w->SetAccessToken(access_token_);
    w->SetNetwork(network_);
    connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
    connect(w,SIGNAL(Error(QString)),w,SLOT(deleteLater()));
    connect(w,SIGNAL(AlbumNameEdited()),this,SLOT(ReloadItems()));
    connect(w,SIGNAL(AlbumNameEdited()),w,SLOT(deleteLater()));
    w->EditAlbumNameAsync(album_id,title);
  }
}

void VkontakteService::DeleteAlbum() {
  if (context_item_->data(Role_AlbumID).isValid()) {
    QString album_id = context_item_->data(Role_AlbumID).toString();
    if (QMessageBox::question(NULL, tr("Delete album"),
                              tr("This action will delete this album (all songs will remain), are you sure you want to continue?"),
                              QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
      return;
    VkontakteApiWorker* w = new VkontakteApiWorker(this);
    w->SetAccessToken(access_token_);
    w->SetNetwork(network_);
    connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
    connect(w,SIGNAL(Error(QString)),w,SLOT(deleteLater()));
    connect(w,SIGNAL(AlbumDeleted()),this,SLOT(ReloadItems()));
    connect(w,SIGNAL(AlbumDeleted()),w,SLOT(deleteLater()));
    w->DeleteAlbumAsync(album_id);
  }
}

void VkontakteService::AddToMyTracks() {
  if (urls_to_add_.isEmpty())
    return;

  BgWorker* worker = new BgWorker(this);
  worker->Start(true);
  VkontakteWorker* w = worker->Worker().get();
  w->SetAccessToken(access_token_);
  w->SetNetwork(network_);
  connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
  connect(w,SIGNAL(Error(QString)),worker,SLOT(quit()));
  connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));
  connect(w,SIGNAL(MultiAddedToMyTracks()),this,SLOT(RepopulateMyTracks()));
  connect(w,SIGNAL(MultiAddedToMyTracks()),worker,SLOT(quit()));

  w->MultiAddToMyTracks(urls_to_add_,user_id_);
}


void VkontakteService::RemoveFromMyTracks() {
  if (urls_to_remove_.isEmpty())
    return;

  BgWorker* worker = new BgWorker(this);
  worker->Start(true);
  VkontakteWorker* w = worker->Worker().get();
  w->SetAccessToken(access_token_);
  w->SetNetwork(network_);
  connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
  connect(w,SIGNAL(Error(QString)),worker,SLOT(quit()));
  connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));

  connect(w,SIGNAL(MultiRemovedFromMyTracks()),this,SLOT(RepopulateMyTracks()));
  connect(w,SIGNAL(MultiRemovedFromMyTracks()),worker,SLOT(quit()));

  w->MultiRemoveFromMyTracks(urls_to_remove_,user_id_);
}


QStandardItem* VkontakteService::FindChild(QStandardItem *parent, const QString &text) {
  for (int i = 0; i< parent->rowCount(); ++i) {
    QStandardItem* child = parent->child(i);
    if (child->text()==text)
      return child;
  }
  return 0;
}

void VkontakteService::DropMimeData(const QMimeData *data, const QModelIndex &index) {
  QList<QUrl> songs = data->urls();
  QString album_id = index.data(Role_AlbumID).toString(); //if empty - delete from album

  if (songs.isEmpty())
    return;

  BgWorker* worker = new BgWorker(this);
  worker->Start(true);
  VkontakteWorker* w = worker->Worker().get();
  w->SetAccessToken(access_token_);
  w->SetNetwork(network_);
  connect(w,SIGNAL(AuthError()),SLOT(Relogin()));
  connect(w,SIGNAL(Error(QString)),worker,SLOT(quit()));
    connect(w,SIGNAL(Error(QString)),SLOT(Log(QString)));

  connect(w,SIGNAL(MultiMovedToAlbum()),this,SLOT(RepopulateMyTracks()));
  connect(w,SIGNAL(MultiMovedToAlbum()),worker,SLOT(quit()));

  w->MultiMoveToAlbum(songs,user_id_,album_id);
}

void VkontakteService::InsertFriendItems(const QList<QStringList> &list_of_user_full_name) {
  for (QList<QStringList>::const_iterator it = list_of_user_full_name.begin();
       it!=list_of_user_full_name.end(); ++it) {
    if (it->size()<2) continue;
    QString user_id = it->at(0);
    QString full_name = it->at(1);

    QStandardItem* user = new QStandardItem(QIcon(":last.fm/icon_user.png"),full_name);
    user->setData(user_id,Role_UserID);
    user->setData(true, InternetModel::Role_CanLazyLoad);
    user->setData(InternetModel::Type_Service,InternetModel::Role_Type);
    friends_->appendRow(user);
  }
}

void VkontakteService::InsertAlbumItems(const QList<QStringList>& list_of_owner_album_title) {
  QStandardItem* parent = NULL;
  QString old_owner;

  for (QList<QStringList>::const_iterator it = list_of_owner_album_title.begin();
       it!=list_of_owner_album_title.end(); ++it) {

    if (it->size() < 3) continue;
    QString owner_id = it->at(0), album_id = it->at(1), title = it->at(2);

    if (old_owner!=owner_id) {
      if (owner_id == user_id_)
        parent = my_tracks_;
      else {
        for (int i = 0; i<friends_->rowCount(); ++i) {
          QStandardItem* child = friends_->child(i);
          if (child->data(Role_UserID).toString() == owner_id)
            parent = child;
        }
      }
    }
    if (!parent) continue;

    QStandardItem* album = new QStandardItem(IconLoader::Load("x-clementine-album"),title);
    album->setData(album_id,VkontakteService::Role_AlbumID);
    album->setData(InternetModel::Type_UserPlaylist,InternetModel::Role_Type);
    album->setData(InternetModel::PlayBehaviour_UseSongLoader,InternetModel::Role_PlayBehaviour);
    album->setData(true, InternetModel::Role_CanBeModified);
    parent->appendRow(album);

    old_owner = owner_id;
  }
}

void VkontakteService::InsertTrackItems(const SongList& songs) {
  QStandardItem *parent = NULL, *user = NULL, *album = NULL;
  QString old_owner, old_album;
  QRegExp rx(kCommentInfoRegexp);

  for (SongList::const_iterator it = songs.begin(); it!=songs.end(); ++it) {
    Song song = *it;
    if (!song.url().hasFragment() || rx.indexIn(song.url().fragment()) ==-1 )
      continue;

    QString owner_id = rx.cap(2), album_id = rx.cap(3);

    if (old_owner!=owner_id) {
      if (owner_id == user_id_)
        user = my_tracks_;
      else {
        for (int i = 0; i<friends_->rowCount(); ++i) {
          QStandardItem* child = friends_->child(i);
          if (child->data(Role_UserID).toString() == owner_id)
            user = child;
        }
      }
      if (!user) continue;
    }
    if (album_id.isEmpty()) {
      parent = user;
    } else {
      if (old_album!=album_id) {
        for (int i = 0; i<user->rowCount(); ++i) {
          QStandardItem* child = user->child(i);
          if (child->data(Role_AlbumID).toString() == album_id)
            album = child;
        }
        if (!album) continue;
        song.set_album(album->text());
      }
      parent = album;
    }

    QStandardItem* song_item = new QStandardItem(song.PrettyTitleWithArtist());
    song_item->setData(InternetModel::PlayBehaviour_SingleItem, InternetModel::Role_PlayBehaviour);
    song_item->setData(song.url(),InternetModel::Role_Url);
    song_item->setData(VkontakteService::Type_Song,InternetModel::Role_Type);
    song_item->setData(QVariant::fromValue(song), InternetModel::Role_SongMetadata);
    parent->appendRow(song_item);
  }
}

void VkontakteService::RepopulateMyTracks() {
  LazyPopulate(my_tracks_);
}
 void VkontakteService::Log(QString error) {
   qLog(Warning) << error;
 }
