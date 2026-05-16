#include <Arduino.h>
#include <math.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"

// 440 Hz beep (A4) intermittent : 250 ms on / 250 ms off.
// Chaine : ESP32 I2S (BCK=3 / WS=45 / DO=42) -> PCM5100A -> NS4150B -> HP onboard.
// L'ampli NS4150B est activé par PIN_PA_MUTE high (cf. CLAUDE.md device).
// Volume reglable via l'encoder : 21 crans (0=mute, 20=max). Demarre a 4.

static constexpr uint32_t SAMPLE_RATE    = 44100;
static constexpr float    TONE_HZ        = 440.0f;
static constexpr float    GAIN_MAX       = 0.30f;
static constexpr int      VOL_STEPS      = 20;
static constexpr int      VOL_DEFAULT    = 4;
static constexpr uint32_t BEEP_ON_MS     = 250;
static constexpr uint32_t BEEP_OFF_MS    = 250;
static constexpr size_t   FRAMES_PER_BUF = 256;    // ~5.8 ms / buffer

static i2s_chan_handle_t i2s_tx;
static int16_t           i2s_buf[FRAMES_PER_BUF * 2];   // stereo interleaved
static float             phase = 0.0f;
static const float       PHASE_INC = 2.0f * (float)M_PI * TONE_HZ / SAMPLE_RATE;
static knob_handle_t     knob     = nullptr;
static int               vol_step = VOL_DEFAULT;

static void fill_buffer(bool active, float gain) {
    for (size_t i = 0; i < FRAMES_PER_BUF; i++) {
        int16_t s = active ? (int16_t)(gain * 32767.0f * sinf(phase)) : 0;
        i2s_buf[i * 2]     = s;     // L
        i2s_buf[i * 2 + 1] = s;     // R (somme passive L+R cote NS4150)
        phase += PHASE_INC;
        if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
    }
}

static void i2s_setup() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &i2s_tx, nullptr);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)PIN_I2S_BCK,
            .ws   = (gpio_num_t)PIN_I2S_WS,
            .dout = (gpio_num_t)PIN_I2S_DO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    i2s_channel_init_std_mode(i2s_tx, &std_cfg);
    i2s_channel_enable(i2s_tx);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Guition JC3636K718 — Basic_Audio (440 Hz beep)");

    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);    // active speaker amp

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    i2s_setup();
    Serial.printf("Vol initial : %d/%d (0=mute, tourner pour ajuster)\n", vol_step, VOL_STEPS);
}

void loop() {
    static uint32_t state_start_ms = 0;
    static bool active = true;

    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        vol_step += delta;
        if (vol_step < 0)         vol_step = 0;
        if (vol_step > VOL_STEPS) vol_step = VOL_STEPS;
        Serial.printf("Vol : %d/%d\n", vol_step, VOL_STEPS);
    }
    float gain = ((float)vol_step / VOL_STEPS) * GAIN_MAX;

    uint32_t now = millis();
    uint32_t target = active ? BEEP_ON_MS : BEEP_OFF_MS;
    if (now - state_start_ms >= target) {
        active = !active;
        state_start_ms = now;
    }

    fill_buffer(active, gain);
    size_t written;
    i2s_channel_write(i2s_tx, i2s_buf, sizeof(i2s_buf), &written, portMAX_DELAY);
}
