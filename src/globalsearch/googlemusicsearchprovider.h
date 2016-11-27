#ifndef GOOGLEMUSICSEARCHPROVIDER_H
#define GOOGLEMUSICSEARCHPROVIDER_H

#include "librarysearchprovider.h"
#include <internet/googlemusic/googlemusicservice.h>

class GoogleMusicService;
class LibraryBackend;
class GoogleMusicSearchProvider: public LibrarySearchProvider
{
public:
  GoogleMusicSearchProvider( GoogleMusicService* service, LibraryBackend* backend );

  virtual bool IsLoggedIn();
  virtual void ShowConfig(); // Remember to set the CanShowConfig hint
  // Returns the Internet service in charge of this provider, or NULL if there
  // is none
  virtual InternetService* internet_service();

private:
  GoogleMusicService* service_;
};

#endif // GOOGLEMUSICSEARCHPROVIDER_H
