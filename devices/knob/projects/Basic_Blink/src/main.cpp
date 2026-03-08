#include <Arduino.h>
#include "knob_display.h"

#define STRIP_HEIGHT  36
#define STRIP_PIXELS  (LCD_H_RES * STRIP_HEIGHT)

// --- RGB565 byte-swapped colors (big-endian for ST77916) ---
#define COLOR_GREEN   0xE007
#define COLOR_RED     0x00F8

// --- Globals ---
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t *strip_buf = NULL;

void fill_screen(uint16_t color) {
    for (int i = 0; i < STRIP_PIXELS; i++) {
        strip_buf[i] = color;
    }
    for (int y = 0; y < LCD_V_RES; y += STRIP_HEIGHT) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + STRIP_HEIGHT, strip_buf);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Basic_Blink: Minimal Screen Blink");

    panel_handle = knob_display_init();

    // Allocate strip buffer in DMA-capable memory
    strip_buf = (uint16_t *)heap_caps_malloc(STRIP_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(strip_buf);

    Serial.println("Init complete. Starting blink loop.");
}

void loop() {
    Serial.println("GREEN");
    fill_screen(COLOR_GREEN);
    delay(1000);

    Serial.println("RED");
    fill_screen(COLOR_RED);
    delay(1000);
}
