#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "guition_lvgl.h"
#include "secrets.h"   // WIFI_SSID / WIFI_PASS (fichier local, gitignore)

// Petit serveur REST de telemetry. Le device se connecte au WiFi (STA), expose
// POST /telemetry et affiche les champs recus en LVGL sur l'ecran rond 360x360.
// Mono-thread : server.handleClient() et lv_timer_handler() tournent tous deux
// dans loop(), donc pas de souci de thread-safety LVGL. Les handlers HTTP se
// contentent de stocker l'etat + lever un flag ; le rendu est defere au loop().
//
// Limite police : les fonts Montserrat de LVGL ne couvrent que l'ASCII. Les
// labels/unites a accents ou symboles (e, degre...) ne s'afficheront pas.

static constexpr int  HTTP_PORT            = 80;
static constexpr char MDNS_HOST[]          = "guition";   // -> guition.local
static constexpr int  MAX_FIELDS           = 6;
static constexpr int  WIFI_BOOT_TIMEOUT_MS = 20000;

// ---- Etat telemetry (ecrit par le handler HTTP, lu par loop ; meme thread) ----
struct Field {
    char label[24];
    char value[24];
    char unit[12];
};
static char  g_title[32]   = "Telemetry";
static Field g_fields[MAX_FIELDS];
static int   g_field_count = 0;
static bool  g_dirty       = false;   // donnee fraiche a rendre
static bool  g_have_data   = false;   // au moins une trame recue
static bool  g_wifi_up     = false;

static WebServer              server(HTTP_PORT);
static esp_lcd_panel_handle_t panel;

// ---- Objets LVGL ----
static lv_obj_t *title_label;
static lv_obj_t *status_label;            // gros message centre (etats sans data)
static lv_obj_t *list_cont;               // conteneur des lignes
static lv_obj_t *row_obj[MAX_FIELDS];     // une ligne = label gauche / valeur droite
static lv_obj_t *row_label[MAX_FIELDS];
static lv_obj_t *row_value[MAX_FIELDS];
static lv_obj_t *footer_label;

// =================== UI ===================

