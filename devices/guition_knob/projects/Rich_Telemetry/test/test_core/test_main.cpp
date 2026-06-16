#include <unity.h>
#include <string.h>
#include "format.h"
#include "color.h"
#include "nav_logic.h"
#include "dashboard.h"
#include "context.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>

static char buf[32];

void test_remaining_seconds(void)  { format_remaining(45, buf, sizeof(buf));    TEST_ASSERT_EQUAL_STRING("45s",  buf); }
void test_remaining_min_boundary(void){ format_remaining(60, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("1m",   buf); }
void test_remaining_minutes(void)  { format_remaining(50*60, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("50m",  buf); }
void test_remaining_hour(void)     { format_remaining(3600, buf, sizeof(buf));  TEST_ASSERT_EQUAL_STRING("1h00", buf); }
void test_remaining_h_m(void)      { format_remaining(6600, buf, sizeof(buf));  TEST_ASSERT_EQUAL_STRING("1h50", buf); }
void test_remaining_days(void)     { format_remaining(453600, buf, sizeof(buf));TEST_ASSERT_EQUAL_STRING("5j6h", buf); }
void test_remaining_zero(void)     { format_remaining(0, buf, sizeof(buf));     TEST_ASSERT_EQUAL_STRING("0s",   buf); }

void test_value_unit(void)    { format_value(42, "%",  buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("42 %", buf); }
void test_value_float(void)   { format_value(9.2, "GB",buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("9.2 GB", buf); }
void test_value_no_unit(void) { format_value(42, "",   buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("42", buf); }

void test_hex_parse(void)     { TEST_ASSERT_EQUAL_HEX32(0x38BDF8, parse_hex_color("#38BDF8", 0)); }
void test_hex_no_hash(void)   { TEST_ASSERT_EQUAL_HEX32(0xA1B2C3, parse_hex_color("A1B2C3", 0)); }
void test_hex_fallback(void)  { TEST_ASSERT_EQUAL_HEX32(0x123456, parse_hex_color("nope", 0x123456)); }

void test_threshold_below(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0x22C55E, threshold_color(t,3,63,0x000000));
}
void test_threshold_mid(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0xF59E0B, threshold_color(t,3,85,0x000000));
}
void test_threshold_over(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0xEF4444, threshold_color(t,3,95,0x000000));
}
static const char* LAYOUT_OK =
  "{\"title\":\"T\",\"background\":\"#0B0B0F\",\"nav\":{\"wrap\":true},"
  "\"components\":{"
    "\"w5h\":{\"type\":\"ring\",\"color\":\"#38BDF8\",\"countdown\":true,"
             "\"thresholds\":[[70,\"#22C55E\"],[90,\"#F59E0B\"]]},"
    "\"cpu\":{\"type\":\"readout\",\"label\":\"CPU\",\"unit\":\"%\"}},"
  "\"pages\":[{\"name\":\"usage\",\"place\":["
    "{\"ref\":\"w5h\",\"radius\":140,\"thickness\":16,\"gap_deg\":70},"
    "{\"ref\":\"cpu\",\"anchor\":\"CENTER\"}]}]}";

void test_layout_parse_counts(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_OK, err, sizeof(err)));
    TEST_ASSERT_EQUAL_INT(2, d.comp_count);
    TEST_ASSERT_EQUAL_INT(1, d.page_count);
    TEST_ASSERT_EQUAL_INT(2, d.pages[0].place_count);
    TEST_ASSERT_TRUE(d.nav_wrap);
}
void test_layout_types_and_geom(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int iw = dash_find(&d, "w5h");
    TEST_ASSERT_EQUAL_INT(COMP_RING, d.components[iw].type);
    TEST_ASSERT_TRUE(d.components[iw].countdown);
    TEST_ASSERT_EQUAL_INT(2, d.components[iw].threshold_count);
    TEST_ASSERT_EQUAL_HEX32(0x38BDF8, d.components[iw].color);
    TEST_ASSERT_EQUAL_INT(140, d.pages[0].places[0].radius);
    TEST_ASSERT_EQUAL_INT(A_CENTER, d.pages[0].places[1].anchor);
}
static const char* LAYOUT_RING_OPTS =
  "{\"title\":\"T\",\"background\":\"#000000\","
  "\"components\":{\"g\":{\"type\":\"ring\",\"color\":\"#38BDF8\","
                        "\"center_pct\":true,\"unit\":\"C\"}},"
  "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"g\","
             "\"radius\":140,\"thickness\":16,\"gap_deg\":70,\"start_angle\":90}]}]}";

