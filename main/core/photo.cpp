#include "core/photo.h"

#include "cJSON.h"

namespace core::photo {

std::string lookupUrl(const char* hex) { return std::string("https://api.planespotters.net/pub/photos/hex/") + hex; }

bool parse(const char* json, size_t len, Info* out) {
  *out = Info{};
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) return false;
  const cJSON* photos = cJSON_GetObjectItemCaseSensitive(root, "photos");
  const cJSON* first = cJSON_IsArray(photos) ? cJSON_GetArrayItem(photos, 0) : nullptr;
  if (first != nullptr) {
    auto src = [&](const char* name) -> std::string {
      const cJSON* t = cJSON_GetObjectItemCaseSensitive(first, name);
      const cJSON* s = t ? cJSON_GetObjectItemCaseSensitive(t, "src") : nullptr;
      return cJSON_IsString(s) ? s->valuestring : "";
    };
    out->image_url = src("thumbnail_large");
    if (out->image_url.empty()) out->image_url = src("thumbnail");
    const cJSON* who = cJSON_GetObjectItemCaseSensitive(first, "photographer");
    const cJSON* link = cJSON_GetObjectItemCaseSensitive(first, "link");
    if (cJSON_IsString(who)) out->photographer = who->valuestring;
    if (cJSON_IsString(link)) out->link = link->valuestring;
  }
  cJSON_Delete(root);
  return !out->image_url.empty();
}

}  // namespace core::photo