static void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0B0F), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    title_label = lv_label_create(scr);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xF5F5F5), 0);
    lv_label_set_text(title_label, g_title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 46);

    list_cont = lv_obj_create(scr);
    lv_obj_remove_style_all(list_cont);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(list_cont, 240, 200);
    lv_obj_center(list_cont);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list_cont, 10, 0);

    for (int i = 0; i < MAX_FIELDS; i++) {
        row_obj[i] = lv_obj_create(list_cont);
        lv_obj_remove_style_all(row_obj[i]);
        lv_obj_clear_flag(row_obj[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(row_obj[i], lv_pct(100));
        lv_obj_set_height(row_obj[i], LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row_obj[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_obj[i], LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        row_label[i] = lv_label_create(row_obj[i]);
        lv_obj_set_style_text_font(row_label[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(row_label[i], lv_color_hex(0x9AA0AA), 0);
        lv_label_set_text(row_label[i], "");

        row_value[i] = lv_label_create(row_obj[i]);
        lv_obj_set_style_text_font(row_value[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(row_value[i], lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(row_value[i], "");
    }

    status_label = lv_label_create(scr);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x9AA0AA), 0);
    lv_label_set_text(status_label, "Connexion WiFi...");
    lv_obj_center(status_label);

    footer_label = lv_label_create(scr);
    lv_obj_set_style_text_font(footer_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(footer_label, lv_color_hex(0x6B7280), 0);
    lv_label_set_text(footer_label, "");
    lv_obj_align(footer_label, LV_ALIGN_BOTTOM_MID, 0, -46);
}

// Affiche un message central, masque la liste.
static void ui_show_status(const char *msg) {
    lv_label_set_text(status_label, msg);
    lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(list_cont, LV_OBJ_FLAG_HIDDEN);
}

// Affiche le dashboard a partir de l'etat g_*.
static void ui_render_telemetry() {
    lv_label_set_text(title_label, g_title);
    for (int i = 0; i < MAX_FIELDS; i++) {
        if (i < g_field_count) {
            lv_label_set_text(row_label[i], g_fields[i].label);
            char vb[40];
            if (g_fields[i].unit[0])
                snprintf(vb, sizeof(vb), "%s %s", g_fields[i].value, g_fields[i].unit);
            else
                snprintf(vb, sizeof(vb), "%s", g_fields[i].value);
            lv_label_set_text(row_value[i], vb);
            lv_obj_clear_flag(row_obj[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(row_obj[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_HIDDEN);
}

static void ui_update_footer() {
    char fb[64];
    snprintf(fb, sizeof(fb), "%s.local | %s",
             MDNS_HOST, WiFi.localIP().toString().c_str());
    lv_label_set_text(footer_label, fb);
}

// =================== HTTP ===================

// Convertit la valeur JSON (nombre ou chaine) en texte affichable. Tout ce qui
// n'est pas une chaine est lu en double : un entier 42 -> "42", un float 9.2 ->
// "9.2" (evite de dependre de la semantique exacte de is<float>()).
static void format_value(JsonVariantConst v, char *out, size_t n) {
    if (v.is<const char *>()) {
        strlcpy(out, v.as<const char *>(), n);
    } else if (v.isNull()) {
        strlcpy(out, "", n);
    } else {
        double d = v.as<double>();
        if (d == (long long)d)
            snprintf(out, n, "%lld", (long long)d);
        else
            snprintf(out, n, "%.1f", d);
    }
}

static void handle_post_telemetry() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Empty body. Expected JSON.\n");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Invalid JSON: %s\n", err.c_str());
        server.send(400, "text/plain", msg);
        return;
    }

    JsonArrayConst fields = doc["fields"].as<JsonArrayConst>();
    if (fields.isNull()) {
        server.send(400, "text/plain", "Missing \"fields\" array.\n");
        return;
    }

    strlcpy(g_title, doc["title"] | "Telemetry", sizeof(g_title));

    int  n         = 0;
    bool truncated = false;
    for (JsonVariantConst fv : fields) {
        if (n >= MAX_FIELDS) { truncated = true; break; }
        JsonObjectConst f = fv.as<JsonObjectConst>();
        strlcpy(g_fields[n].label, f["label"] | "", sizeof(g_fields[n].label));
        format_value(f["value"], g_fields[n].value, sizeof(g_fields[n].value));
        strlcpy(g_fields[n].unit,  f["unit"]  | "", sizeof(g_fields[n].unit));
        n++;
    }

    g_field_count = n;
    g_have_data   = true;
    g_dirty       = true;

    if (truncated)
        Serial.printf("[telemetry] >%d champs, tronque\n", MAX_FIELDS);

    server.send(200, "application/json", "{\"ok\":true}\n");
}

static void handle_get_status() {
    JsonDocument doc;
    doc["ip"]        = WiFi.localIP().toString();
    doc["hostname"]  = String(MDNS_HOST) + ".local";
    doc["rssi"]      = WiFi.RSSI();
    doc["uptime_s"]  = (uint32_t)(millis() / 1000);
    doc["fields"]    = g_field_count;
    doc["have_data"] = g_have_data;

    String out;
    serializeJson(doc, out);
    out += "\n";
    server.send(200, "application/json", out);
}

static void handle_get_root() {
    String ip = WiFi.localIP().toString();
    String html =
        "<!doctype html><meta charset=utf-8><title>Guition Telemetry</title>"
        "<h2>Guition K718 - Telemetry</h2>"
        "<p>POST JSON sur <code>/telemetry</code> :</p>"
        "<pre>curl -X POST http://" + ip + "/telemetry \\\n"
        "  -H 'Content-Type: application/json' \\\n"
        "  -d '{\"title\":\"Mac\",\"fields\":["
        "{\"label\":\"CPU\",\"value\":42,\"unit\":\"%\"}]}'</pre>"
        "<p><a href=/status>/status</a></p>";
    server.send(200, "text/html", html);
}

// Demarre mDNS + routes HTTP une seule fois (au 1er passage en ligne).
static void start_services() {
    static bool started = false;
    if (started) return;
    started = true;

    if (MDNS.begin(MDNS_HOST)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[mdns] http://%s.local\n", MDNS_HOST);
    } else {
        Serial.println("[mdns] echec demarrage");
    }

    server.on("/", HTTP_GET, handle_get_root);
    server.on("/telemetry", HTTP_POST, handle_post_telemetry);
    server.on("/status", HTTP_GET, handle_get_status);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found\n"); });
    server.begin();
    Serial.printf("[http] serveur sur :%d\n", HTTP_PORT);
}

// =================== WiFi ===================

static bool wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[wifi] connexion a %s", WIFI_SSID);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_BOOT_TIMEOUT_MS) {
        delay(200);
        Serial.print(".");
        lv_timer_handler();   // garde l'ecran rafraichi pendant l'attente
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

// =================== Arduino ===================

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nGuition JC3636K718 - Basic_WiFi_Telemetry");

    panel = guition_lvgl_init();
    build_ui();
    ui_show_status("Connexion WiFi...");
    lv_timer_handler();

    g_wifi_up = wifi_connect();
    if (g_wifi_up) {
        Serial.printf("[wifi] connecte, IP=%s\n", WiFi.localIP().toString().c_str());
        start_services();
        ui_update_footer();
        ui_show_status("En attente...");
    } else {
        Serial.println("[wifi] ECHEC (verifie secrets.h) - reconnexion auto en tache de fond");
        ui_show_status("WiFi: echec");
    }
}

void loop() {
    server.handleClient();

    if (g_dirty) {
        g_dirty = false;
        ui_render_telemetry();
    }

    // Surveillance WiFi a 1 Hz pour le feedback de (re)connexion.
    static uint32_t last_check = 0;
    if (millis() - last_check > 1000) {
        last_check = millis();
        bool now = (WiFi.status() == WL_CONNECTED);
        if (now != g_wifi_up) {
            g_wifi_up = now;
            if (now) {
                Serial.printf("[wifi] reconnecte, IP=%s\n",
                              WiFi.localIP().toString().c_str());
                start_services();   // no-op si deja demarre
                ui_update_footer();
                if (g_have_data) ui_render_telemetry();
                else             ui_show_status("En attente...");
            } else {
                Serial.println("[wifi] perdu, reconnexion...");
                ui_show_status("Reconnexion...");
            }
        }
    }

    lv_timer_handler();
    delay(5);
}
