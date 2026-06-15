#include "audio_engine.h"
#include "config.h"
#include "sounds.h"
#include <Arduino.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "guition_pins.h"

static i2s_chan_handle_t s_tx        = nullptr;
static QueueHandle_t     s_req_q     = nullptr;   // requêtes de lecture (index de son), longueur 1
static volatile int      s_volume    = AUDIO_VOL_DEFAULT;
static volatile bool     s_playing   = false;
static int16_t           s_buf[AUDIO_FRAMES_PER_BUF * 2];

static void i2s_setup() {
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&cc, &s_tx, nullptr);
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
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
    i2s_channel_init_std_mode(s_tx, &sc);
    i2s_channel_enable(s_tx);
}

// Applique le volume maître (entier) à un bloc déjà rendu, en place.
static void apply_volume(int16_t* stereo, size_t frames) {
    int vol = s_volume;
    if (vol >= AUDIO_VOL_STEPS) return;          // pleine échelle : rien à faire
    for (size_t i = 0; i < frames * 2; i++) {
        stereo[i] = (int16_t)(((int32_t)stereo[i] * vol) / AUDIO_VOL_STEPS);
    }
}

// La tâche audio : récupère une éventuelle requête, rend le bloc actif (ou du silence),
// applique le volume, écrit en I2S. i2s_channel_write bloque tant que le DMA est plein,
// ce qui cadence naturellement la tâche au temps réel.
static void audio_task(void*) {
    Sound* active = nullptr;
    for (;;) {
        int req;
        if (xQueueReceive(s_req_q, &req, 0) == pdTRUE) {
            active = sounds_get(req);
            if (active) active->trigger();
        }

        // NB : on ne touche PAS à PIN_PA_MUTE ici. L'ampli reste allumé en permanence
        // (cf. audio_begin). Le couper à l'idle refaisait passer le NS4150B par son
        // soft-start (~centaines de ms) à chaque lecture, ce qui avalait entièrement
        // les sons plus courts que ce réveil (Beep/Pop/Tick).
        if (active) {
            bool more = active->render(s_buf, AUDIO_FRAMES_PER_BUF);
            apply_volume(s_buf, AUDIO_FRAMES_PER_BUF);
            s_playing = true;
            if (!more) {                            // dernier bloc (traîne incluse)
                active = nullptr;
                s_playing = false;
            }
        } else {
            memset(s_buf, 0, sizeof(s_buf));
        }

        size_t written;
        i2s_channel_write(s_tx, s_buf, sizeof(s_buf), &written, portMAX_DELAY);
    }
}

void audio_begin() {
    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);                // ampli activé en permanence (cf.
                                                    // Basic_Audio_*) : le soft-start du
                                                    // NS4150B avalait les sons courts si on
                                                    // le coupait entre deux. Souffle au
                                                    // repos accepté (compromis).
    i2s_setup();
    s_req_q = xQueueCreate(1, sizeof(int));
    // Cœur 0 (PRO_CPU) : libre ici (pas de WiFi), laisse LVGL/Arduino sur le cœur 1.
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, nullptr, 5, nullptr, 0);
}

void audio_play(int sound_index) {
    if (!s_req_q || sound_index < 0 || sound_index >= sounds_count()) return;
    xQueueOverwrite(s_req_q, &sound_index);         // coalesce : seule la dernière requête compte
}

void audio_set_volume(int step) {
    if (step < 0)                step = 0;
    if (step > AUDIO_VOL_STEPS)  step = AUDIO_VOL_STEPS;
    s_volume = step;
}

int  audio_get_volume() { return s_volume; }
bool audio_is_playing() { return s_playing; }
