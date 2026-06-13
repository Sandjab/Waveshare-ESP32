#include "dashboard.h"
#include "color.h"
#include "format.h"
#include <ArduinoJson.h>
#include <string.h>

int dash_find(const Dashboard* d, const char* id) {
    for (int i = 0; i < d->comp_count; i++)
        if (strncmp(d->components[i].id, id, ID_LEN) == 0) return i;
    return -1;
}

static CompType parse_type(const char* s) {
    if (!s) return COMP_NONE;
    if (!strcmp(s,"label"))    return COMP_LABEL;
    if (!strcmp(s,"readout"))  return COMP_READOUT;
    if (!strcmp(s,"bar"))      return COMP_BAR;
    if (!strcmp(s,"ring"))     return COMP_RING;
    if (!strcmp(s,"led_ring")) return COMP_LED_RING;
    if (!strcmp(s,"sound"))    return COMP_SOUND;
    return COMP_NONE;
}

static Anchor parse_anchor(const char* s) {
    if (!s) return A_CENTER;
    if (!strcmp(s,"TOP_MID"))      return A_TOP_MID;
    if (!strcmp(s,"BOTTOM_MID"))   return A_BOTTOM_MID;
    if (!strcmp(s,"LEFT_MID"))     return A_LEFT_MID;
    if (!strcmp(s,"RIGHT_MID"))    return A_RIGHT_MID;
    if (!strcmp(s,"TOP_LEFT"))     return A_TOP_LEFT;
    if (!strcmp(s,"TOP_RIGHT"))    return A_TOP_RIGHT;
    if (!strcmp(s,"BOTTOM_LEFT"))  return A_BOTTOM_LEFT;
    if (!strcmp(s,"BOTTOM_RIGHT")) return A_BOTTOM_RIGHT;
    return A_CENTER;
}

bool dash_set_layout(Dashboard* d, const char* json, char* err, size_t errn) {
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, json);
    if (e) { snprintf(err, errn, "JSON: %s", e.c_str()); return false; }

    Dashboard t{};
    strlcpy(t.title, doc["title"] | "", sizeof(t.title));
    t.background = parse_hex_color(doc["background"] | "#000000", 0x000000);
    t.nav_wrap   = doc["nav"]["wrap"] | true;

    JsonObjectConst comps = doc["components"].as<JsonObjectConst>();
    if (comps.isNull()) { snprintf(err, errn, "components manquant"); return false; }
    for (JsonPairConst kv : comps) {
        if (t.comp_count >= MAX_COMPONENTS) { snprintf(err, errn, "trop de composants"); return false; }
        Component& c = t.components[t.comp_count];
        strlcpy(c.id, kv.key().c_str(), sizeof(c.id));
        JsonObjectConst o = kv.value().as<JsonObjectConst>();
        c.type = parse_type(o["type"] | "");
        if (c.type == COMP_NONE) { snprintf(err, errn, "type inconnu pour '%s'", c.id); return false; }
        strlcpy(c.label, o["label"] | "", sizeof(c.label));
        strlcpy(c.unit,  o["unit"]  | "", sizeof(c.unit));
        strlcpy(c.text,  o["text"]  | "", sizeof(c.text));
        strlcpy(c.vstr,  o["text"]  | "", sizeof(c.vstr));
        c.color       = parse_hex_color(o["color"] | "#FFFFFF", 0xFFFFFF);
        c.vmin        = o["min"] | 0;
        c.vmax        = o["max"] | 100;
        c.pill        = o["pill"] | false;
        c.center_pct  = o["center_pct"] | false;
        c.countdown   = o["countdown"] | false;
        c.font        = o["font"] | 20;
        c.led_brightness_cfg = o["brightness"] | 64;
        JsonArrayConst th = o["thresholds"].as<JsonArrayConst>();
        for (JsonArrayConst pair : th) {
            if (c.threshold_count >= MAX_THRESHOLDS) break;
            c.thresholds[c.threshold_count].limit = pair[0].as<float>();
            c.thresholds[c.threshold_count].color = parse_hex_color(pair[1] | "#FFFFFF", 0xFFFFFF);
            c.threshold_count++;
        }
        t.comp_count++;
    }

    JsonArrayConst pages = doc["pages"].as<JsonArrayConst>();
    for (JsonObjectConst pg : pages) {
        if (t.page_count >= MAX_PAGES) { snprintf(err, errn, "trop de pages"); return false; }
        Page& p = t.pages[t.page_count];
        strlcpy(p.name, pg["name"] | "", sizeof(p.name));
        for (JsonObjectConst pl : pg["place"].as<JsonArrayConst>()) {
            if (p.place_count >= MAX_PLACEMENTS_PER_PAGE) { snprintf(err, errn, "trop de placements"); return false; }
            const char* ref = pl["ref"] | "";
            int ci = dash_find(&t, ref);
            if (ci < 0) { snprintf(err, errn, "ref inconnue '%s'", ref); return false; }
            Placement& q = p.places[p.place_count];
            q.comp_index  = ci;
            q.anchor      = parse_anchor(pl["anchor"] | "CENTER");
            q.dx          = pl["dx"] | 0;       q.dy     = pl["dy"] | 0;
            q.width       = pl["width"] | 0;    q.height = pl["height"] | 0;
            q.radius      = pl["radius"] | 0;   q.thickness = pl["thickness"] | 16;
            q.gap_deg     = pl["gap_deg"] | 70; q.start_angle = pl["start_angle"] | 0;
            p.place_count++;
        }
        t.page_count++;
    }

    t.active_page  = 0;
    t.layout_dirty = true;
    *d = t;
    return true;
}

