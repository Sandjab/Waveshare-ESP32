#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "guition_lvgl.h"
#include "config.h"
#include "secrets.h"
#include "view.h"

static WebServer server(HTTP_PORT);
static Dashboard g_dash;
static bool g_wifi_up = false;

static bool wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_BOOT_TIMEOUT_MS) {
        delay(200); Serial.print("."); lv_timer_handler();
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

static void start_services() {
    static bool started = false;
    if (started) return;
    started = true;
    if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", HTTP_PORT);
    server.begin();
    Serial.printf("[http] :%d  http://%s.local\n", HTTP_PORT, MDNS_HOST);
}

void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\nGuition JC3636K718 - Rich_Telemetry");
    guition_lvgl_init();
    lv_timer_handler();
    char err[80];
    dash_set_layout(&g_dash, view_default_layout(), err, sizeof(err));
    view_rebuild(&g_dash);
    g_wifi_up = wifi_connect();
    if (g_wifi_up) {
        Serial.printf("[wifi] IP=%s\n", WiFi.localIP().toString().c_str());
        start_services();
    } else {
        Serial.println("[wifi] ECHEC (verifie secrets.h)");
    }
}

void loop() {
    server.handleClient();
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        last = millis();
        bool now = (WiFi.status() == WL_CONNECTED);
        if (now && !g_wifi_up) start_services();
        g_wifi_up = now;
    }
    if (g_dash.layout_dirty) view_rebuild(&g_dash);
    if (g_dash.values_dirty) view_sync(&g_dash);
    lv_timer_handler();
    delay(5);
}
