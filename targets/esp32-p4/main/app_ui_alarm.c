#include "app_ui_alarm.h"
#include "audio_alarm.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_alarm";

static lv_obj_t *s_alarm_panel = NULL;
static lv_obj_t *s_ring_popup = NULL;
static lv_obj_t *s_hour_roller = NULL;
static lv_obj_t *s_minute_roller = NULL;
static lv_obj_t *s_enable_btn = NULL;
static lv_obj_t *s_type_btn = NULL;

static alarm_config_t s_edit_alarm;

static void refresh_alarm_buttons(void) {
  if (s_enable_btn) {
    lv_obj_t *label = lv_obj_get_child(s_enable_btn, 0);
    if (label) {
      lv_label_set_text(label, s_edit_alarm.enabled ? "Alarm: ON" : "Alarm: OFF");
    }
  }
  if (s_type_btn) {
    lv_obj_t *label = lv_obj_get_child(s_type_btn, 0);
    if (label) {
      lv_label_set_text(label, s_edit_alarm.type == ALARM_TYPE_RADIO ? "Type: Radio" : "Type: Beep");
    }
  }
}

static void enable_event_cb(lv_event_t *e) {
  (void)e;
  s_edit_alarm.enabled = !s_edit_alarm.enabled;
  refresh_alarm_buttons();
}

static void type_event_cb(lv_event_t *e) {
  (void)e;
  s_edit_alarm.type = (s_edit_alarm.type == ALARM_TYPE_BEEP) ? ALARM_TYPE_RADIO : ALARM_TYPE_BEEP;
  refresh_alarm_buttons();
}