static void apply_one(Component& c, JsonVariantConst v) {
    switch (c.type) {
        case COMP_LABEL:
            strlcpy(c.vstr, v.as<const char*>() ? v.as<const char*>() : c.vstr, sizeof(c.vstr));
            break;
        case COMP_READOUT:
            if (v.is<const char*>()) strlcpy(c.vstr, v.as<const char*>(), sizeof(c.vstr));
            else format_value(v.as<double>(), c.unit, c.vstr, sizeof(c.vstr));
            break;
        case COMP_BAR:
            c.value = v.as<int>();
            break;
        case COMP_RING:
            c.value      = v["pct"] | c.value;
            c.reset_in_s = v["reset_in_s"] | c.reset_in_s;
            if (v["caption"].is<const char*>()) {
                strlcpy(c.caption, v["caption"].as<const char*>(), sizeof(c.caption));
            } else if (c.countdown) {
                format_remaining(c.reset_in_s, c.caption, sizeof(c.caption));
            }
            break;
        case COMP_LED_RING: {
            const char* m = v["mode"] | "";
            if      (!strcmp(m,"off"))      c.led_mode = LED_OFF;
            else if (!strcmp(m,"solid"))    c.led_mode = LED_SOLID;
            else if (!strcmp(m,"progress")) c.led_mode = LED_PROGRESS;
            else if (!strcmp(m,"spinner"))  c.led_mode = LED_SPINNER;
            else if (!strcmp(m,"blink"))    c.led_mode = LED_BLINK;
            else if (!strcmp(m,"breathe"))  c.led_mode = LED_BREATHE;
            if (v["color"].is<const char*>()) c.led_color = parse_hex_color(v["color"], c.led_color);
            c.led_value      = v["value"]      | c.led_value;
            c.led_brightness = v["brightness"] | c.led_brightness_cfg;
            c.led_period_ms  = v["period_ms"]  | (c.led_period_ms ? c.led_period_ms : 1000);
            break;
        }
        case COMP_SOUND:
            c.snd_pending = true;
            c.snd_tone = v["tone"] | 0;
            c.snd_ms   = v["ms"]   | 150;
            strlcpy(c.snd_name, v["name"] | "", sizeof(c.snd_name));
            break;
        default: break;
    }
}

int dash_apply_update(Dashboard* d, const char* json, char* unknown_csv, size_t n) {
    unknown_csv[0] = '\0';
    JsonDocument doc;
    if (deserializeJson(doc, json)) return -1;
    int updated = 0;
    for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
        int ci = dash_find(d, kv.key().c_str());
        if (ci < 0) {
            size_t len = strlen(unknown_csv);
            snprintf(unknown_csv + len, n - len, "%s%s", len ? "," : "", kv.key().c_str());
            continue;
        }
        apply_one(d->components[ci], kv.value());
        d->components[ci].dirty = true;
        d->values_dirty = true;
        updated++;
    }
    return updated;
}

void dash_tick_countdown(Dashboard* d, uint32_t elapsed_s) {
    for (int i = 0; i < d->comp_count; i++) {
        Component& c = d->components[i];
        if (c.type != COMP_RING || !c.countdown) continue;
        if (c.reset_in_s == 0) continue;
        c.reset_in_s = (c.reset_in_s > elapsed_s) ? c.reset_in_s - elapsed_s : 0;
        format_remaining(c.reset_in_s, c.caption, sizeof(c.caption));
        c.dirty = true;
        d->values_dirty = true;
    }
}
