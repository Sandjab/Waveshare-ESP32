#pragma once
//
// crystal_ping.h — son "ping" cristallin, declenche par notification.
//
// Synth additif 4 partiels (Strike Note 880 Hz + octave + tierce inharmonique
// + quint haute), enveloppe expo decroissante. Copie minimale du synth de
// Basic_Audio_Ping, replie dans une task FreeRTOS qui dort sur notification :
// silence total tant qu'on ne declenche pas, ping complet (~1.5 s) sur trigger.
// Re-trigger pendant un ping en cours : reset start_ms + phases pour attaque
// nette immediate.
//
// Pinned sur core 0 pour laisser la loop Arduino + LVGL sur core 1.

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "guition_pins.h"

namespace crystal_ping {

static constexpr uint32_t SAMPLE_RATE       = 44100;
static constexpr size_t   FRAMES_PER_BUF    = 256;
static constexpr float    GAIN              = 0.08f;
static constexpr int      SILENCE_BUFS_END  = 4;     // drain DMA avant mute
static constexpr uint32_t PING_DURATION_MS  = 1500;

struct Partial {
    float freq;
    float amp;
    float decay_tau_s;
    float phase;
    float phase_inc;
};

// Strike Note 880 Hz (A5) + octave + tierce inharmonique (×2.76) + quint (×5.4).
static Partial partials[] = {
    {  880.0f, 1.00f, 0.50f, 0.0f, 0.0f },
    { 1760.0f, 0.50f, 0.30f, 0.0f, 0.0f },
    { 2429.0f, 0.30f, 0.20f, 0.0f, 0.0f },
    { 4752.0f, 0.15f, 0.10f, 0.0f, 0.0f },
};
static constexpr size_t N_PARTIALS = sizeof(partials) / sizeof(partials[0]);

static i2s_chan_handle_t i2s_tx           = nullptr;
static int16_t           i2s_buf[FRAMES_PER_BUF * 2];
static TaskHandle_t      audio_task_handle = nullptr;
static volatile bool     retrigger        = false;

static inline void reset_voice() {
    for (size_t i = 0; i < N_PARTIALS; i++) partials[i].phase = 0.0f;
}

static inline void fill_silence_and_write() {
    memset(i2s_buf, 0, sizeof(i2s_buf));
    size_t written;
    i2s_channel_write(i2s_tx, i2s_buf, sizeof(i2s_buf), &written, portMAX_DELAY);
}

static void audio_task(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        digitalWrite(PIN_PA_MUTE, HIGH);  // unmute ampli NS4150B

        uint32_t start_ms = millis();
        reset_voice();
        retrigger = false;

        while (millis() - start_ms < PING_DURATION_MS) {
            if (retrigger) {
                start_ms  = millis();
                retrigger = false;
                reset_voice();
            }

            float t = (millis() - start_ms) / 1000.0f;
            float env[N_PARTIALS];
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
                mix *= GAIN;
                if (mix > 1.0f)  mix = 1.0f;
                if (mix < -1.0f) mix = -1.0f;
                int16_t s = (int16_t)(mix * 32767.0f);
                i2s_buf[i * 2]     = s;
                i2s_buf[i * 2 + 1] = s;
            }

            size_t written;
            i2s_channel_write(i2s_tx, i2s_buf, sizeof(i2s_buf), &written, portMAX_DELAY);
        }

        // Drain : remplir plusieurs buffers de zeros pour vider la queue DMA
        // avant de muter le PA. Sinon le NS4150B (Class D) continue d'amplifier
        // ce qui reste dans le pipeline I2S -> ronflement residuel.
        for (int i = 0; i < SILENCE_BUFS_END; i++) fill_silence_and_write();
        digitalWrite(PIN_PA_MUTE, LOW);  // mute ampli -> silence total
    }
}

static inline void init() {
    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, LOW);  // demarre mute, unmute uniquement pendant le ping

    for (size_t i = 0; i < N_PARTIALS; i++) {
        partials[i].phase_inc = 2.0f * (float)M_PI * partials[i].freq / SAMPLE_RATE;
    }

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

    xTaskCreatePinnedToCore(audio_task, "crystal_ping", 4096, nullptr, 5, &audio_task_handle, 0);
}

static inline void trigger() {
    if (!audio_task_handle) return;
    retrigger = true;
    xTaskNotifyGive(audio_task_handle);
}

}  // namespace crystal_ping
