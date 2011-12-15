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

#include <QtGlobal>

#ifdef Q_OS_WIN32
#  define _WIN32_WINNT 0x0600
#  include <windows.h>
#  include <iostream>
#endif // Q_OS_WIN32

#include "config.h"
#include "core/commandlineoptions.h"
#include "core/crashreporting.h"
#include "core/database.h"
#include "core/encoding.h"
#include "core/logging.h"
#include "core/mac_startup.h"
#include "core/network.h"
#include "core/networkproxyfactory.h"
#include "core/player.h"
#include "core/potranslator.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/ubuntuunityhack.h"
#include "core/utilities.h"
#include "covers/albumcoverfetcher.h"
#include "covers/amazoncoverprovider.h"
#include "covers/artloader.h"
#include "covers/coverproviders.h"
#include "engines/enginebase.h"
#include "globalsearch/globalsearch.h"
#include "internet/digitallyimportedclient.h"
#include "internet/internetmodel.h"
#include "internet/somafmservice.h"
#include "library/directory.h"
#include "playlist/playlist.h"
#include "playlist/playlistmanager.h"
#include "smartplaylists/generator.h"
#include "ui/equalizer.h"
#include "ui/iconloader.h"
#include "ui/mainwindow.h"
#include "ui/systemtrayicon.h"
#include "version.h"
#include "widgets/osd.h"

#include "qtsingleapplication.h"
#include "qtsinglecoreapplication.h"

#include <QDir>
#include <QLibraryInfo>
#include <QNetworkCookie>
#include <QNetworkProxyFactory>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextCodec>
#include <QTranslator>
#include <QtDebug>

#include <glib-object.h>
#include <glib.h>
#include <gst/gst.h>

#include <boost/scoped_ptr.hpp>
using boost::scoped_ptr;

#include <echonest/Config.h>

#ifdef HAVE_QCA
  #include <QtCrypto>
#endif

#ifdef Q_OS_DARWIN
  #include <sys/resource.h>
  #include <sys/sysctl.h>
#endif

#ifdef HAVE_LIBLASTFM
  #include "internet/lastfmservice.h"
#else
  class LastFMService;
#endif

#ifdef HAVE_DBUS
  #include "core/mpris.h"
  #include "core/mpris2.h"
  #include "dbus/metatypes.h"
  #include "globalsearch/globalsearchservice.h"
  #include <QDBusArgument>
  #include <QDBusConnection>
  #include <QImage>

  QDBusArgument& operator<< (QDBusArgument& arg, const QImage& image);
  const QDBusArgument& operator>> (const QDBusArgument& arg, QImage& image);
#endif

class GstEnginePipeline;

// Load sqlite plugin on windows and mac.
#ifdef HAVE_STATIC_SQLITE
# include <QtPlugin>
  Q_IMPORT_PLUGIN(qsqlite)
#endif

void LoadTranslation(const QString& prefix, const QString& path,
                     const QString& override_language = QString()) {
#if QT_VERSION < 0x040700
  // QTranslator::load will try to open and read "clementine" if it exists,
  // without checking if it's a file first.
  // This was fixed in Qt 4.7
  QFileInfo maybe_clementine_directory(path + "/clementine");
  if (maybe_clementine_directory.exists() && !maybe_clementine_directory.isFile())
    return;
#endif

  QString language = override_language.isEmpty() ?
                     QLocale::system().name() : override_language;

  QTranslator* t = new PoTranslator;
  if (t->load(prefix + "_" + language, path))
    QCoreApplication::installTranslator(t);
  else
    delete t;
  QTextCodec::setCodecForTr(QTextCodec::codecForLocale());
}

#ifdef HAVE_REMOTE
#include <xrme/connection.h>
#endif

