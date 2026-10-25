#pragma once

#include "lvgl.h"

lv_obj_t *create_alarm_panel(lv_obj_t *parent);
void show_alarm_panel(void);
void show_alarm_ring_popup(void);
void hide_alarm_ring_popup(void);

// Volume control drawn on the left part of the blue ring of the digital face.
lv_obj_t *create_volume_arc(lv_obj_t *parent);
void volume_arc_set_night(lv_obj_t *arc, bool night);   // match the ring colours of the day/night art
