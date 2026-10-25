#pragma once

/* Minimal UI translation: English text stays in the code and tr() swaps it for
 * the Dutch version when that language is active. Unknown strings fall back to
 * the English original, so a missing translation never shows an empty label.
 *
 * The language is read once at boot (device_config.language) and changes after
 * "Save & Restart", so labels built at start-up need no re-translation. */

typedef enum {
  I18N_EN = 0,
  I18N_NL = 1,
} i18n_lang_t;

/* Accepts "en" or "nl" (case-insensitive); anything else selects English. */
void i18n_set_language_code(const char *code);
i18n_lang_t i18n_language(void);

/* Returns the translation of `en`, or `en` itself in English / when unknown.
 * The result is a pointer to static storage (or `en`), never to be freed. */
const char *tr(const char *en);
