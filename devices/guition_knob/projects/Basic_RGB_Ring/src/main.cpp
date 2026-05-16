#include <Arduino.h>
#include "rgb_ring.h"

// Rainbow rotation: each LED is offset by 1/13 of the hue wheel, the whole
// pattern slides 1 step per frame at ~50 fps. Full rotation ~5 s.

// Global ring instance (declared extern in rgb_ring.h).
Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    Serial.println("Guition JC3636K718 — Basic_RGB_Ring (rainbow rotation)");

    // 128/255 ≈ 50 % brightness. Sur une rotation HSV chaque LED n'active
    // qu'1-2 canaux à la fois → ~130 mA en pointe pour 13 LEDs, très loin
    // du budget 500 mA d'un port USB standard. Monter à 255 si alim externe.
    rgb_ring_init(128);
}

void loop() {
    static uint16_t hue = 0;
    for (uint8_t i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t pixel_hue = hue + (uint32_t)i * 65536u / RGB_RING_LED_COUNT;
        rgb_ring_set_hsv(i, pixel_hue);
    }
    rgb_ring_show();
    hue += 256;        // 65536 / 256 = 256 frames per full cycle
    delay(20);         // ~50 fps
}
