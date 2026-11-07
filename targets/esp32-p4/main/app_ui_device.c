#include "app_ui_device.h"

#include <string.h>

#include "device_discovery.h"
#include "i18n.h"

#define PANEL_BG    0x1a1a2e
#define DANGER      0xc62828
#define QR_SIZE     220

/* The QR code needs CONFIG_LV_USE_QRCODE=y (in sdkconfig.defaults). An older
 * local sdkconfig may not have it yet: then the panel shows only the
 * addresses, and the build still works. */
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
#define HAVE_QRCODE 1
#else
#define HAVE_QRCODE 0
#endif

static lv_obj_t *s_web_panel = NULL;
static lv_obj_t *s_qr = NULL;
static lv_obj_t *s_ip_label = NULL;
static lv_obj_t *s_local_label = NULL;
static lv_obj_t *s_hint_label = NULL;

static lv_obj_t *s_reset_panel = NULL;
static lv_obj_t *s_reset_text = NULL;
static lv_obj_t *s_reset_buttons = NULL;

// ---- helpers ------------------------------------------------------------------

static lv_obj_t *make_panel(lv_obj_t *parent, int w, int h) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, w, h);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(PANEL_BG), 0);
  lv_obj_set_style_radius(panel, 20, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  return panel;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, uint32_t bg, lv_event_cb_t cb, void *user_data) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 170, 48);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return btn;
}

static void hide_cb(lv_event_t *e) {
  lv_obj_t *panel = (lv_obj_t *) lv_event_get_user_data(e);
  if (panel != NULL) lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

// ---- web page panel -------------------------------------------------------------

static void refresh_web_panel(void) {
  char ip_url[48];
  char local_url[64];
  discovery_local_url(local_url, sizeof(local_url));
  bool connected = discovery_ip_url(ip_url, sizeof(ip_url));

  // The IP address works on every phone; the .local name not on all of them.
  const char *qr_text = connected ? ip_url : local_url;
#if HAVE_QRCODE
  lv_qrcode_update(s_qr, qr_text, strlen(qr_text));
#else
  (void) qr_text;
#endif

  lv_label_set_text(s_ip_label, connected ? ip_url : tr("Not connected to Wi-Fi"));
  lv_label_set_text(s_local_label, local_url);
}

void show_web_page_panel(void) {
  if (s_web_panel == NULL) return;
  refresh_web_panel();
  lv_obj_clear_flag(s_web_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_web_panel);
}

static void create_web_panel(lv_obj_t *parent) {
  s_web_panel = make_panel(parent, 520, 540);

  lv_obj_t *title = make_label(s_web_panel, &lv_font_montserrat_24, lv_color_white());
  lv_label_set_text(title, tr("Web Page"));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  s_hint_label = make_label(s_web_panel, &lv_font_montserrat_18, lv_color_hex(0xb0bec5));
  lv_label_set_text(s_hint_label, tr("Scan with your phone camera"));
  lv_obj_align(s_hint_label, LV_ALIGN_TOP_MID, 0, 46);

#if HAVE_QRCODE
  s_qr = lv_qrcode_create(s_web_panel);
  lv_qrcode_set_size(s_qr, QR_SIZE);
  lv_qrcode_set_dark_color(s_qr, lv_color_black());
  lv_qrcode_set_light_color(s_qr, lv_color_white());
  // white quiet zone around the code, scanners need it
  lv_obj_set_style_border_color(s_qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(s_qr, 10, 0);
  lv_obj_align(s_qr, LV_ALIGN_TOP_MID, 0, 80);
#else
  lv_label_set_text(s_hint_label, tr("Type this address in your browser"));
#endif

  s_ip_label = make_label(s_web_panel, &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(s_ip_label, LV_ALIGN_TOP_MID, 0, 330);

  lv_obj_t *or_label = make_label(s_web_panel, &lv_font_montserrat_18, lv_color_hex(0xb0bec5));
  lv_label_set_text(or_label, tr("or"));
  lv_obj_align(or_label, LV_ALIGN_TOP_MID, 0, 366);

  s_local_label = make_label(s_web_panel, &lv_font_montserrat_18, lv_color_white());
  lv_obj_align(s_local_label, LV_ALIGN_TOP_MID, 0, 392);

  lv_obj_t *close = make_button(s_web_panel, tr("Close"), 0x37474f, hide_cb, s_web_panel);
  lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, -12);
}

// ---- factory reset panel ----------------------------------------------------------

static void erase_cb(lv_event_t *e) {
  (void) e;
  lv_label_set_text(s_reset_text, tr("Erasing all settings...\nThe clock restarts and opens the setup."));
  lv_obj_add_flag(s_reset_buttons, LV_OBJ_FLAG_HIDDEN);
  device_factory_reset_async(800);   // let the message draw first
}

void show_factory_reset_panel(void) {
  if (s_reset_panel == NULL) return;
  lv_label_set_text(s_reset_text,
    tr("Erase all settings?\nWi-Fi, location, radio stations, alarm and messages are removed. "
       "The clock restarts and opens the setup."));
  lv_obj_clear_flag(s_reset_buttons, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_reset_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_reset_panel);
}

static void create_reset_panel(lv_obj_t *parent) {
  s_reset_panel = make_panel(parent, 500, 360);

  lv_obj_t *title = make_label(s_reset_panel, &lv_font_montserrat_24, lv_color_white());
  lv_label_set_text(title, tr("Factory Reset"));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  s_reset_text = make_label(s_reset_panel, &lv_font_montserrat_18, lv_color_white());
  lv_label_set_long_mode(s_reset_text, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_reset_text, 420);
  lv_obj_align(s_reset_text, LV_ALIGN_TOP_MID, 0, 64);

  s_reset_buttons = lv_obj_create(s_reset_panel);
  lv_obj_remove_style_all(s_reset_buttons);
  lv_obj_set_size(s_reset_buttons, 420, 60);
  lv_obj_align(s_reset_buttons, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_obj_t *cancel = make_button(s_reset_buttons, tr("Cancel"), 0x37474f, hide_cb, s_reset_panel);
  lv_obj_align(cancel, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t *erase = make_button(s_reset_buttons, tr("Erase"), DANGER, erase_cb, NULL);
  lv_obj_align(erase, LV_ALIGN_RIGHT_MID, -10, 0);
}

void create_device_panels(lv_obj_t *parent) {
  create_web_panel(parent);
  create_reset_panel(parent);
}