int main(int argc, char *argv[]) {
  if (CrashReporting::SendCrashReport(argc, argv)) {
    return 0;
  }

  CrashReporting crash_reporting;

#ifdef Q_OS_DARWIN
  // Do Mac specific startup to get media keys working.
  // This must go before QApplication initialisation.
  mac::MacMain();

  {
    // Bump the soft limit for the number of file descriptors from the default of 256 to
    // the maximum (usually 10240).
    struct rlimit limit;
    getrlimit(RLIMIT_NOFILE, &limit);

    // getrlimit() lies about the hard limit so we have to check sysctl.
    int max_fd = 0;
    size_t len = sizeof(max_fd);
    sysctlbyname("kern.maxfilesperproc", &max_fd, &len, NULL, 0);

    limit.rlim_cur = max_fd;
    int ret = setrlimit(RLIMIT_NOFILE, &limit);

    if (ret == 0) {
      qLog(Debug) << "Max fd:" << max_fd;
    }
  }
#endif

  QCoreApplication::setApplicationName("Clementine");
  QCoreApplication::setApplicationVersion(CLEMENTINE_VERSION_DISPLAY);
  QCoreApplication::setOrganizationName("Clementine");
  QCoreApplication::setOrganizationDomain("clementine-player.org");

#ifdef Q_OS_DARWIN
  // Must happen after QCoreApplication::setOrganizationName().
  setenv("XDG_CONFIG_HOME", Utilities::GetConfigPath(Utilities::Path_Root).toLocal8Bit().constData(), 1);
  if (mac::MigrateLegacyConfigFiles()) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(Utilities::GetConfigPath(
        Utilities::Path_Root) + "/" + Database::kDatabaseFilename);
    db.open();
    QSqlQuery query(
        "UPDATE songs SET art_manual = replace("
        "art_manual, '.config', 'Library/Application Support') "
        "WHERE art_manual LIKE '%.config%'", db);
    query.exec();
    db.close();
    QSqlDatabase::removeDatabase(db.connectionName());
  }
#endif

  // This makes us show up nicely in gnome-volume-control
  g_type_init();
  g_set_application_name(QCoreApplication::applicationName().toLocal8Bit());

  qRegisterMetaType<CoverSearchResult>("CoverSearchResult");
  qRegisterMetaType<QList<CoverSearchResult> >("QList<CoverSearchResult>");
  qRegisterMetaType<CoverSearchResults>("CoverSearchResults");
  qRegisterMetaType<Directory>("Directory");
  qRegisterMetaType<DirectoryList>("DirectoryList");
  qRegisterMetaType<Subdirectory>("Subdirectory");
  qRegisterMetaType<SubdirectoryList>("SubdirectoryList");
  qRegisterMetaType<Song>("Song");
  qRegisterMetaType<QList<Song> >("QList<Song>");
  qRegisterMetaType<SongList>("SongList");
  qRegisterMetaType<PlaylistItemPtr>("PlaylistItemPtr");
  qRegisterMetaType<QList<PlaylistItemPtr> >("QList<PlaylistItemPtr>");
  qRegisterMetaType<PlaylistItemList>("PlaylistItemList");
  qRegisterMetaType<Engine::State>("Engine::State");
  qRegisterMetaType<Engine::SimpleMetaBundle>("Engine::SimpleMetaBundle");
  qRegisterMetaType<Equalizer::Params>("Equalizer::Params");
  qRegisterMetaTypeStreamOperators<Equalizer::Params>("Equalizer::Params");
  qRegisterMetaType<const char*>("const char*");
  qRegisterMetaType<QNetworkReply*>("QNetworkReply*");
  qRegisterMetaType<QNetworkReply**>("QNetworkReply**");
  qRegisterMetaType<smart_playlists::GeneratorPtr>("smart_playlists::GeneratorPtr");
  qRegisterMetaType<ColumnAlignmentMap>("ColumnAlignmentMap");
  qRegisterMetaTypeStreamOperators<QMap<int, int> >("ColumnAlignmentMap");
  qRegisterMetaType<QNetworkCookie>("QNetworkCookie");
  qRegisterMetaType<QList<QNetworkCookie> >("QList<QNetworkCookie>");
  qRegisterMetaType<SearchProvider::Result>("SearchProvider::Result");
  qRegisterMetaType<SearchProvider::ResultList>("SearchProvider::ResultList");
  qRegisterMetaType<DigitallyImportedClient::Channel>("DigitallyImportedClient::Channel");
  qRegisterMetaType<SomaFMService::Stream>("SomaFMService::Stream");
  qRegisterMetaTypeStreamOperators<DigitallyImportedClient::Channel>("DigitallyImportedClient::Channel");
  qRegisterMetaTypeStreamOperators<SomaFMService::Stream>("SomaFMService::Stream");

  qRegisterMetaType<GstBuffer*>("GstBuffer*");
  qRegisterMetaType<GstElement*>("GstElement*");
  qRegisterMetaType<GstEnginePipeline*>("GstEnginePipeline*");

