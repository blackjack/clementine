#include "googlemusicsearchprovider.h"
#include <library/librarybackend.h>
#include "ui/iconloader.h"

GoogleMusicSearchProvider::GoogleMusicSearchProvider( GoogleMusicService* service, LibraryBackend* backend )
  : LibrarySearchProvider( backend,
                           tr( GoogleMusicService::kServiceName ),
                           "googlemusic",
                           IconLoader::Load("googlemusic", IconLoader::IconType::Provider),
                           false,
                           app_,
                           service
                         ),
    service_( service )
{
  SearchProvider::Init( name(), id(), icon(), hints() | CanShowConfig );
}

bool GoogleMusicSearchProvider::IsLoggedIn()
{
  return service_->IsLoggedIn();
}

void GoogleMusicSearchProvider::ShowConfig()
{
  service_->ShowConfig();
}

InternetService* GoogleMusicSearchProvider::internet_service()
{
  return service_;
}