void test_ring_center_pct_parsed(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_RING_OPTS, err, sizeof(err)));
    int ig = dash_find(&d, "g");
    TEST_ASSERT_TRUE(d.components[ig].center_pct);
    TEST_ASSERT_EQUAL_STRING("C", d.components[ig].unit);
}
void test_ring_start_angle_parsed(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_RING_OPTS, err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(90, d.pages[0].places[0].start_angle);
}
void test_ring_start_angle_default_zero(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));   // LAYOUT_OK ne définit pas start_angle
    TEST_ASSERT_EQUAL_INT(0, d.pages[0].places[0].start_angle);
}
static const char* LAYOUT_RING_CCOL =
  "{\"title\":\"T\",\"background\":\"#000000\","
  "\"components\":{\"g\":{\"type\":\"ring\",\"color\":\"#38BDF8\","
                        "\"center_pct\":true,\"center_color\":\"#FF0000\"}},"
  "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"g\",\"radius\":140}]}]}";

void test_ring_center_color_set(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_RING_CCOL, err, sizeof(err)));
    int ig = dash_find(&d, "g");
    TEST_ASSERT_TRUE(d.components[ig].center_color_set);
    TEST_ASSERT_EQUAL_HEX32(0xFF0000, d.components[ig].center_color);
}
void test_ring_center_color_defaults_to_color(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_RING_OPTS, err, sizeof(err));   // pas de center_color
    int ig = dash_find(&d, "g");
    TEST_ASSERT_FALSE(d.components[ig].center_color_set);
    TEST_ASSERT_EQUAL_HEX32(0x38BDF8, d.components[ig].center_color);  // retombe sur color
}
void test_layout_unknown_type_rejected(void) {
    Dashboard d{}; char err[80];
    const char* bad = "{\"components\":{\"x\":{\"type\":\"frobnicator\"}},\"pages\":[]}";
    TEST_ASSERT_FALSE(dash_set_layout(&d, bad, err, sizeof(err)));
}

