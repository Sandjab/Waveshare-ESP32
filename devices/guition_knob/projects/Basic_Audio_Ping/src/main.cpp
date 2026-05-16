#include <Arduino.h>
#include <math.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"

// "Ping" type notification/cloche : additive synthesis de 4 partiels avec
// enveloppe exponentielle decroissante. Les partiels suivent grossierement
// le spectre inharmonique d'une cloche (1, 2, 2.76, 5.4 × fondamentale —
// rapports classiques de Strike Note / Hum / Tierce / Quint).
//
// Re-trigger toutes les 1500 ms. L'enveloppe est evaluee 1 fois par buffer
// (~5.8 ms) plutot que par sample : variation < 7 % sur le partiel le plus
// court, inaudible.
//
// Volume reglable via l'encoder : 21 crans (0=mute, 20=max). Demarre a 4
// (~10 % du max). Pas de switch click sur l'encoder Guition -> position 0
// fait office d'arret.

static constexpr uint32_t SAMPLE_RATE      = 44100;
static constexpr size_t   FRAMES_PER_BUF   = 256;
static constexpr float    GAIN_MAX         = 0.50f;
static constexpr int      VOL_STEPS        = 20;
static constexpr int      VOL_DEFAULT      = 4;       // 4/20 * 0.50 = 0.10 (doux)
static constexpr uint32_t PING_INTERVAL_MS = 1500;

struct Partial {
    float freq;           // Hz
    float amp;            // peak (0..1)
    float decay_tau_s;    // time constant (env = exp(-t/tau))
    float phase;
    float phase_inc;      // precomputed = 2π·f/SR
};

// Strike Note 880 Hz ≈ A5 → "ping" cristallin.
static Partial partials[] = {
    {  880.0f, 1.00f, 0.50f, 0.0f, 0.0f },   // fondamentale
    { 1760.0f, 0.50f, 0.30f, 0.0f, 0.0f },   // octave
    { 2429.0f, 0.30f, 0.20f, 0.0f, 0.0f },   // tierce inharmonique (×2.76)
    { 4752.0f, 0.15f, 0.10f, 0.0f, 0.0f },   // quint haute (×5.4)
};
static constexpr size_t N_PARTIALS = sizeof(partials) / sizeof(partials[0]);

static i2s_chan_handle_t i2s_tx;
static int16_t           i2s_buf[FRAMES_PER_BUF * 2];

static uint32_t      ping_start_ms = 0;
static knob_handle_t knob          = nullptr;
static int           vol_step      = VOL_DEFAULT;

static void ping_trigger() {
    ping_start_ms = millis();
    for (size_t i = 0; i < N_PARTIALS; i++) {
        partials[i].phase = 0.0f;        // attaque nette
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
    Serial.println("Guition JC3636K718 — Basic_Audio_Ping (additive bell)");

    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);

    for (size_t i = 0; i < N_PARTIALS; i++) {
        partials[i].phase_inc = 2.0f * (float)M_PI * partials[i].freq / SAMPLE_RATE;
    }

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    i2s_setup();
    ping_trigger();
    Serial.printf("Vol initial : %d/%d (0=mute, tourner pour ajuster)\n", vol_step, VOL_STEPS);
}

void loop() {
    // Encoder delta -> vol_step (clamp 0..VOL_STEPS).
    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        vol_step += delta;
        if (vol_step < 0)         vol_step = 0;
        if (vol_step > VOL_STEPS) vol_step = VOL_STEPS;
        Serial.printf("Vol : %d/%d\n", vol_step, VOL_STEPS);
    }
    float output_gain = ((float)vol_step / VOL_STEPS) * GAIN_MAX;

    uint32_t elapsed_ms = millis() - ping_start_ms;
    if (elapsed_ms >= PING_INTERVAL_MS) {
        ping_trigger();
        elapsed_ms = 0;
    }

    // Eval enveloppes 1 fois par buffer (constantes sur 5.8 ms).
    float env[N_PARTIALS];
    float t = elapsed_ms / 1000.0f;
    for (size_t i = 0; i < N_PARTIALS; i++) {
        env[i] = expf(-t / partials[i].decay_tau_s) * partials[i].amp;
    }

    for (size_t i = 0; i < FRAMES_PER_BUF; i++) {
        float mix = 0.0f;
        for (size_t k = 0; k < N_PARTIALS; k++) {
            mix += env[k] * sinf(partials[k].phase);
            partials[k].phase += partials[k].phase_inc;
            if (partials[k].phase >= 2.0f * (float)M_PI) {
                partials[k].phase -= 2.0f * (float)M_PI;
            }
        }
        mix *= output_gain;
        if (mix > 1.0f)  mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;
        int16_t s = (int16_t)(mix * 32767.0f);
        i2s_buf[i * 2]     = s;
        i2s_buf[i * 2 + 1] = s;
    }

    size_t written;
    i2s_channel_write(i2s_tx, i2s_buf, sizeof(i2s_buf), &written, portMAX_DELAY);
}
