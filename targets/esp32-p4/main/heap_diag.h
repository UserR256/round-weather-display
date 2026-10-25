#pragma once

#include "esp_heap_caps.h"
#include "esp_log.h"

// Logs the internal memory that DMA users (I2S, hardware AES for TLS) depend on.
// Cheap enough to leave in; grep the log for "heap".
static inline void heap_diag(const char *where)
{
  ESP_LOGI(
    "heap",
    "%-28s internal free %u, DMA free %u (largest %u), PSRAM free %u",
    where,
    (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
  );
}
