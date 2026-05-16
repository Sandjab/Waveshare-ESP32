#include <Arduino.h>
#include <math.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"

// Arpège Cmaj7 (C4, E4, G4, B4) en synthèse Karplus-Strong sur le HP onboard.
// Chaque "corde" est un buffer circulaire de longueur N = SR/freq, initialisé
// avec du bruit blanc au moment du pluck, puis filtré sample-par-sample par
// une moyenne glissante (low-pass) avec un facteur de damping. C'est le modèle
// physique le plus simple d'une corde pincée — ~3 ko de RAM pour 4 voix.
// Volume reglable via l'encoder : 21 crans (0=mute, 20=max). Demarre a 4.

static constexpr uint32_t SAMPLE_RATE      = 44100;
static constexpr size_t   FRAMES_PER_BUF   = 256;
static constexpr float    PLUCK_AMPLITUDE  = 0.30f;
static constexpr float    DAMPING          = 0.996f;     // 1.0 = corde infinie
static constexpr float    GAIN_MAX         = 0.50f;      // headroom 4 voies
static constexpr int      VOL_STEPS        = 20;
static constexpr int      VOL_DEFAULT      = 4;
static constexpr size_t   MAX_STRING_LEN   = 200;        // >= SR/220Hz

// Cmaj7 ascending: C4 - E4 - G4 - B4
static constexpr float STRING_FREQ[4] = { 261.63f, 329.63f, 392.00f, 493.88f };
static constexpr uint32_t PLUCK_INTERVAL_MS  = 280;      // entre 2 cordes
static constexpr uint32_t REST_AFTER_LAST_MS = 1400;     // pause fin arpège

struct KSVoice {
    float  buf[MAX_STRING_LEN];
    size_t len;
    size_t idx;
};

static KSVoice           strings[4];
static i2s_chan_handle_t i2s_tx;
static int16_t           i2s_buf[FRAMES_PER_BUF * 2];
static knob_handle_t     knob     = nullptr;
static int               vol_step = VOL_DEFAULT;

static void string_init(KSVoice& s, float freq) {
    s.len = (size_t)(SAMPLE_RATE / freq + 0.5f);
    if (s.len > MAX_STRING_LEN) s.len = MAX_STRING_LEN;
    s.idx = 0;
    for (size_t i = 0; i < s.len; i++) s.buf[i] = 0.0f;
}

static void string_pluck(KSVoice& s) {
    for (size_t i = 0; i < s.len; i++) {
        // bruit blanc uniforme [-PLUCK_AMPLITUDE, +PLUCK_AMPLITUDE]
        float r = (float)random(-32768, 32768) / 32768.0f;
        s.buf[i] = r * PLUCK_AMPLITUDE;
    }
}

static inline float string_next(KSVoice& s) {
    float out = s.buf[s.idx];
    size_t next = s.idx + 1;
    if (next >= s.len) next = 0;
    // K-S : new = 0.5 * (current + next) * damping
    s.buf[s.idx] = 0.5f * (s.buf[s.idx] + s.buf[next]) * DAMPING;
    s.idx = next;
    return out;
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
    Serial.println("Guition JC3636K718 — Basic_Audio_Harp (Karplus-Strong Cmaj7)");

    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);    // active speaker amp

    for (int i = 0; i < 4; i++) string_init(strings[i], STRING_FREQ[i]);

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    i2s_setup();
    Serial.printf("Vol initial : %d/%d (0=mute, tourner pour ajuster)\n", vol_step, VOL_STEPS);
}

void loop() {
    // Arpège : pluck 0,1,2,3 puis silence, puis recommence.
    static int      pluck_step    = 0;
    static uint32_t last_pluck_ms = 0;

    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        vol_step += delta;
        if (vol_step < 0)         vol_step = 0;
        if (vol_step > VOL_STEPS) vol_step = VOL_STEPS;
        Serial.printf("Vol : %d/%d\n", vol_step, VOL_STEPS);
    }
    float output_gain = ((float)vol_step / VOL_STEPS) * GAIN_MAX;

    uint32_t now = millis();
    uint32_t target = (pluck_step >= 4) ? REST_AFTER_LAST_MS : PLUCK_INTERVAL_MS;
    if (now - last_pluck_ms >= target) {
        last_pluck_ms = now;
        if (pluck_step < 4) {
            string_pluck(strings[pluck_step]);
        }
        pluck_step++;
        if (pluck_step > 4) pluck_step = 0;
    }

    // Mix les 4 voix → buffer stéréo (L=R, l'ampli somme déjà L+R en mono).
    for (size_t i = 0; i < FRAMES_PER_BUF; i++) {
        float mix = string_next(strings[0])
                  + string_next(strings[1])
                  + string_next(strings[2])
                  + string_next(strings[3]);
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
