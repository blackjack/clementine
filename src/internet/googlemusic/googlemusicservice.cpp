#include "googlemusicservice.h"
#include "googlemusicurlhandler.h"
#include "googlemusicclient.h"
#include <globalsearch/googlemusicsearchprovider.h>

#include "core/application.h"
#include "core/database.h"
#include "core/mergedproxymodel.h"
#include "core/player.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "globalsearch/globalsearch.h"
#include "library/librarymodel.h"
#include "library/librarybackend.h"
#include "library/libraryfilterwidget.h"
#include "ui/iconloader.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QDesktopServices>
#include <QSettings>
#include <QNetworkCookieJar>
#include <QInputDialog>
#include <QMessageBox>

const char* GoogleMusicService::kServiceName = "Google Music";
const char* GoogleMusicService::kSettingsGroup = "GoogleMusic";
const char* GoogleMusicService::kSongsTable = "googlemusic_songs";
const char* GoogleMusicService::kFtsTable = "googlemusic_songs_fts";
const char* GoogleMusicService::kHomepage = "http://music.google.com";

GoogleMusicService::GoogleMusicService(Application* app, InternetModel* parent)
  : InternetService(kServiceName, app, parent, parent),
    url_handler_(new GoogleMusicUrlHandler(app, this)),
    client_(new GoogleMusicClient(this)),
    context_menu_(NULL),
    library_item_(NULL),
    playlist_item_(NULL),
    library_backend_(NULL),
    library_model_(NULL),
    library_filter_(NULL),
    library_sort_model_(new QSortFilterProxyModel(this)),
    load_database_task_id_(0),
    load_playlists_task_id_(0),
    total_song_count_(0)
{
  connect(client_,
          SIGNAL(TracksGot(QVariantList)),
          SLOT(SongsGot(QVariantList)));
  connect(client_,
          SIGNAL(TracksLoaded(int)),
          SLOT(ReloadDatabaseFinished(int)));
  connect(client_,
          SIGNAL(PlaylistsGot(QVariantList)),
          SLOT(PlaylistsGot(QVariantList)));
  connect(client_,
          SIGNAL(PlaylistsLoaded(int)),
          SLOT(GetAllPlaylistsFinished(int)));
  connect(client_,
          SIGNAL(SongUrlGot(QUrl,int)),
          SLOT(GetSongUrlFinished(QUrl,int)));

  connect(client_,
          SIGNAL(PlaylistCreated(QString,QString,int)),
          SLOT(PlaylistCreated(QString,QString,int)));
  connect(client_,
          SIGNAL(PlaylistRenamed(QString,QString,int)),
          SLOT(PlaylistRenamed(QString,QString,int)));
  connect(client_,
          SIGNAL(PlaylistDeleted(QString,int)),
          SLOT(PlaylistDeleted(QString,int)));
  connect(client_,
          SIGNAL(SongsAddedToPlaylist(QString,QVariantList,int)),
          SLOT(SongsAddedToPlaylist(QString,QVariantList,int)));
  connect(client_,
          SIGNAL(SongsDeletedFromPlaylist(QString,QVariantList,int)),
          SLOT(SongsDeletedFromPlaylist(QString,QVariantList,int)));
  connect(client_,
          SIGNAL(PlaylistSongsGot(QString,QVariantMap,int)),
          SLOT(PlaylistSongsGot(QString,QVariantMap,int)));

  // Create the library backend in the database thread
  library_backend_ = new LibraryBackend;
  library_backend_->moveToThread(app_->database()->thread());
  library_backend_->Init(app_->database(), kSongsTable,
                         QString::null, QString::null, kFtsTable);
  library_model_ = new LibraryModel(library_backend_, app_, this);

  connect(library_backend_,
          SIGNAL(TotalSongCountUpdated(int)),
          SLOT(UpdateTotalSongCount(int)));

  library_sort_model_->setSourceModel(library_model_);
  library_sort_model_->setSortRole(LibraryModel::Role_SortText);
  library_sort_model_->setDynamicSortFilter(true);
  library_sort_model_->setSortLocaleAware(true);
  library_sort_model_->sort(0);

  app_->player()->RegisterUrlHandler(url_handler_);
  app_->global_search()->AddProvider(new GoogleMusicSearchProvider(this,library_backend_));
}

GoogleMusicService::~GoogleMusicService() {
  delete context_menu_;
}

