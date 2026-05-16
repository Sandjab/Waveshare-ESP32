#include <Arduino.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"
#include "voice_samples.h"

// Sample-based TTS multilingue : phrases generees offline avec macOS
// `say -v <voice>` + sox vers PCM 16-bit mono 16 kHz, embed en flash
// (voice_samples.cpp). 6 phrases tournent en boucle avec 4 voix (Thomas FR M,
// Amélie FR-CA F, Samantha EN-US F, Alice IT F). Volume via encoder
// (0..20 crans, defaut 4). I2S a 16 kHz, PCM5100A auto-detecte la cadence.

static constexpr uint32_t SAMPLE_RATE     = 16000;
static constexpr size_t   FRAMES_PER_BUF  = 256;
static constexpr float    GAIN_MAX        = 0.50f;
static constexpr int      VOL_STEPS       = 20;
static constexpr int      VOL_DEFAULT     = 4;
static constexpr uint32_t PHRASE_PAUSE_MS = 1500;

static i2s_chan_handle_t i2s_tx;
static knob_handle_t     knob     = nullptr;
static int               vol_step = VOL_DEFAULT;
static int16_t           stereo_buf[FRAMES_PER_BUF * 2];

static void poll_volume() {
    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        vol_step += delta;
        if (vol_step < 0)         vol_step = 0;
        if (vol_step > VOL_STEPS) vol_step = VOL_STEPS;
        Serial.printf("Vol : %d/%d\n", vol_step, VOL_STEPS);
    }
}

static inline float current_gain() {
    return ((float)vol_step / VOL_STEPS) * GAIN_MAX;
}

static void play_phrase(const int16_t* mono, size_t n_samples) {
    size_t i = 0;
    while (i < n_samples) {
        poll_volume();
        float gain = current_gain();
        size_t chunk = (n_samples - i < FRAMES_PER_BUF) ? (n_samples - i) : FRAMES_PER_BUF;
        for (size_t j = 0; j < chunk; j++) {
            int16_t s = (int16_t)((float)mono[i + j] * gain);
            stereo_buf[j * 2]     = s;     // L
            stereo_buf[j * 2 + 1] = s;     // R
        }
        size_t written;
        i2s_channel_write(i2s_tx, stereo_buf, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        i += chunk;
    }
}

static void play_silence_ms(uint32_t ms) {
    memset(stereo_buf, 0, sizeof(stereo_buf));
    uint32_t total_frames = SAMPLE_RATE * ms / 1000;
    while (total_frames > 0) {
        poll_volume();
        uint32_t chunk = (total_frames < FRAMES_PER_BUF) ? total_frames : FRAMES_PER_BUF;
        size_t written;
        i2s_channel_write(i2s_tx, stereo_buf, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        total_frames -= chunk;
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
    Serial.println("Guition JC3636K718 — Basic_Audio_Speak (TTS Thomas FR)");

    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    i2s_setup();
    Serial.printf("Vol initial : %d/%d (0=mute, tourner pour ajuster)\n", vol_step, VOL_STEPS);
}

void loop() {
    play_phrase(thomas_petitpas_pcm, thomas_petitpas_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
    play_phrase(thomas_volonte_pcm,  thomas_volonte_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
    play_phrase(thomas_biere_pcm,    thomas_biere_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
    play_phrase(amelie_knob_pcm,     amelie_knob_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
    play_phrase(samantha_hasta_pcm,  samantha_hasta_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
    play_phrase(alice_knob_pcm,      alice_knob_pcm_len);
    play_silence_ms(PHRASE_PAUSE_MS);
}
