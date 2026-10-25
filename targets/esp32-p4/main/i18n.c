#include "i18n.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  const char *en;
  const char *nl;
} i18n_entry_t;

/* English -> Dutch. Format strings keep their conversions in the same order. */
static const i18n_entry_t NL_TABLE[] = {
#include "i18n_nl.inc"
};

static volatile i18n_lang_t s_language = I18N_EN;

void i18n_set_language_code(const char *code)
{
  s_language = (code != NULL
    && tolower((unsigned char) code[0]) == 'n'
    && tolower((unsigned char) code[1]) == 'l')
    ? I18N_NL
    : I18N_EN;
}

i18n_lang_t i18n_language(void)
{
  return s_language;
}

const char *tr(const char *en)
{
  if (en == NULL || s_language != I18N_NL) {
    return en;
  }

  for (size_t i = 0; i < sizeof(NL_TABLE) / sizeof(NL_TABLE[0]); ++i) {
    if (strcmp(NL_TABLE[i].en, en) == 0) {
      return NL_TABLE[i].nl;
    }
  }
  return en;
}
