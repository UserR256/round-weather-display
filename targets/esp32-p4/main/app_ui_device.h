#pragma once

#include "lvgl.h"

// Settings panels for the device itself:
//  - "Web Page": QR code + address of the clock's web page
//  - "Factory Reset": confirmation, then all settings are erased
// Both are created hidden on `parent` (the settings overlay).
void create_device_panels(lv_obj_t *parent);
void show_web_page_panel(void);
void show_factory_reset_panel(void);
