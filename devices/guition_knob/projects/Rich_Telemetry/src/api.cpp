#include "api.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <string.h>
#include "config.h"
#include "nav_input.h"
#include "view.h"
#include "persist.h"
#include "secret_store.h"
#include "freertos/semphr.h"

extern String g_layout_json;
extern SemaphoreHandle_t g_ctx_mutex;

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

static void h_set_context() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    dash_set_context(D, S->arg("plain").c_str(), millis());
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
    S->send(200, "application/json", "{\"ok\":true}\n");
}

static void h_set_secrets() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    if (!secret_store_merge(S->arg("plain").c_str())) { S->send(400, "text/plain", "Invalid JSON\n"); return; }
    S->send(200, "application/json", "{\"ok\":true}\n");   // ne renvoie JAMAIS le contenu
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
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    JsonArray arr = doc["sources"].to<JsonArray>();
    for (int i = 0; i < D->source_count; i++) {
        JsonObject o     = arr.add<JsonObject>();
        o["name"]        = D->sources[i].name;          // char[] -> ArduinoJson copie
        o["last_status"] = D->sources[i].last_status;
        o["err_count"]   = D->sources[i].err_count;
        o["updated_at"]  = D->sources[i].updated_at;
    }
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
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

static void h_set_layout() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    String body = S->arg("plain");
    char err[80];
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    bool ok = dash_set_layout(D, body.c_str(), err, sizeof(err));
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
    if (!ok) {
        S->send(400, "application/json", String("{\"ok\":false,\"error\":\"") + err + "\"}\n");
        return;
    }
    g_layout_json = body;
    if (!persist_save(g_layout_json)) { S->send(500, "text/plain", "FS write failed\n"); return; }
    S->send(200, "application/json", "{\"ok\":true}\n");
}

static void h_get_layout() {
    S->send(200, "application/json", g_layout_json.length() ? g_layout_json : String("{}"));
}

static void h_page() {
    JsonDocument doc;
    if (!S->hasArg("plain") || deserializeJson(doc, S->arg("plain"))) {
        S->send(400, "text/plain", "Invalid JSON\n"); return;
    }
    if (doc["dir"].is<const char*>()) {
        nav_goto_dir(D, strcmp(doc["dir"], "prev") == 0 ? -1 : +1);
    } else if (doc["index"].is<int>()) {
        int idx = doc["index"];
        if (idx < 0 || idx >= D->page_count) {
            S->send(404, "text/plain", "page index out of range\n"); return;
        }
        view_show_page(D, idx);
    } else if (doc["name"].is<const char*>()) {
        const char* nm = doc["name"];
        int found = -1;
        for (int p = 0; p < D->page_count; p++)
            if (strcmp(D->pages[p].name, nm) == 0) { found = p; break; }
        if (found < 0) { S->send(404, "text/plain", "page name not found\n"); return; }
        view_show_page(D, found);
    }
    JsonDocument res; res["page"] = D->active_page;
    res["name"] = D->pages[D->active_page].name;
    String out; serializeJson(res, out); out += "\n";
    S->send(200, "application/json", out);
}

void api_register(WebServer& server, Dashboard* d) {
    S = &server; D = d;
    server.enableCORS(true);   // Allow-Origin/Methods/Headers: * sur toutes les réponses (outil de dev LAN mono-utilisateur)
    server.on("/update", HTTP_POST, h_update);
    server.on("/context", HTTP_POST, h_set_context);
    server.on("/secrets", HTTP_POST, h_set_secrets);   // pas de route GET : write-only par conception
    server.on("/status", HTTP_GET,  h_status);
    server.on("/layout", HTTP_POST, h_set_layout);
    server.on("/layout", HTTP_GET,  h_get_layout);
    server.on("/page",   HTTP_POST, h_page);
    server.on("/",       HTTP_GET,  h_root);
    server.onNotFound([](){
        // enableCORS(true) ajoute déjà Allow-Origin/Methods/Headers: * à chaque réponse ;
        // le preflight OPTIONS a juste besoin d'un statut 2xx (sinon le navigateur le rejette).
        if (S->method() == HTTP_OPTIONS) S->send(204);
        else                             S->send(404, "text/plain", "Not found\n");
    });
}