#ifdef HAVE_REMOTE
  qRegisterMetaType<xrme::SIPInfo>("xrme::SIPInfo");
#endif

#ifdef HAVE_LIBLASTFM
  lastfm::ws::ApiKey = LastFMService::kApiKey;
  lastfm::ws::SharedSecret = LastFMService::kSecret;
  lastfm::setNetworkAccessManager(new NetworkAccessManager);
#endif

  CommandlineOptions options(argc, argv);

  {
    // Only start a core application now so we can check if there's another
    // Clementine running without needing an X server.
    // This MUST be done before parsing the commandline options so QTextCodec
    // gets the right system locale for filenames.
    QtSingleCoreApplication a(argc, argv);
    crash_reporting.SetApplicationPath(a.applicationFilePath());

    // Parse commandline options - need to do this before starting the
    // full QApplication so it works without an X server
    if (!options.Parse())
      return 1;

    if (a.isRunning()) {
      if (options.is_empty()) {
        qLog(Info) << "Clementine is already running - activating existing window";
      }
      if (a.sendMessage(options.Serialize(), 5000)) {
        return 0;
      }
      // Couldn't send the message so start anyway
    }
  }

  // Detect technically invalid usage of non-ASCII in ID3v1 tags.
  UniversalEncodingHandler handler;
  TagLib::ID3v1::Tag::setStringHandler(&handler);

#ifdef Q_OS_LINUX
  // Force Clementine's menu to be shown in the Clementine window and not in
  // the Unity global menubar thing.  See:
  // https://bugs.launchpad.net/unity/+bug/775278
  setenv("QT_X11_NO_NATIVE_MENUBAR", "1", true);
#endif

  // Initialise logging
  logging::Init();
  logging::SetLevels(options.log_levels());
  g_log_set_default_handler(reinterpret_cast<GLogFunc>(&logging::GLog), NULL);

  QtSingleApplication a(argc, argv);
#ifdef Q_OS_DARWIN
  QCoreApplication::setLibraryPaths(
      QStringList() << QCoreApplication::applicationDirPath() + "/../PlugIns");
#endif

  a.setQuitOnLastWindowClosed(false);

  // Do this check again because another instance might have started by now
  if (a.isRunning() && a.sendMessage(options.Serialize(), 5000)) {
    return 0;
  }

#ifndef Q_OS_DARWIN
  // Gnome on Ubuntu has menu icons disabled by default.  I think that's a bad
  // idea, and makes some menus in Clementine look confusing.
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
#else
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif

  // Set the permissions on the config file on Unix - it can contain passwords
  // for internet services so it's important that other users can't read it.
  // On Windows these are stored in the registry instead.
#ifdef Q_OS_UNIX
  {
    QSettings s;

    // Create the file if it doesn't exist already
    if (!QFile::exists(s.fileName())) {
      QFile file(s.fileName());
      file.open(QIODevice::WriteOnly);
    }

    // Set -rw-------
    QFile::setPermissions(s.fileName(), QFile::ReadOwner | QFile::WriteOwner);
  }
#endif

#ifdef HAVE_QCA
  QCA::Initializer qca_initializer;
