#include <cstring>

#include "server_assets.h"

AssetInfo::~AssetInfo() {
  delete[] entries;
}

const AssetEntry * AssetInfo::begin() {
  return &entries[0];
}

const AssetEntry * AssetInfo::end() {
  return &entries[entryCount];
}

const AssetEntry * AssetInfo::find(const char* name) {
  for (const AssetEntry &entry : *this) {
    if (std::strcmp(entry.name, name) == 0) {
      return &entry;
    }
  }
  return nullptr;
}
