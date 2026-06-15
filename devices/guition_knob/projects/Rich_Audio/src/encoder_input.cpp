#include "encoder_input.h"
#include "audio_engine.h"
#include "ui.h"
#include "bidi_switch_knob.h"
#include "guition_pins.h"

static knob_handle_t s_knob = nullptr;

void encoder_begin() {
    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    s_knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(s_knob);
}

void encoder_tick() {
    if (!s_knob) return;
    int delta = iot_knob_get_count_value(s_knob);
    if (delta == 0) return;
    iot_knob_clear_count_value(s_knob);
    audio_set_volume(audio_get_volume() + delta);
    ui_set_volume(audio_get_volume());
}
