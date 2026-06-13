#include <unity.h>
#include <string.h>
#include "format.h"
#include "color.h"
#include "nav_logic.h"
#include "dashboard.h"

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
void test_layout_unknown_type_rejected(void) {
    Dashboard d{}; char err[80];
    const char* bad = "{\"components\":{\"x\":{\"type\":\"frobnicator\"}},\"pages\":[]}";
    TEST_ASSERT_FALSE(dash_set_layout(&d, bad, err, sizeof(err)));
}
void test_layout_invalid_keeps_old(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_set_layout(&d, "{ not json", err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(2, d.comp_count);
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
    RUN_TEST(test_layout_parse_counts);
    RUN_TEST(test_layout_types_and_geom);
    RUN_TEST(test_layout_unknown_type_rejected);
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