void GoogleMusicService::ReloadSettings() {
  QSettings s;
  s.beginGroup(kSettingsGroup);

  QByteArray xt = s.value("xt_cookie").toByteArray(),
      sjsaid = s.value("sjsaid_cookie").toByteArray(),
      auth = s.value("auth").toByteArray();

  client_->setAuthInfo(auth,xt,sjsaid);
}

bool GoogleMusicService::IsLoggedIn() const
{
  QSettings s;
  s.beginGroup(kSettingsGroup);
  return s.value("logged_in").toBool();
}

QStandardItem* GoogleMusicService::CreateRootItem() {
  QStandardItem* root = new QStandardItem( IconLoader::Load("googlemusic",IconLoader::IconType::Provider), kServiceName);

  library_item_ = new QStandardItem(IconLoader::Load("media-optical", IconLoader::IconType::Other),tr("Media Library"));
  playlist_item_ = new QStandardItem(IconLoader::Load("view-media-playlist", IconLoader::IconType::Other),tr("Playlists"));
  library_item_->setData(Type_Library,InternetModel::Role_Type);
  playlist_item_->setData(Type_PlaylistList,InternetModel::Role_Type);
  library_item_->setData(true,InternetModel::Role_CanLazyLoad);
  playlist_item_->setData(true,InternetModel::Role_CanLazyLoad);

  library_item_->setData(true,InternetModel::Role_CanLazyLoad);
  root->insertRow(root->rowCount(),playlist_item_);

  root->insertRow(root->rowCount(),library_item_);

  return root;
}

void GoogleMusicService::LazyPopulate(QStandardItem* item) {
  switch (item->data(InternetModel::Role_Type).toInt()) {
    case Type_Library:
      library_model_->Init();
      if (total_song_count_ == 0 && !load_database_task_id_) {
        ReloadDatabase();
      }
      model()->merged_model()->AddSubModel(item->index(), library_sort_model_);
      break;
    case Type_PlaylistList:
      if (playlist_item_->rowCount()==0 && !load_playlists_task_id_) {
        GetAllPlaylists();
      }
      break;
  }
}

void GoogleMusicService::UpdateTotalSongCount(int count) {
  total_song_count_ = count;
}

void GoogleMusicService::ReloadDatabase() {
  if (!load_database_task_id_) {
    library_backend_->DeleteAll();
    client_->GetAllSongs();
    load_database_task_id_ = app_->task_manager()->StartTask(tr("Downloading Google Music catalogue"));
  }
}

void GoogleMusicService::SongsGot(QVariantList songsJson)
{
  SongList songs;
  foreach ( const QVariant& item, songsJson ) {
    songs << ReadTrack(item.toMap());
  }
  library_backend_->AddOrUpdateSongs(songs);
}

