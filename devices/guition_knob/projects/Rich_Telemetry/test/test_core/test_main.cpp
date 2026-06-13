#include <unity.h>
#include <string.h>
#include "format.h"
#include "color.h"

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
    RUN_TEST(test_hex_parse);
    RUN_TEST(test_hex_no_hash);
    RUN_TEST(test_hex_fallback);
    RUN_TEST(test_threshold_below);
    RUN_TEST(test_threshold_mid);
    RUN_TEST(test_threshold_over);
    RUN_TEST(test_threshold_none);
    return UNITY_END();
}
