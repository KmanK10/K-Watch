#pragma once

// Recolours everything on screen in shades of one colour, for sleep mode at night.

#include <stdint.h>

typedef enum {
    TINT_NONE,
    TINT_RED,
    TINT_GREEN,
} tint_t;

// Redraws the whole screen when the tint changes.
void tint_set(tint_t tint);

// Called by the display driver on each block of RGB565 pixels just before it goes to the panel.
void tint_apply(uint16_t *px, uint32_t count);