static void save_event_cb(lv_event_t *e) {
  (void)e;
  if (s_hour_roller) s_edit_alarm.hour = (uint8_t)lv_roller_get_selected(s_hour_roller);
  if (s_minute_roller) s_edit_alarm.minute = (uint8_t)lv_roller_get_selected(s_minute_roller);

  audio_alarm_set(&s_edit_alarm);
  ESP_LOGI(TAG, "Alarm saved: %02d:%02d %s %s",
           s_edit_alarm.hour, s_edit_alarm.minute,
           s_edit_alarm.enabled ? "ON" : "OFF",
           s_edit_alarm.type == ALARM_TYPE_RADIO ? "RADIO" : "BEEP");

  if (s_alarm_panel) {
    lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

static void close_event_cb(lv_event_t *e) {
  (void)e;
  if (s_alarm_panel) {
    lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

static void stop_event_cb(lv_event_t *e) {
  (void)e;
  audio_alarm_stop();
  if (s_ring_popup) {
    lv_obj_add_flag(s_ring_popup, LV_OBJ_FLAG_HIDDEN);
  }
}

static void snooze_event_cb(lv_event_t *e) {
  (void)e;
  audio_alarm_snooze();
  if (s_ring_popup) {
    lv_obj_add_flag(s_ring_popup, LV_OBJ_FLAG_HIDDEN);
  }
}

static lv_obj_t *create_ring_popup(lv_obj_t *parent) {
  s_ring_popup = lv_obj_create(parent);
  lv_obj_set_size(s_ring_popup, 420, 260);
  lv_obj_center(s_ring_popup);
  lv_obj_set_style_bg_color(s_ring_popup, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_radius(s_ring_popup, 24, 0);
  lv_obj_set_style_border_width(s_ring_popup, 2, 0);
  lv_obj_set_style_border_color(s_ring_popup, lv_color_hex(0xff6b6b), 0);
  lv_obj_add_flag(s_ring_popup, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = lv_label_create(s_ring_popup);
  lv_label_set_text(title, "Alarm!");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

  lv_obj_t *sub = lv_label_create(s_ring_popup);
  lv_label_set_text(sub, "Time to wake up");
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 75);
  lv_obj_set_style_text_color(sub, lv_color_hex(0xaaaaaa), 0);

  lv_obj_t *stop_btn = lv_btn_create(s_ring_popup);
  lv_obj_set_size(stop_btn, 160, 50);
  lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
  lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0x4caf50), 0);
  lv_obj_add_event_cb(stop_btn, stop_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *stop_label = lv_label_create(stop_btn);
  lv_label_set_text(stop_label, "Stop");
  lv_obj_center(stop_label);

  lv_obj_t *snooze_btn = lv_btn_create(s_ring_popup);
  lv_obj_set_size(snooze_btn, 160, 50);
  lv_obj_align(snooze_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
  lv_obj_set_style_bg_color(snooze_btn, lv_color_hex(0xff9800), 0);
  lv_obj_add_event_cb(snooze_btn, snooze_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *snooze_label = lv_label_create(snooze_btn);
  lv_label_set_text(snooze_label, "Snooze 10m");
  lv_obj_center(snooze_label);

  return s_ring_popup;
}

lv_obj_t *create_alarm_panel(lv_obj_t *parent) {
  audio_alarm_get(&s_edit_alarm);

  s_alarm_panel = lv_obj_create(parent);
  lv_obj_set_size(s_alarm_panel, 500, 400);
  lv_obj_center(s_alarm_panel);
  lv_obj_set_style_bg_color(s_alarm_panel, lv_color_hex(0x1a1a2e), 0);
  lv_obj_set_style_radius(s_alarm_panel, 20, 0);
  lv_obj_add_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = lv_label_create(s_alarm_panel);
  lv_label_set_text(title, "Alarm Settings");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

  s_hour_roller = lv_roller_create(s_alarm_panel);
  char hours[24 * 4];
  char *p = hours;
  for (int i = 0; i < 24; i++) {
    p += sprintf(p, "%02d\n", i);
  }
  *(p - 1) = '\0';
  lv_roller_set_options(s_hour_roller, hours, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_selected(s_hour_roller, s_edit_alarm.hour, LV_ANIM_OFF);
  lv_obj_set_width(s_hour_roller, 80);
  lv_obj_align(s_hour_roller, LV_ALIGN_CENTER, -60, -20);

  s_minute_roller = lv_roller_create(s_alarm_panel);
  char minutes[60 * 4];
  p = minutes;
  for (int i = 0; i < 60; i++) {
    p += sprintf(p, "%02d\n", i);
  }
  *(p - 1) = '\0';
  lv_roller_set_options(s_minute_roller, minutes, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_selected(s_minute_roller, s_edit_alarm.minute, LV_ANIM_OFF);
  lv_obj_set_width(s_minute_roller, 80);
  lv_obj_align(s_minute_roller, LV_ALIGN_CENTER, 60, -20);

  lv_obj_t *colon = lv_label_create(s_alarm_panel);
  lv_label_set_text(colon, ":");
  lv_obj_align(colon, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_text_color(colon, lv_color_white(), 0);
  lv_obj_set_style_text_font(colon, &lv_font_montserrat_24, 0);

  s_enable_btn = lv_btn_create(s_alarm_panel);
  lv_obj_set_size(s_enable_btn, 180, 45);
  lv_obj_align(s_enable_btn, LV_ALIGN_BOTTOM_LEFT, 30, -80);
  lv_obj_add_event_cb(s_enable_btn, enable_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *en_label = lv_label_create(s_enable_btn);
  lv_obj_center(en_label);

  s_type_btn = lv_btn_create(s_alarm_panel);
  lv_obj_set_size(s_type_btn, 180, 45);
  lv_obj_align(s_type_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -80);
  lv_obj_add_event_cb(s_type_btn, type_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *type_label = lv_label_create(s_type_btn);
  lv_obj_center(type_label);

  lv_obj_t *save_btn = lv_btn_create(s_alarm_panel);
  lv_obj_set_size(save_btn, 140, 40);
  lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 40, -20);
  lv_obj_add_event_cb(save_btn, save_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *save_label = lv_label_create(save_btn);
  lv_label_set_text(save_label, "Save");
  lv_obj_center(save_label);

  lv_obj_t *close_btn = lv_btn_create(s_alarm_panel);
  lv_obj_set_size(close_btn, 140, 40);
  lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
  lv_obj_add_event_cb(close_btn, close_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *close_label = lv_label_create(close_btn);
  lv_label_set_text(close_label, "Close");
  lv_obj_center(close_label);

  refresh_alarm_buttons();

  // Create the ringing popup as well
  create_ring_popup(parent);

  return s_alarm_panel;
}

void show_alarm_panel(void) {
  if (s_alarm_panel) {
    audio_alarm_get(&s_edit_alarm);
    if (s_hour_roller) lv_roller_set_selected(s_hour_roller, s_edit_alarm.hour, LV_ANIM_OFF);
    if (s_minute_roller) lv_roller_set_selected(s_minute_roller, s_edit_alarm.minute, LV_ANIM_OFF);
    refresh_alarm_buttons();
    lv_obj_clear_flag(s_alarm_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_alarm_panel);
  }
}

void show_alarm_ring_popup(void) {
  if (s_ring_popup) {
    lv_obj_clear_flag(s_ring_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ring_popup);
  }
}

void hide_alarm_ring_popup(void) {
  if (s_ring_popup) {
    lv_obj_add_flag(s_ring_popup, LV_OBJ_FLAG_HIDDEN);
  }
}
