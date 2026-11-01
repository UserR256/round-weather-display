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

// Once a minute: current internal/DMA free plus the LOWEST DMA free since boot
// ("min"). The min is the number that matters: if it stays well above ~15-20 KB
// while the radio plays and the weather refreshes, there is enough margin.
// grep the log for "heap: periodic".
static inline void heap_diag_periodic(uint32_t uptime_seconds, const char *radio_state)
{
  if (uptime_seconds == 0 || uptime_seconds % 60 != 0) {
    return;
  }
  ESP_LOGI(
    "heap",
    "periodic up %lus radio=%s | internal free %u (min %u) | DMA free %u (min %u, largest %u)",
    (unsigned long) uptime_seconds,
    radio_state != NULL ? radio_state : "?",
    (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
    (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
  );
}
