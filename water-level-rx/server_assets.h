#ifndef _SERVER_ASSETS_H
#define _SERVER_ASSETS_H

// Required for PROGMEM in the generated assets file
#include <avr/pgmspace.h>

#include <cstddef>

struct AssetEntry {
  const char *name;
  const char *contentType;
  const char *content;
};

class AssetInfo {
  std::size_t entryCount;
  AssetEntry *entries;

public:
  AssetInfo();
  AssetInfo(const AssetInfo&) = delete;
  ~AssetInfo();

  const AssetEntry * begin();
  const AssetEntry * end();
  const AssetEntry * find(const char* name);
};

#endif
