#pragma once

#include "lvgl.h"

/**
 * The screen shown while the device starts: Squadron Leader Biffo in a brass
 * frame, the title, what's starting and the version. Shared by the firmware
 * and the simulator.
 */
namespace ui::boot {

/** Builds the boot screen on `scr`. False if Biffo's portrait didn't decode (the rest still shows). */
bool build(lv_obj_t* scr, const char* version);
/** The line under the title ("Starting Wi-Fi module..."). */
void setStatus(const char* status);
/** Frees the portrait's pixels; call once the boot screen has been deleted. */
void release();

}  // namespace ui::boot
