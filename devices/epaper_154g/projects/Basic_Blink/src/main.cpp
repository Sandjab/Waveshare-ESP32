#include <Arduino.h>
#include "epaper154g_pins.h"

// Blink de la LED verte onboard (GPIO 3, active LOW).
// Premier projet de validation : pas d'e-paper (refresh 15-20 s, inadapté à un blink).

void setup() {
    Serial.begin(115200);
    Serial.println("Waveshare ESP32-S3-ePaper-1.54G — Basic_Blink");

    pinMode(PIN_LED_GREEN, OUTPUT);
}

void loop() {
    Serial.println("LED ON");
    digitalWrite(PIN_LED_GREEN, LOW);   // active LOW
    delay(500);

    Serial.println("LED OFF");
    digitalWrite(PIN_LED_GREEN, HIGH);
    delay(500);
}