#endif

  // Resources
  Q_INIT_RESOURCE(data);
  Q_INIT_RESOURCE(translations);

  // Has the user forced a different language?
  QString language = options.language();
  if (language.isEmpty()) {
    QSettings s;
    s.beginGroup("General");
    language = s.value("language").toString();
  }

  // Translations
  LoadTranslation("qt", QLibraryInfo::location(QLibraryInfo::TranslationsPath), language);
  LoadTranslation("clementine", ":/translations", language);
  LoadTranslation("clementine", a.applicationDirPath(), language);
  LoadTranslation("clementine", QDir::currentPath(), language);

  // Icons
  IconLoader::Init();

  Echonest::Config::instance()->setAPIKey("DFLFLJBUF4EGTXHIG");
  Echonest::Config::instance()->setNetworkAccessManager(new NetworkAccessManager);

  // Network proxy
  QNetworkProxyFactory::setApplicationProxyFactory(
      NetworkProxyFactory::Instance());

  // Seed the random number generator
  srand(time(NULL));

  // Initialize the repository of cover providers.  Last.fm registers itself
  // when its service is created.
  CoverProviders cover_providers;
  cover_providers.AddProvider(new AmazonCoverProvider);

  // Create some key objects
  scoped_ptr<BackgroundThread<Database> > database(
      new BackgroundThreadImplementation<Database, Database>(NULL));
  database->Start(true);
  TaskManager task_manager;
  PlaylistManager playlists(&task_manager, NULL);
  Player player(&playlists);
  GlobalSearch global_search;
  InternetModel internet_model(database.get(), &task_manager, &player,
                               &cover_providers, &global_search, NULL);

#ifdef Q_OS_LINUX
  // In 11.04 Ubuntu decided that the system tray should be reserved for certain
  // whitelisted applications.  Clementine will override this setting and insert
  // itself into the list of whitelisted apps.
  UbuntuUnityHack hack;
#endif // Q_OS_LINUX

  // Create the tray icon and OSD
  scoped_ptr<SystemTrayIcon> tray_icon(SystemTrayIcon::CreateSystemTrayIcon());
  OSD osd(tray_icon.get());

  ArtLoader art_loader;

#ifdef HAVE_DBUS
  qDBusRegisterMetaType<QImage>();
  qDBusRegisterMetaType<TrackMetadata>();
  qDBusRegisterMetaType<TrackIds>();
  qDBusRegisterMetaType<QList<QByteArray> >();

  mpris::Mpris mpris(&player, &art_loader);

  QObject::connect(&playlists, SIGNAL(CurrentSongChanged(Song)), &art_loader, SLOT(LoadArt(Song)));
  QObject::connect(&art_loader, SIGNAL(ThumbnailLoaded(Song, QString, QImage)),
                   &osd, SLOT(CoverArtPathReady(Song, QString)));

  GlobalSearchService global_search_service(&global_search);
#endif

  // Window
  MainWindow w(
      database.get(),
      &task_manager,
      &playlists,
      &internet_model,
      &player,
      tray_icon.get(),
      &osd,
      &art_loader,
      &cover_providers,
      &global_search);
#ifdef HAVE_DBUS
  QObject::connect(&mpris, SIGNAL(RaiseMainWindow()), &w, SLOT(Raise()));
#endif
  QObject::connect(&a, SIGNAL(messageReceived(QByteArray)), &w, SLOT(CommandlineOptionsReceived(QByteArray)));
  w.CommandlineOptionsReceived(options);

  int ret = a.exec();

#ifdef Q_OS_LINUX
  // The nvidia driver would cause Clementine (or any application that used
  // opengl) to use 100% cpu on shutdown.  See:
  //   http://code.google.com/p/clementine-player/issues/detail?id=2088
  //   https://bugs.gentoo.org/show_bug.cgi?id=375615
  // Work around this problem by exiting immediately (and not running the buggy
  // nvidia atexit shutdown handler) if we're using one of the affected versions
  // of the nvidia driver.

  QFile self_maps("/proc/self/maps");
  if (self_maps.open(QIODevice::ReadOnly)) {
    QByteArray data = self_maps.readAll();
    if (data.contains("libnvidia-tls.so.285.03") ||
        data.contains("libnvidia-tls.so.280.13") ||
        data.contains("libnvidia-tls.so.275.28") ||
        data.contains("libnvidia-tls.so.275.19")) {
      qLog(Warning) << "Exiting immediately to work around NVIDIA driver bug";
      _exit(ret);
    }
    self_maps.close();
  }
#endif

  return ret;
}