void GoogleMusicService::ReloadDatabaseFinished(int error) {
  FinishTask(load_database_task_id_);
  library_model_->Reset();
  if (error) {
    emit StreamError(tr("Failed to download Google Music catalogue"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
  }
}

void GoogleMusicService::GetAllPlaylists()
{
  if (!load_playlists_task_id_) {
    playlist_item_->removeRows(0,playlist_item_->rowCount()); //clear item
    client_->GetAllPlaylists();
    load_playlists_task_id_ = app_->task_manager()->StartTask(tr("Downloading Google Music playlists"));
  }
}

void GoogleMusicService::PlaylistsGot(QVariantList playlistsJson)
{
  foreach (QVariant item, playlistsJson) {
    QVariantMap map = item.toMap();
    QString name = map["title"].toString();
    QStandardItem* playlist = new QStandardItem(IconLoader::Load("view-media-playlist", IconLoader::IconType::Other),name);
    playlist->setData(map["playlistId"],Role_PlaylistID);
    playlist->setData(InternetModel::PlayBehaviour_MultipleItems,InternetModel::Role_PlayBehaviour);
    playlist->setData(true,InternetModel::Role_CanBeModified);
    playlist_item_->insertRow(playlist_item_->rowCount(),playlist);
    PopulatePlaylist(map,playlist);
  }
}

void GoogleMusicService::GetAllPlaylistsFinished(int error)
{
  FinishTask(load_playlists_task_id_);
  if (error) {
    emit StreamError(tr("Failed to download Google Music playlists"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
  }
}


Song GoogleMusicService::ReadTrack(const QVariantMap &json) {
  Song song;

  song.set_album(json["album"].toString());

  QVariantList art_list = json["albumArtRef"].toList();
  if (!art_list.empty()) {
    song.set_art_automatic(art_list.first().toString());
  }

  song.set_albumartist(json["albumArtist"].toString());
  song.set_artist(json["artist"].toString());
  song.set_bpm(json["beatsPerMinute"].toInt());
  song.set_comment(json["comment"].toString());
  song.set_composer(json["composer"].toString());
  song.set_ctime(json["creationTimestamp"].toLongLong()/1e6);
  song.set_disc(json["discNumber"].toInt());
  song.set_length_nanosec(json["durationMillis"].toLongLong()*1e6);
  song.set_filesize(json["estimatedSize"].toInt());
  song.set_genre(json["genre"].toString());
  song.set_lastplayed(json["lastPlayed"].toLongLong()/1e6);
  song.set_mtime(json["lastModifiedTimestamp"].toLongLong()/1e6);
  song.set_playcount(json["playCount"].toInt());
  song.set_rating(json["rating"].toFloat());
  song.set_title(json["title"].toString());
  song.set_track(json["trackNumber"].toInt());
  song.set_url(QUrl("googlemusic://"+json["id"].toString()));
  song.set_year(json["year"].toInt());
  song.set_filetype(Song::Type_Stream);
  // We need to set these to satisfy the database constraints
  song.set_directory_id(0);
  song.set_mtime(0);
  song.set_filesize(0);
  song.set_valid(true);
  return song;
}

void GoogleMusicService::PopulatePlaylist(const QVariantMap &json, QStandardItem* playlistItem)
{
  foreach (QVariant songJson, json["playlist"].toList()) {
    QVariantMap map = songJson.toMap();
    Song s = ReadTrack(map);
    QStandardItem* row = CreateSongItem(s);
    row->setData(map["playlistEntryId"],Role_PlaylistEntryID);
    playlistItem->insertRow(playlistItem->rowCount(),row);
  }
}

QStandardItem *GoogleMusicService::GetPlaylistByID(const QString &id)
{
  for (int i=0; i<playlist_item_->rowCount();++i) {
    QStandardItem* child = playlist_item_->child(i);
    QVariant item_id = child->data(Role_PlaylistID);
    if (item_id.isValid() && item_id.toString() == id)
      return child;
  }
  return NULL;
}

void GoogleMusicService::EnsureMenuCreated() {
  if (context_menu_)
    return;

  context_menu_ = new QMenu;

  context_menu_->addActions(GetPlaylistActions());
  context_menu_->addSeparator();
  context_menu_->addAction(IconLoader::Load("download", IconLoader::IconType::Other),
                           tr("Open %1 in browser").arg("Google Music player"),
                           this, SLOT(Homepage()));
  reload_database_action_ = context_menu_->addAction(IconLoader::Load("view-refresh", IconLoader::IconType::Other),
                                                     tr("Refresh catalogue"),
                                                     this, SLOT(ReloadDatabase()));

  reload_playlists_action_ = context_menu_->addAction(IconLoader::Load("view-refresh", IconLoader::IconType::Other),
                                                      tr("Refresh playlists"),
                                                      this, SLOT(GetAllPlaylists()));

  create_playlist_action_ = context_menu_->addAction(IconLoader::Load("list-add", IconLoader::IconType::Other),
                                                     tr("Create playlist"),
                                                     this, SLOT(CreatePlaylist()));

  rename_playlist_action_ = context_menu_->addAction(IconLoader::Load("edit-rename", IconLoader::IconType::Other),
                                                     tr("Rename playlist"),
                                                     this, SLOT(RenamePlaylist()));

  delete_playlist_action_ = context_menu_->addAction(IconLoader::Load("edit-delete", IconLoader::IconType::Other),
                                                     tr("Delete playlist"),
                                                     this, SLOT(DeletePlaylist()));

  delete_song_action_ = context_menu_->addAction(IconLoader::Load("edit-delete", IconLoader::IconType::Other),
                                                 tr("Delete song(s) from playlist"),
                                                 this, SLOT(DeleteSongsFromPlaylist()));

  QAction* config_action = context_menu_->addAction(IconLoader::Load("configure", IconLoader::IconType::Other),
                                                    tr("Configure Google Music..."),
                                                    this, SLOT(ShowConfig()));

  library_filter_ = new LibraryFilterWidget(0);
  library_filter_->SetSettingsGroup(kSettingsGroup);
  library_filter_->SetLibraryModel(library_model_);
  library_filter_->SetFilterHint(tr("Search Google Music catalogue"));
  library_filter_->SetAgeFilterEnabled(false);
  library_filter_->AddMenuAction(config_action);

  context_menu_->addSeparator();
  context_menu_->addMenu(library_filter_->menu());
}

void GoogleMusicService::FinishTask(int &task_id) const
{
  if (task_id) {
    app_->task_manager()->SetTaskFinished(task_id);
    task_id = 0;
  }
}

void GoogleMusicService::ShowContextMenu(const QPoint& global_pos) {
  EnsureMenuCreated();

  QModelIndex current = model()->current_index();
  const bool is_library = current.model() == library_sort_model_;

  int type = current.data(InternetModel::Role_Type).toInt();

  current_playlist_id_ = current.data(Role_PlaylistID).toString();
  current_song_id_ = current.data(InternetModel::Role_SongMetadata).value<Song>().url().host();

  while (type != Type_Library && type!= Type_PlaylistList && current.isValid()) {
    current = current.parent();
    type = current.data(InternetModel::Role_Type).toInt();
  }

  rename_playlist_action_->setVisible(!current_playlist_id_.isEmpty());
  delete_playlist_action_->setVisible(!current_playlist_id_.isEmpty());
  delete_song_action_->setVisible(!current_song_id_.isEmpty());

  switch (type) {
    case Type_Library:
      reload_database_action_->setVisible(true);
      reload_playlists_action_->setVisible(false);
      create_playlist_action_->setVisible(false);
      break;
    case Type_PlaylistList:
      reload_database_action_->setVisible(false);
      reload_playlists_action_->setVisible(true);
      create_playlist_action_->setVisible(true);
      break;
    default:
      reload_database_action_->setVisible(false);
      reload_playlists_action_->setVisible(false);
      create_playlist_action_->setVisible(false);
  }


  GetAppendToPlaylistAction()->setEnabled(is_library);
  GetReplacePlaylistAction()->setEnabled(is_library);
  GetOpenInNewPlaylistAction()->setEnabled(is_library);
  context_menu_->popup(global_pos);
}

QWidget* GoogleMusicService::HeaderWidget() const {
  const_cast<GoogleMusicService*>(this)->EnsureMenuCreated();
  return library_filter_;
}

void GoogleMusicService::DropMimeData(const QMimeData *data, const QModelIndex &index)
{   
  //Find playlistId
  //Available drop targets are playlist and song within playlist
  QString playlistId;
  if (index.data(Role_PlaylistID).isValid())
    playlistId = index.data(Role_PlaylistID).toString();
  else if (index.parent().data(Role_PlaylistID).isValid())
    playlistId = index.parent().data(Role_PlaylistID).toString();
  else return;

  //Extract song IDs
  QList<QUrl> urls = data->urls();
  QStringList songs;
  foreach (const QUrl& url, urls) {
    if (url.scheme()=="googlemusic")
      songs << url.host();
  }
  if (songs.empty())
    return;

  client_->AddSongsToPlaylist(playlistId,songs);
}

void GoogleMusicService::Homepage() {
  QDesktopServices::openUrl(QUrl(kHomepage));
}

void GoogleMusicService::GetSongUrl(const QUrl& url) const {
  client_->GetSongUrl(url);
}

void GoogleMusicService::GetSongUrlFinished(QUrl url, int error)
{
  url_handler_->TrackLoadFinished(url);
  if (error) {
    emit StreamError(tr("Failed to get song url for"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
  }
}

void GoogleMusicService::CreatePlaylist()
{
  bool ok;
  QString title = QInputDialog::getText(NULL,
                                        tr("Playlist Title"),
                                        tr("Enter new playlist title:"),
                                        QLineEdit::Normal,
                                        "", &ok);
  if (ok && !title.isEmpty())
    client_->CreatePlaylist(title);
}

void GoogleMusicService::PlaylistCreated(QString id, QString name, int error)
{
  if (error) {
    emit StreamError(tr("Failed to create playlist"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  QStandardItem* playlist = new QStandardItem(IconLoader::Load("view-media-playlist", IconLoader::IconType::Other),name);
  playlist->setData(id,Role_PlaylistID);
  playlist->setData(InternetModel::PlayBehaviour_MultipleItems,InternetModel::Role_PlayBehaviour);
  playlist->setData(true,InternetModel::Role_CanBeModified);
  playlist_item_->insertRow(playlist_item_->rowCount(),playlist);
}

void GoogleMusicService::RenamePlaylist()
{
  if (current_playlist_id_.isEmpty())
    return;

  bool ok;
  QStandardItem* current = GetPlaylistByID(current_playlist_id_);
  if (!current)
    return;
  QString title = QInputDialog::getText(NULL,
                                        tr("New Title"),
                                        tr("Enter playlist new title:"), QLineEdit::Normal,
                                        current->text(), &ok);
  if (ok && !title.isEmpty())
    client_->RenamePlaylist(current_playlist_id_,title);
}

void GoogleMusicService::PlaylistRenamed(QString id, QString newTitle, int error)
{
  if (error) {
    emit StreamError(tr("Failed to rename playlist"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  QStandardItem* item = GetPlaylistByID(id);
  if (item)
    item->setText(newTitle);
}

void GoogleMusicService::DeletePlaylist()
{
  if (current_playlist_id_.isEmpty())
    return;
  int result =  QMessageBox::question(NULL,
                                      tr("Delete Playlist"),
                                      tr("Do you really want to delete this playlist?"),
                                      QMessageBox::Yes|QMessageBox::No);
  if (result==QMessageBox::Yes)
    client_->DeletePlaylist(current_playlist_id_);
}

void GoogleMusicService::PlaylistDeleted(QString id, int error)
{
  if (error) {
    emit StreamError(tr("Failed to delete playlist"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  for (int i=0; i<playlist_item_->rowCount();++i) {
    QStandardItem* child = playlist_item_->child(i);
    QVariant item_id = child->data(Role_PlaylistID);
    if (item_id.isValid() && item_id.toString() == id)
      playlist_item_->removeRow(i);
  }
}

void GoogleMusicService::SongsAddedToPlaylist(QString id, QVariantList songJson, int error)
{
  if (error) {
    emit StreamError(tr("Failed to add songs to playlist"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  client_->GetPlaylistSongs(id);
}

void GoogleMusicService::SongsDeletedFromPlaylist(QString id, QVariantList songJson, int error)
{
  if (error) {
    emit StreamError(tr("Failed to delete songs from playlist"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  client_->GetPlaylistSongs(id);
}

void GoogleMusicService::PlaylistSongsGot(QString id, QVariantMap playlistJson, int error)
{
  if (error) {
    emit StreamError(tr("Failed to get playlist songs"));
    if (error==GoogleMusicClient::AuthorizationError) {
      ForgetCredentials();
      ShowConfig();
    }
    return;
  }

  QStandardItem* playlist = GetPlaylistByID(id);
  if (!playlist)
    return;

  playlist->removeRows(0,playlist->rowCount());
  PopulatePlaylist(playlistJson,playlist);
}

void GoogleMusicService::DeleteSongsFromPlaylist()
{
  //This method supports deleting selected songs from multiple playlists simultaneously
  QModelIndexList selected = model()->selected_indexes();
  //Key is Playlist ID, value is pair of vector of songs' ids and their playlist entry ids
  QMap<QString, QPair<QStringList,QStringList> > songsByAlbum;

  foreach (const QModelIndex& index, selected) {
    if (!index.data(InternetModel::Role_SongMetadata).isValid()
        || !index.data(Role_PlaylistEntryID).isValid()
        || !index.parent().data(Role_PlaylistID).isValid())
      continue;

    QString playlist_id = index.parent().data(Role_PlaylistID).toString();
    QString playlist_entry_id = index.data(Role_PlaylistEntryID).toString();
    QUrl song_id = index.data(InternetModel::Role_SongMetadata).value<Song>().url();
    if (song_id.scheme()=="googlemusic") {
      QPair<QStringList,QStringList>& val = songsByAlbum[playlist_id];
      val.first.push_back(song_id.host());
      val.second.push_back(playlist_entry_id);
    }
  }

  foreach (const QString& album_id, songsByAlbum.keys()) {
    QPair<QStringList,QStringList>& val = songsByAlbum[album_id];
    client_->DeleteSongsFromPlaylist(album_id,val.first,val.second);
  }
}

void GoogleMusicService::ForgetCredentials()
{
  client_->resetAuth();
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.remove("auth");
  s.remove("xt_cookie");
  s.remove("sjsaid_cookie");
  s.setValue("logged_in",false);
}

void GoogleMusicService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_GoogleMusic);
}

void GoogleMusicService::LoggedIn(QByteArray auth, QByteArray xt, QByteArray sjsaid)
{
  QSettings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("auth",auth);
  s.setValue("xt_cookie",xt);
  s.setValue("sjsaid_cookie",sjsaid);
  s.setValue("logged_in", true);

  ReloadSettings();
  sender()->deleteLater();
}