// Conformité firmware ↔ schema : pour CHAQUE type déclaré dans le schema partagé
// (component.oneOf → comp_* → type.const), parse_type (via dash_set_layout) doit le
// résoudre ; un type absent du schema doit être rejeté. Échoue rouge si le firmware
// oublie un type que le schema déclare. Le schema est lu depuis RT_SCHEMA_PATH.
void test_schema_types_all_resolve(void) {
    FILE* f = fopen(RT_SCHEMA_PATH, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "impossible d'ouvrir RT_SCHEMA_PATH: " RT_SCHEMA_PATH);
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* schema = (char*)malloc((size_t)n + 1);
    size_t rd = fread(schema, 1, (size_t)n, f); schema[rd] = '\0';
    fclose(f);

    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, schema);
    TEST_ASSERT_TRUE_MESSAGE(!e, "schema JSON invalide");

    JsonArrayConst oneOf = doc["$defs"]["component"]["oneOf"].as<JsonArrayConst>();
    TEST_ASSERT_FALSE_MESSAGE(oneOf.isNull(), "component.oneOf absent du schema");

    int count = 0;
    for (JsonObjectConst ref : oneOf) {
        const char* r = ref["$ref"];                       // ex "#/$defs/comp_ring"
        TEST_ASSERT_NOT_NULL_MESSAGE(r, "entree oneOf sans $ref");
        const char* slash = strrchr(r, '/');
        TEST_ASSERT_NOT_NULL(slash);
        const char* defName = slash + 1;                   // "comp_ring"
        const char* typeName = doc["$defs"][defName]["properties"]["type"]["const"];
        TEST_ASSERT_NOT_NULL_MESSAGE(typeName, defName);

        char layout[192];
        snprintf(layout, sizeof(layout),
            "{\"components\":{\"x\":{\"type\":\"%s\"}},\"pages\":[]}", typeName);
        Dashboard d{}; char err[80];
        TEST_ASSERT_TRUE_MESSAGE(dash_set_layout(&d, layout, err, sizeof(err)), typeName);
        count++;
    }
    free(schema);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count, "aucun type extrait du schema");

    // Un type absent du schema doit être rejeté.
    Dashboard d{}; char err[80];
    TEST_ASSERT_FALSE(dash_set_layout(&d,
        "{\"components\":{\"x\":{\"type\":\"definitely_not_a_type\"}},\"pages\":[]}",
        err, sizeof(err)));
}
void test_layout_invalid_keeps_old(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_set_layout(&d, "{ not json", err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(2, d.comp_count);
}

void test_countdown_decrements_and_formats(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_apply_update(&d, "{\"w5h\":{\"pct\":63,\"reset_in_s\":3601}}", unk, sizeof(unk));
    int iw = dash_find(&d,"w5h");
    d.components[iw].dirty = false;
    dash_tick_countdown(&d, 1);
    TEST_ASSERT_EQUAL_UINT32(3600, d.components[iw].reset_in_s);
    TEST_ASSERT_EQUAL_STRING("1h00", d.components[iw].caption);
    TEST_ASSERT_TRUE(d.components[iw].dirty);
}
void test_countdown_floor_zero(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_apply_update(&d, "{\"w5h\":{\"pct\":99,\"reset_in_s\":3}}", unk, sizeof(unk));
    dash_tick_countdown(&d, 10);
    TEST_ASSERT_EQUAL_UINT32(0, d.components[dash_find(&d,"w5h")].reset_in_s);
}

void test_update_partial_leaves_others(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int icpu = dash_find(&d,"cpu"), iw = dash_find(&d,"w5h");
    d.components[iw].value = 10;
    int nupd = dash_apply_update(&d, "{\"cpu\":42}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(1, nupd);
    TEST_ASSERT_EQUAL_STRING("42 %", d.components[icpu].vstr);
    TEST_ASSERT_EQUAL_INT(10, d.components[iw].value);
    TEST_ASSERT_TRUE(d.values_dirty);
}
void test_update_ring_object(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int iw = dash_find(&d,"w5h");
    dash_apply_update(&d, "{\"w5h\":{\"pct\":63,\"reset_in_s\":6600}}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(63, d.components[iw].value);
    TEST_ASSERT_EQUAL_UINT32(6600, d.components[iw].reset_in_s);
    TEST_ASSERT_EQUAL_STRING("1h50", d.components[iw].caption);
}
void test_update_unknown_reported_not_applied(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int nupd = dash_apply_update(&d, "{\"ghost\":1,\"cpu\":5}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(1, nupd);
    TEST_ASSERT_EQUAL_STRING("ghost", unk);
}

void test_next_mid(void)     { TEST_ASSERT_EQUAL_INT(2, nav_next(1, 3, true)); }
void test_next_wrap(void)    { TEST_ASSERT_EQUAL_INT(0, nav_next(2, 3, true)); }
void test_next_clamp(void)   { TEST_ASSERT_EQUAL_INT(2, nav_next(2, 3, false)); }
void test_prev_wrap(void)    { TEST_ASSERT_EQUAL_INT(2, nav_prev(0, 3, true)); }
void test_prev_clamp(void)   { TEST_ASSERT_EQUAL_INT(0, nav_prev(0, 3, false)); }
void test_single_page(void)  { TEST_ASSERT_EQUAL_INT(0, nav_next(0, 1, true)); }
void test_empty(void)        { TEST_ASSERT_EQUAL_INT(0, nav_next(0, 0, true)); }

void test_threshold_none(void) {
    Threshold t[1] = {{70,0x22C55E}};
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, threshold_color(t,0,50,0xABCDEF));
}

// --- contexte (blackboard) ---
void test_ctx_set_find_num(void) {
    Context c{};
    TEST_ASSERT_TRUE(ctx_set_num(&c, "cpu", 42, 100));
    int i = ctx_find(&c, "cpu");
    TEST_ASSERT_TRUE(i >= 0);
    TEST_ASSERT_EQUAL_INT(CTX_NUM, c.vars[i].type);
    TEST_ASSERT_EQUAL_INT(42, (int)c.vars[i].num);
    TEST_ASSERT_EQUAL_UINT32(100, c.vars[i].updated_at);
}
void test_ctx_overwrite_keeps_one_slot(void) {
    Context c{};
    ctx_set_num(&c, "x", 1, 0);
    ctx_set_str(&c, "x", "hi", 5);
    TEST_ASSERT_EQUAL_INT(1, c.count);                 // meme nom = meme slot
    int i = ctx_find(&c, "x");
    TEST_ASSERT_EQUAL_INT(CTX_STR, c.vars[i].type);
    TEST_ASSERT_EQUAL_STRING("hi", c.vars[i].str);
}
void test_ctx_full_rejects(void) {
    Context c{};
    char nm[8];
    for (int k = 0; k < MAX_CTX_VARS; k++) { snprintf(nm, sizeof(nm), "v%d", k); TEST_ASSERT_TRUE(ctx_set_num(&c, nm, k, 0)); }
    TEST_ASSERT_FALSE(ctx_set_num(&c, "over", 1, 0));  // plein -> refus
}

// --- extracteur JSON Pointer ---
void test_ptr_nested_object(void) {
    JsonDocument d; deserializeJson(d, "{\"main\":{\"temp\":21}}");
    JsonVariantConst v = ctx_extract_pointer(d.as<JsonVariantConst>(), "/main/temp");
    TEST_ASSERT_FALSE(v.isNull());
    TEST_ASSERT_EQUAL_INT(21, v.as<int>());
}
void test_ptr_array_index(void) {
    JsonDocument d; deserializeJson(d, "{\"list\":[10,20,30]}");
    JsonVariantConst v = ctx_extract_pointer(d.as<JsonVariantConst>(), "/list/1");
    TEST_ASSERT_EQUAL_INT(20, v.as<int>());
}
void test_ptr_missing_is_null(void) {
    JsonDocument d; deserializeJson(d, "{\"a\":1}");
    TEST_ASSERT_TRUE(ctx_extract_pointer(d.as<JsonVariantConst>(), "/a/b").isNull());
    TEST_ASSERT_TRUE(ctx_extract_pointer(d.as<JsonVariantConst>(), "/nope").isNull());
}
void test_ptr_escape(void) {
    JsonDocument d; deserializeJson(d, "{\"a/b\":7}");
    TEST_ASSERT_EQUAL_INT(7, ctx_extract_pointer(d.as<JsonVariantConst>(), "/a~1b").as<int>());
}

void test_ctx_apply_json_num_and_str(void) {
    Context c{};
    JsonDocument d; deserializeJson(d, "{\"cpu\":42,\"host\":\"srv1\"}");
    int n = ctx_apply_json(&c, d.as<JsonObjectConst>(), 7);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(42, (int)c.vars[ctx_find(&c,"cpu")].num);
    TEST_ASSERT_EQUAL_INT(CTX_STR, c.vars[ctx_find(&c,"host")].type);
    TEST_ASSERT_EQUAL_STRING("srv1", c.vars[ctx_find(&c,"host")].str);
}

void test_layout_bind_parsed(void) {
    Dashboard d{}; char err[80];
    const char* L = "{\"components\":{\"t\":{\"type\":\"readout\",\"bind\":\"temp\"}},\"pages\":[]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, L, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("temp", d.components[dash_find(&d,"t")].bind);
}
void test_dash_set_context_writes_ctx(void) {
    Dashboard d{};
    dash_set_context(&d, "{\"temp\":21}", 3);
    TEST_ASSERT_TRUE(ctx_find(&d.ctx, "temp") >= 0);
    TEST_ASSERT_EQUAL_INT(21, (int)d.ctx.vars[ctx_find(&d.ctx,"temp")].num);
}

// --- context_apply : variables liees -> composants ---
static const char* bound_layout(const char* type, const char* extra) {
    static char b[256];
    snprintf(b, sizeof(b),
        "{\"components\":{\"x\":{\"type\":\"%s\",\"bind\":\"v\"%s}},"
        "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"x\"}]}]}", type, extra);
    return b;
}
void test_ctxapply_readout_num_formats(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("readout", ",\"unit\":\"C\""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":21}", 1);
    context_apply(&d);
    int i = dash_find(&d,"x");
    TEST_ASSERT_EQUAL_STRING("21 C", d.components[i].vstr);
    TEST_ASSERT_TRUE(d.components[i].dirty);
}
void test_ctxapply_readout_string(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("readout", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":\"OK\"}", 1);
    context_apply(&d);
    TEST_ASSERT_EQUAL_STRING("OK", d.components[dash_find(&d,"x")].vstr);
}
void test_ctxapply_bar_value(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":63}", 1);
    context_apply(&d);
    TEST_ASSERT_EQUAL_INT(63, d.components[dash_find(&d,"x")].value);
}
void test_ctxapply_unchanged_not_dirty(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":63}", 1);
    context_apply(&d);
    d.components[dash_find(&d,"x")].dirty = false;
    context_apply(&d);                                  // meme valeur : pas de re-dirty
    TEST_ASSERT_FALSE(d.components[dash_find(&d,"x")].dirty);
}
void test_ctxapply_missing_var_keeps_value(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    d.components[dash_find(&d,"x")].value = 7;
    context_apply(&d);                                  // variable "v" absente
    TEST_ASSERT_EQUAL_INT(7, d.components[dash_find(&d,"x")].value);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_remaining_seconds);
    RUN_TEST(test_remaining_min_boundary);
    RUN_TEST(test_remaining_minutes);
    RUN_TEST(test_remaining_hour);
    RUN_TEST(test_remaining_h_m);
    RUN_TEST(test_remaining_days);
    RUN_TEST(test_remaining_zero);
    RUN_TEST(test_value_unit);
    RUN_TEST(test_value_float);
    RUN_TEST(test_value_no_unit);
    RUN_TEST(test_countdown_decrements_and_formats);
    RUN_TEST(test_countdown_floor_zero);
    RUN_TEST(test_update_partial_leaves_others);
    RUN_TEST(test_update_ring_object);
    RUN_TEST(test_update_unknown_reported_not_applied);
    RUN_TEST(test_layout_parse_counts);
    RUN_TEST(test_layout_types_and_geom);
    RUN_TEST(test_ring_center_pct_parsed);
    RUN_TEST(test_ring_start_angle_parsed);
    RUN_TEST(test_ring_start_angle_default_zero);
    RUN_TEST(test_ring_center_color_set);
    RUN_TEST(test_ring_center_color_defaults_to_color);
    RUN_TEST(test_layout_unknown_type_rejected);
    RUN_TEST(test_schema_types_all_resolve);
    RUN_TEST(test_ctx_set_find_num);
    RUN_TEST(test_ctx_overwrite_keeps_one_slot);
    RUN_TEST(test_ctx_full_rejects);
    RUN_TEST(test_ptr_nested_object);
    RUN_TEST(test_ptr_array_index);
    RUN_TEST(test_ptr_missing_is_null);
    RUN_TEST(test_ptr_escape);
    RUN_TEST(test_ctx_apply_json_num_and_str);
    RUN_TEST(test_layout_bind_parsed);
    RUN_TEST(test_dash_set_context_writes_ctx);
    RUN_TEST(test_ctxapply_readout_num_formats);
    RUN_TEST(test_ctxapply_readout_string);
    RUN_TEST(test_ctxapply_bar_value);
    RUN_TEST(test_ctxapply_unchanged_not_dirty);
    RUN_TEST(test_ctxapply_missing_var_keeps_value);
    RUN_TEST(test_layout_invalid_keeps_old);
    RUN_TEST(test_hex_parse);
    RUN_TEST(test_hex_no_hash);
    RUN_TEST(test_hex_fallback);
    RUN_TEST(test_threshold_below);
    RUN_TEST(test_threshold_mid);
    RUN_TEST(test_threshold_over);
    RUN_TEST(test_threshold_none);
    RUN_TEST(test_next_mid);
    RUN_TEST(test_next_wrap);
    RUN_TEST(test_next_clamp);
    RUN_TEST(test_prev_wrap);
    RUN_TEST(test_prev_clamp);
    RUN_TEST(test_single_page);
    RUN_TEST(test_empty);
    return UNITY_END();
}
