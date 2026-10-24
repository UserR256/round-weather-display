#pragma once

#include "lvgl.h"

lv_obj_t *create_alarm_panel(lv_obj_t *parent);
void show_alarm_panel(void);
void show_alarm_ring_popup(void);
void hide_alarm_ring_popup(void);
