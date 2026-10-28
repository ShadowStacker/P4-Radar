#include "core/update.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "cJSON.h"

namespace core::update {

bool parseVersion(const char* text, int out[3]) {
  if (text == nullptr) return false;
  const char* p = text;
  if (*p == 'v' || *p == 'V') ++p;
  for (int i = 0; i < 3; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    char* end = nullptr;
    out[i] = static_cast<int>(std::strtol(p, &end, 10));
    p = end;
    if (i < 2) {
      if (*p != '.') return false;
      ++p;
    }
  }
  return *p == '\0' || *p == '-' || *p == '+';  // "1.2.3-beta" counts as 1.2.3
}

int compareVersions(const char* a, const char* b) {
  int va[3], vb[3];
  const bool oa = parseVersion(a, va), ob = parseVersion(b, vb);
  if (!oa || !ob) return static_cast<int>(oa) - static_cast<int>(ob);
  for (int i = 0; i < 3; ++i) {
    if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
  }
  return 0;
}

std::string assetName(const char* variant, const char* tag) {
  return std::string("plane-radar-") + variant + "-" + tag + "-ota.bin";
}

bool parseLatestRelease(const char* json, size_t len, const char* variant, Release* out) {
  *out = Release{};
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;
  int v[3];
  const cJSON* tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
  const bool ok = cJSON_IsString(tag) && parseVersion(tag->valuestring, v);
  if (ok) {
    out->tag = tag->valuestring;
    const std::string want = assetName(variant, out->tag.c_str());
    const cJSON* assets = cJSON_GetObjectItemCaseSensitive(root, "assets");
    const cJSON* asset = nullptr;
    cJSON_ArrayForEach(asset, assets) {
      const cJSON* name = cJSON_GetObjectItemCaseSensitive(asset, "name");
      const cJSON* url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");
      const cJSON* size = cJSON_GetObjectItemCaseSensitive(asset, "size");
      if (cJSON_IsString(name) && cJSON_IsString(url) && want == name->valuestring) {
        out->asset_url = url->valuestring;
        out->asset_size = cJSON_IsNumber(size) && size->valuedouble > 0 ? static_cast<size_t>(size->valuedouble) : 0;
        break;
      }
    }
  }
  cJSON_Delete(root);
  return ok;
}

}  // namespace core::update
