#pragma once

// Public AirPlaneTracker service: current flights and persisted history.
#define FLIGHTS_API_URL "https://roshpinaoverhead.online/api/flights"
#define HISTORY_API_URL "https://roshpinaoverhead.online/api/flights/history"

// The same service proxies the official alert source.
#define ALERTS_API_URL "https://roshpinaoverhead.online/api/alerts"

// Installation coordinates are local configuration because they can reveal a
// home address. Copy location.example.h to location.h and keep location.h out
// of version control. Secret-free builds deliberately disable weather and
// solar polling rather than querying a misleading placeholder location.
#if __has_include("location.h")
#include "location.h"
#define HAS_LOCATION_CONFIGURATION 1
#elif __has_include("../location.h")
#include "../location.h"
#define HAS_LOCATION_CONFIGURATION 1
#else
#define HAS_LOCATION_CONFIGURATION 0
#define HOME_LATITUDE 0.0
#define HOME_LONGITUDE 0.0
#endif
