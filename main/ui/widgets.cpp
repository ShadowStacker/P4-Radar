#include "ui/widgets.h"

#include "app/app.h"
#include "ui/theme.h"

namespace ui {

namespace {

struct Handler {
  lv_event_code_t code;
  Callback cb;
};

void dispatch(lv_event_t* e) {
  auto* h = static_cast<Handler*>(lv_event_get_user_data(e));
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_DELETE) {
    delete h;
    return;
  }
  if (code == h->code && h->cb) {
    if (code == LV_EVENT_CLICKED) app::sound(core::Sound::Click);
    // Copy: the callback may delete this object (and the handler with it).
    Callback cb = h->cb;
    cb();
  }
}

}  // namespace

void onEvent(lv_obj_t* obj, lv_event_code_t code, Callback cb) {
  auto* h = new Handler{code, std::move(cb)};
  lv_obj_add_event_cb(obj, dispatch, code, h);
  lv_obj_add_event_cb(obj, dispatch, LV_EVENT_DELETE, h);
}

void onClick(lv_obj_t* obj, Callback cb) { onEvent(obj, LV_EVENT_CLICKED, std::move(cb)); }

lv_obj_t* iconButton(lv_obj_t* parent, const char* symbol, int32_t d, Callback cb) {
  const Palette& p = pal();
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, d, d);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, p.panel, 0);
  lv_obj_set_style_bg_opa(b, 200, 0);
  lv_obj_set_style_border_color(b, p.panel_border, 0);
  lv_obj_set_style_border_width(b, sc(2), 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_bg_color(b, p.accent, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(b, p.accent, LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(b, 90, LV_STATE_CHECKED);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, symbol);
  lv_obj_set_style_text_font(l, font(FontSize::L), 0);
  lv_obj_set_style_text_color(l, p.text, 0);
  lv_obj_center(l);
  if (cb) onClick(b, std::move(cb));
  return b;
}

lv_obj_t* pillButton(lv_obj_t* parent, const char* text, Callback cb, bool accent) {
  const Palette& p = pal();
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_height(b, sc(64));
  lv_obj_set_style_pad_hor(b, sc(28), 0);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_bg_color(b, accent ? p.accent : p.panel, 0);
  lv_obj_set_style_bg_opa(b, accent ? LV_OPA_COVER : 230, 0);
  lv_obj_set_style_border_color(b, p.panel_border, 0);
  lv_obj_set_style_border_width(b, accent ? 0 : sc(2), 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font(FontSize::M), 0);
  lv_obj_set_style_text_color(l, accent ? lv_color_black() : p.text, 0);
  lv_obj_center(l);
  if (cb) onClick(b, std::move(cb));
  return b;
}

lv_obj_t* panel(lv_obj_t* parent) {
  const Palette& p = pal();
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_style_bg_color(o, p.panel, 0);
  lv_obj_set_style_bg_opa(o, 235, 0);
  lv_obj_set_style_border_color(o, p.panel_border, 0);
  lv_obj_set_style_border_width(o, sc(2), 0);
  lv_obj_set_style_radius(o, sc(28), 0);
  lv_obj_set_style_pad_all(o, sc(18), 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* f, lv_color_t c) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}

void confirm(const char* title, const char* body, const char* yes_text, Callback on_yes) {
  const Palette& p = pal();
  lv_obj_t* scrim = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(scrim);
  lv_obj_set_size(scrim, screenW(), screenH());
  lv_obj_set_style_bg_color(scrim, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scrim, 160, 0);
  lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* box = panel(scrim);
  lv_obj_set_width(box, sc(560));
  lv_obj_set_height(box, LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, sc(16), 0);
  lv_obj_set_style_pad_all(box, sc(28), 0);

  lv_obj_t* t = label(box, title, font(FontSize::L), p.text);
  (void)t;
  lv_obj_t* b = label(box, body, font(FontSize::M), p.text_dim);
  lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(b, lv_pct(100));
  lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* row = lv_obj_create(box);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(row, sc(8), 0);

  pillButton(row, "Cancel", [scrim] { lv_obj_delete_async(scrim); });
  pillButton(
      row, yes_text,
      [scrim, on_yes] {
        lv_obj_delete_async(scrim);
        if (on_yes) on_yes();
      },
      true);
}

void toast(const char* text, uint32_t ms) {
  const Palette& p = pal();
  lv_obj_t* o = panel(lv_layer_top());
  lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(o, sc(26), 0);
  lv_obj_set_style_pad_ver(o, sc(14), 0);
  lv_obj_align(o, LV_ALIGN_TOP_MID, 0, sc(150));
  label(o, text, font(FontSize::M), p.text);
  lv_obj_delete_delayed(o, ms);
}

}  // namespace ui
