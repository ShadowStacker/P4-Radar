#include "ui/keyboard.h"

#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

namespace {

struct InputCtx {
  lv_obj_t* scrim;
  lv_obj_t* ta;
  std::function<void(const std::string&)> on_done;
};

void onKeyboardEvent(lv_event_t* e) {
  auto* ctx = static_cast<InputCtx*>(lv_event_get_user_data(e));
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const std::string text = lv_textarea_get_text(ctx->ta);
    auto cb = ctx->on_done;
    lv_obj_delete_async(ctx->scrim);
    if (cb) cb(text);
  } else if (code == LV_EVENT_CANCEL) {
    lv_obj_delete_async(ctx->scrim);
  }
}

void onScrimDelete(lv_event_t* e) { delete static_cast<InputCtx*>(lv_event_get_user_data(e)); }

}  // namespace

void textInput(const char* title, const std::string& initial, InputKind kind,
               std::function<void(const std::string&)> on_done) {
  const Palette& p = pal();
  const int32_t W = screenW(), H = screenH();

  lv_obj_t* scrim = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(scrim);
  lv_obj_set_size(scrim, W, H);
  lv_obj_set_style_bg_color(scrim, p.bg, 0);
  lv_obj_set_style_bg_opa(scrim, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(scrim, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* t = label(scrim, title, font(FontSize::L), p.text);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, sc(110));

  lv_obj_t* ta = lv_textarea_create(scrim);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, initial.c_str());
  lv_obj_set_width(ta, sc(560));
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, sc(170));
  lv_obj_set_style_text_font(ta, font(FontSize::L), 0);
  lv_obj_set_style_bg_color(ta, p.panel, 0);
  lv_obj_set_style_border_color(ta, p.accent, 0);
  lv_obj_set_style_text_color(ta, p.text, 0);
  lv_obj_set_style_radius(ta, sc(18), 0);
  lv_obj_add_state(ta, LV_STATE_FOCUSED);
  if (kind == InputKind::Password) {
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_password_show_time(ta, 1200);
  }

  if (kind == InputKind::Password) {
    lv_obj_t* eye = iconButton(scrim, LV_SYMBOL_EYE_OPEN, sc(60), nullptr);
    lv_obj_align_to(eye, ta, LV_ALIGN_OUT_RIGHT_MID, sc(8), 0);
    onClick(eye, [ta] { lv_textarea_set_password_mode(ta, !lv_textarea_get_password_mode(ta)); });
    lv_obj_set_width(ta, sc(480));
    lv_obj_align(ta, LV_ALIGN_TOP_MID, -sc(36), sc(170));
    lv_obj_align_to(eye, ta, LV_ALIGN_OUT_RIGHT_MID, sc(10), 0);
  }

  // The keyboard sits in the widest part of the circle.
  lv_obj_t* kb = lv_keyboard_create(scrim);
  lv_obj_set_size(kb, sc(660), sc(330));
  lv_obj_align(kb, LV_ALIGN_TOP_MID, 0, sc(268));
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_text_font(kb, font(FontSize::M), 0);
  lv_obj_set_style_pad_all(kb, sc(4), 0);
  lv_obj_set_style_pad_gap(kb, sc(6), 0);
  lv_obj_set_style_radius(kb, sc(10), LV_PART_ITEMS);
  lv_keyboard_set_textarea(kb, ta);
  if (kind == InputKind::Number) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);

  auto* ctx = new InputCtx{scrim, ta, std::move(on_done)};
  lv_obj_add_event_cb(kb, onKeyboardEvent, LV_EVENT_READY, ctx);
  lv_obj_add_event_cb(kb, onKeyboardEvent, LV_EVENT_CANCEL, ctx);
  lv_obj_add_event_cb(scrim, onScrimDelete, LV_EVENT_DELETE, ctx);

  // Explicit cancel / OK below the keyboard for discoverability.
  lv_obj_t* row = lv_obj_create(scrim);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, sc(420), LV_SIZE_CONTENT);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, sc(612));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  pillButton(row, "Cancel", [kb] { lv_obj_send_event(kb, LV_EVENT_CANCEL, nullptr); });
  pillButton(row, LV_SYMBOL_OK "  OK", [kb] { lv_obj_send_event(kb, LV_EVENT_READY, nullptr); }, true);
}

}  // namespace ui
