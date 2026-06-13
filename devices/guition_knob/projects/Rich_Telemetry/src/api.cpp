#include "api.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "config.h"

static Dashboard* D = nullptr;
static WebServer* S = nullptr;

static void h_update() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    char unk[UNKNOWN_CSV_LEN];
    int n = dash_apply_update(D, S->arg("plain").c_str(), unk, sizeof(unk));
    if (n < 0) { S->send(400, "text/plain", "Invalid JSON\n"); return; }
    JsonDocument res; res["ok"] = true; res["updated"] = n;
    if (unk[0]) res["unknown"] = unk;
    String out; serializeJson(res, out); out += "\n";
    S->send(200, "application/json", out);
}

static void h_status() {
    JsonDocument doc;
    doc["ip"]         = WiFi.localIP().toString();
    doc["hostname"]   = String(MDNS_HOST) + ".local";
    doc["rssi"]       = WiFi.RSSI();
    doc["uptime_s"]   = (uint32_t)(millis() / 1000);
    doc["page"]       = D->active_page;
    doc["pages"]      = D->page_count;
    doc["components"] = D->comp_count;
    String out; serializeJson(doc, out); out += "\n";
    S->send(200, "application/json", out);
}

static void h_root() {
    String ip = WiFi.localIP().toString();
    String html =
        "<!doctype html><meta charset=utf-8><title>Rich_Telemetry</title>"
        "<h2>Guition K718 - Rich_Telemetry</h2>"
        "<p>POST /update (valeurs partielles), POST /layout, POST /page.</p>"
        "<pre>curl -X POST http://" + ip + "/update -H 'Content-Type: application/json' \\\n"
        "  -d '{\"w5h\":{\"pct\":63,\"reset_in_s\":6600}}'</pre>"
        "<p><a href=/status>/status</a> &middot; <a href=/layout>/layout</a></p>";
    S->send(200, "text/html", html);
}

void api_register(WebServer& server, Dashboard* d) {
    S = &server; D = d;
    server.on("/update", HTTP_POST, h_update);
    server.on("/status", HTTP_GET,  h_status);
    server.on("/",       HTTP_GET,  h_root);
    server.onNotFound([](){ S->send(404, "text/plain", "Not found\n"); });
}
