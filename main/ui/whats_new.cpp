#include "ui/whats_new.h"

#include <cstdio>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/release_notes.h"
#include "core/update.h"
#include "platform/platform.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "ui/widgets.h"

namespace app {
extern const char kChangelogMarkdown[];  // generated from CHANGELOG.md (cmake_changelog.cmake)
}

namespace ui {

namespace {

lv_obj_t* s_box = nullptr;

void close() {
  // Hidden while its deletion is pending, so a second tap on OK can't delete it twice.
  if (s_box == nullptr || lv_obj_has_flag(s_box, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_delete_async(s_box);
}

void show(const std::vector<core::release_notes::Section>& sections) {
  if (sections.empty() || s_box != nullptr) return;
  const Palette& p = pal();
  s_box = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_box);
  lv_obj_set_size(s_box, screenW(), screenH());
  lv_obj_set_style_bg_color(s_box, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_box, 150, 0);
  lv_obj_add_flag(s_box, LV_OBJ_FLAG_CLICKABLE);  // taps don't reach the radar
  // However it goes away (OK, or cleared with the rest of the top layer), it can open again.
  lv_obj_add_event_cb(s_box, [](lv_event_t*) { s_box = nullptr; }, LV_EVENT_DELETE, nullptr);

  lv_obj_t* card = panel(s_box);
  lv_obj_set_size(card, sc(560), sc(560));
  lv_obj_center(card);
  lv_obj_set_style_pad_all(card, sc(24), 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, sc(10), 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

  label(card, (std::string(LV_SYMBOL_BELL "  What's new in ") + sections.front().version).c_str(), font(FontSize::L),
        p.accent);

  // The notes scroll; the OK button stays put below them.
  lv_obj_t* list = lv_obj_create(card);
  lv_obj_remove_style_all(list);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, sc(8), 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  for (size_t i = 0; i < sections.size(); ++i) {
    if (sections.size() > 1) {
      lv_obj_t* h = label(list, (sections[i].version + "  (" + sections[i].date + ")").c_str(), font(FontSize::S),
                          p.text_dim);
      if (i > 0) lv_obj_set_style_pad_top(h, sc(10), 0);
    }
    for (const std::string& item : sections[i].items) {
      lv_obj_t* l = label(list, ("-  " + item).c_str(), font(FontSize::S), p.text);
      lv_obj_set_width(l, lv_pct(100));
      lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    }
  }
  lv_obj_t* hint = label(card, "Full notes: CHANGELOG.md on GitHub", font(FontSize::S), p.text_dim);
  (void)hint;
  pillButton(card, LV_SYMBOL_OK "  OK", [] { close(); }, true);
}

std::vector<core::release_notes::Section> allSections() {
  return core::release_notes::parse(app::kChangelogMarkdown);
}

}  // namespace

void maybeShowWhatsNew() {
  auto& st = app::settings();
  const std::string current = plat::firmwareVersion();
  if (current == st.last_seen_version) return;
  // Not during first-time setup, nor over the settings or another dialog.
  if (st.wifi_ssid[0] == '\0' || settingsOpen() || app::alarmRinging() >= 0 || app::sleeping()) return;
  if (plat::millis() < 6000) return;  // let the radar come up first
  const std::string previous = st.last_seen_version;
  std::snprintf(st.last_seen_version, sizeof(st.last_seen_version), "%s", current.c_str());
  app::settingsChanged();
  show(core::release_notes::newSince(allSections(), current, previous, 3));
}

void showWhatsNew() {
  const std::string current = plat::firmwareVersion();
  std::vector<core::release_notes::Section> all = allSections();
  // This version and the two before it (sections are newest first).
  std::vector<core::release_notes::Section> recent;
  for (const auto& s : allSections()) {
    if (core::update::compareVersions(s.version.c_str(), current.c_str()) > 0) continue;  // not out yet
    recent.push_back(s);
    if (recent.size() == 3) break;
  }
  show(recent);
}

}  // namespace ui
