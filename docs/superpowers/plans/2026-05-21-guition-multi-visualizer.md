# Basic_Audio_Multiviz Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implémenter `Basic_Audio_Multiviz` sur Guition K718 : 12 visualisations audio-réactives cyclées via la rotation de l'encodeur, avec OSD + tick haptique sur switch, en réutilisant la chaîne mic PDM + FFT + bandes de `Basic_Audio_Visualizer`.

**Architecture:** Registry de `Visualizer` (struct + fn pointers) — pattern AVS Winamp. Pipeline audio (mic + FFT + bandes + transient detect) extrait dans un module mutualisé. Encoder polling, OSD via `lv_layer_top()`, init/deinit par viz pour libérer les objets LVGL au switch.

**Tech Stack:** PlatformIO + Arduino ESP32 (Espressif 5.x), LVGL 8.4, `arduinoFFT` 2.x, `Adafruit_NeoPixel`, `Adafruit_DRV2605`, ESP-IDF I2S PDM API (`driver/i2s_pdm.h`).

**Spec source:** `docs/superpowers/specs/2026-05-21-guition-multi-visualizer-design.md`

---

## Verification approach (embedded — pas de TDD classique)

Le repo n'a **aucun harness de test ESP32**. Le cycle de vérification est :

1. **Build** : `./build.sh guition_knob Basic_Audio_Multiviz` — catch les erreurs de compilation / link / lib_deps.
2. **Flash + observation** : `./build.sh guition_knob Basic_Audio_Multiviz --upload` puis observation visuelle sur l'écran + l'anneau LED + écoute des ticks haptiques.
3. **Serial monitor** au besoin : `pio device monitor -b 115200` (`Serial.printf` temporaires retirés après validation).

Chaque task se termine par "Build + Flash + Observe + Commit". On ne passe à la task suivante que si l'observation est conforme à l'attendu.

**Avant chaque flash**, vérifier sur quel device on est branché — plusieurs devices du monorepo partagent `VID:PID=303A:1001`. Le script `build.sh` invoque automatiquement `tools/device_mac.py check guition_knob` qui abort si le MAC ne matche pas l'inventaire `devices.local.yaml`.

---

## File Structure

Tout sous `devices/guition_knob/projects/Basic_Audio_Multiviz/`.

**À créer** :
- `platformio.ini` — copie de `Basic_Audio_Visualizer/platformio.ini` + lib DRV2605
- `src/lv_conf.h` — copie de `Basic_Audio_Visualizer/src/lv_conf.h`
- `src/viz_api.h` — `struct AudioFrame`, `struct Visualizer`
- `src/audio_pipeline.h` + `src/audio_pipeline.cpp` — mic PDM + FFT + bandes + transient
- `src/osd.h` + `src/osd.cpp` — label `"N/12 — Nom"` avec fade
- `src/main.cpp` — setup, loop, encoder, viz registry, dispatch
- `src/viz_spectrum_radial.cpp` (#1)
- `src/viz_oscillo.cpp` (#2)
- `src/viz_spectrum_bars.cpp` (#3)
- `src/viz_peak_meter.cpp` (#4)
- `src/viz_lissajous.cpp` (#5)
- `src/viz_tunnel.cpp` (#6)
- `src/viz_beat_bloom.cpp` (#7)
- `src/viz_starfield.cpp` (#8)
- `src/viz_geiss_stripes.cpp` (#9)
- `src/viz_g_wave.cpp` (#10)
- `src/viz_kaleidoscope.cpp` (#11)
- `src/viz_matrix_rain.cpp` (#12)

**À modifier** :
- `devices/guition_knob/README.md` — ajouter une ligne pointant `Basic_Audio_Multiviz`

**Intact** :
- `devices/guition_knob/projects/Basic_Audio_Visualizer/` (démo minimale conservée)

---

## Task 1: Scaffold project (platformio.ini + lv_conf.h + main.cpp stub)

**Files:**
- Create: `devices/guition_knob/projects/Basic_Audio_Multiviz/platformio.ini`
- Create: `devices/guition_knob/projects/Basic_Audio_Multiviz/src/lv_conf.h`
- Create: `devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp`

- [ ] **Step 1: Créer `platformio.ini`**

```ini
[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.07/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
lib_extra_dirs =
    ../../lib
    ../../../../shared/lib
lib_deps =
    lvgl/lvgl@^8.4.0
    adafruit/Adafruit NeoPixel
    kosme/arduinoFFT@^2.0.4
    adafruit/Adafruit DRV2605 Library
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE
    -Isrc
```

- [ ] **Step 2: Créer `src/lv_conf.h` avec les fonts utilisées par cette démo**

Chaque projet a sa propre `lv_conf.h` minimale qui n'active que les fonts dont il a besoin (économise du flash). On a besoin de Montserrat 14 (Matrix Rain) et 20 (OSD).

```cpp
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH         16
#define LV_COLOR_16_SWAP       1

#define LV_MEM_SIZE            (48U * 1024U)

#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_20  1

#define LV_BUILD_EXAMPLES      0

#endif
```

- [ ] **Step 3: Créer `main.cpp` stub minimal**

```cpp
#include <Arduino.h>
#include "guition_lvgl.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Basic_Audio_Multiviz scaffolding OK");
    guition_lvgl_init(72);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

- [ ] **Step 4: Build pour valider le scaffolding**

Run: `./build.sh guition_knob Basic_Audio_Multiviz`
Expected: build succeed, pas de warning critique.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/
git commit -m "Guition: scaffold Basic_Audio_Multiviz project"
```

---

## Task 2: viz_api.h — interface AudioFrame + Visualizer

**Files:**
- Create: `devices/guition_knob/projects/Basic_Audio_Multiviz/src/viz_api.h`

- [ ] **Step 1: Écrire `viz_api.h`**

```cpp
#pragma once

#include <stdint.h>
#include <stddef.h>

// Données audio passées à chaque viz à chaque tour de loop.
// Pointeurs valables uniquement le temps du render() courant.
struct AudioFrame {
    const int16_t* wave;       // 512 samples PCM bruts (post DC removal)
    const double*  fft_mag;    // 256 bins de magnitude FFT
    const float*   bands;      // 13 bandes log normalisées [0..1] (post peak-meter)
    float          rms;        // RMS normalisé [0..1]
    bool           transient;  // true si onset détecté ce tick
    uint32_t       t_ms;       // millis() au tick courant
};

// Une visualisation = init/render/deinit + nom.
struct Visualizer {
    const char* name;
    void (*init)();
    void (*render)(const AudioFrame&);
    void (*deinit)();
};
```

- [ ] **Step 2: Build (devra encore compiler — `viz_api.h` n'est pas encore inclus)**

Run: `./build.sh guition_knob Basic_Audio_Multiviz`
Expected: build succeed.

- [ ] **Step 3: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/viz_api.h
git commit -m "Multiviz: add Visualizer + AudioFrame interface (viz_api.h)"
```

---

## Task 3: audio_pipeline — mic + FFT + bandes + transient

**Files:**
- Create: `src/audio_pipeline.h`
- Create: `src/audio_pipeline.cpp`

- [ ] **Step 1: Écrire `audio_pipeline.h`**

```cpp
#pragma once

#include "viz_api.h"

// Initialise le mic PDM (I2S_NUM_0), prépare la FFT et le layout des 13 bandes.
// À appeler une fois dans setup() avant audio_pipeline_tick().
void audio_pipeline_init();

// Bloque ~32 ms pour capturer 512 samples @ 16 kHz, calcule FFT + bandes + RMS
// + transient detect. Retourne une AudioFrame valide jusqu'au prochain appel.
const AudioFrame& audio_pipeline_tick();
```

- [ ] **Step 2: Écrire `audio_pipeline.cpp`**

```cpp
#include "audio_pipeline.h"

#include <Arduino.h>
#include <math.h>
#include <arduinoFFT.h>
#include "driver/i2s_pdm.h"
#include "guition_pins.h"

// --- Audio config ---
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t   FFT_SIZE    = 512;
static constexpr size_t   N_BANDS     = 13;
static constexpr float    PEAK_FLOOR  = 20000.0f;

// --- FFT buffers ---
static double v_re[FFT_SIZE];
static double v_im[FFT_SIZE];
static int16_t wave_buf[FFT_SIZE];
static ArduinoFFT<double> fft(v_re, v_im, FFT_SIZE, SAMPLE_RATE);

// --- Band layout ---
static int   band_lo[N_BANDS];
static int   band_hi[N_BANDS];
static float band_mag[N_BANDS] = { 0 };
static float peak_running       = 100.0f;

// --- Transient detect ---
static constexpr size_t TRANSIENT_HIST = 20;
static float   energy_hist[TRANSIENT_HIST] = { 0 };
static size_t  energy_idx = 0;
static uint32_t last_transient_ms = 0;
static constexpr uint32_t TRANSIENT_COOLDOWN_MS = 80;
static constexpr float    TRANSIENT_THRESHOLD   = 1.5f;

// --- PDM channel ---
static i2s_chan_handle_t rx_chan;

// --- Exported frame ---
static AudioFrame current_frame;

static void compute_band_layout() {
    constexpr float F_MIN  = 100.0f;
    constexpr float F_MAX  = 7000.0f;
    constexpr float BIN_HZ = (float)SAMPLE_RATE / FFT_SIZE;
    for (size_t i = 0; i <= N_BANDS; i++) {
        float f   = F_MIN * powf(F_MAX / F_MIN, (float)i / N_BANDS);
        int   bin = (int)roundf(f / BIN_HZ);
        if (i < N_BANDS) band_lo[i]     = bin;
        if (i > 0)       band_hi[i - 1] = bin;
    }
}

void audio_pipeline_init() {
    // PDM RX = I2S_NUM_0 obligatoire sur ESP32-S3.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_chan));

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                   I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = (gpio_num_t)PIN_MIC_SCK,
            .din = (gpio_num_t)PIN_MIC_DATA,
            .invert_flags = { .clk_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    compute_band_layout();

    current_frame.wave     = wave_buf;
    current_frame.fft_mag  = v_re;
    current_frame.bands    = band_mag;
}

static bool detect_transient(float total_energy, uint32_t now_ms) {
    float sum = 0;
    for (size_t i = 0; i < TRANSIENT_HIST; i++) sum += energy_hist[i];
    float avg = sum / TRANSIENT_HIST;
    energy_hist[energy_idx] = total_energy;
    energy_idx = (energy_idx + 1) % TRANSIENT_HIST;

    if (total_energy > TRANSIENT_THRESHOLD * avg
        && (now_ms - last_transient_ms) > TRANSIENT_COOLDOWN_MS) {
        last_transient_ms = now_ms;
        return true;
    }
    return false;
}

const AudioFrame& audio_pipeline_tick() {
    // 1. Capture
    size_t bytes_read = 0;
    i2s_channel_read(rx_chan, wave_buf, sizeof(wave_buf), &bytes_read, portMAX_DELAY);

    // 2. DC removal + cast double
    double mean = 0;
    for (size_t i = 0; i < FFT_SIZE; i++) mean += wave_buf[i];
    mean /= FFT_SIZE;
    for (size_t i = 0; i < FFT_SIZE; i++) {
        v_re[i] = (double)wave_buf[i] - mean;
        v_im[i] = 0.0;
    }

    // 3. FFT
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    // 4. Bandes log + peak follower
    float new_mags[N_BANDS];
    float max_band     = 0;
    float total_energy = 0;
    for (size_t i = 0; i < N_BANDS; i++) {
        double sum = 0;
        int    n   = band_hi[i] - band_lo[i];
        if (n < 1) n = 1;
        for (int bin = band_lo[i]; bin < band_hi[i]; bin++) sum += v_re[bin];
        new_mags[i] = (float)(sum / n);
        if (new_mags[i] > max_band) max_band = new_mags[i];
        total_energy += new_mags[i];
    }

    if (max_band > peak_running) peak_running = max_band;
    else                          peak_running = peak_running * 0.97f + max_band * 0.03f;
    if (peak_running < PEAK_FLOOR) peak_running = PEAK_FLOOR;

    constexpr float DB_RANGE = 40.0f;
    float rms_acc = 0;
    for (size_t i = 0; i < N_BANDS; i++) {
        float mag = new_mags[i];
        float norm;
        if (mag < 1.0f) {
            norm = 0.0f;
        } else {
            float db = 20.0f * log10f(mag / peak_running);
            norm = 1.0f + db / DB_RANGE;
            if (norm > 1.0f) norm = 1.0f;
            if (norm < 0.0f) norm = 0.0f;
        }
        if (norm > band_mag[i]) band_mag[i] = norm;
        else                    band_mag[i] = band_mag[i] * 0.75f + norm * 0.25f;
        rms_acc += band_mag[i];
    }

    // 5. Transient
    uint32_t now = millis();
    bool t = detect_transient(total_energy, now);

    current_frame.rms       = rms_acc / N_BANDS;
    current_frame.transient = t;
    current_frame.t_ms      = now;
    return current_frame;
}
```

- [ ] **Step 3: Build**

Run: `./build.sh guition_knob Basic_Audio_Multiviz`
Expected: build succeed.

- [ ] **Step 4: Vérification temporaire transient detect via Serial**

Modifier temporairement `main.cpp` pour logger les transients :

```cpp
#include "audio_pipeline.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    guition_lvgl_init(72);
    audio_pipeline_init();
}

void loop() {
    const AudioFrame& af = audio_pipeline_tick();
    if (af.transient) Serial.printf("[%lu] TRANSIENT rms=%.2f\n", af.t_ms, af.rms);
    lv_timer_handler();
}
```

Flash + ouvrir `pio device monitor -b 115200`, taper des mains près du mic ou jouer de la musique. Vérifier que les transients sont loggés sur les kicks/claps (pas en continu, pas absents sur claps clairs).

Run: `./build.sh guition_knob Basic_Audio_Multiviz --upload`

- [ ] **Step 5: Restaurer `main.cpp` au stub minimal (sans le log temporaire)**

Restaurer le stub de Task 1, Step 3 (sans `audio_pipeline_init/tick`). On rebrachera l'audio dans Task 5.

- [ ] **Step 6: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/audio_pipeline.h \
        devices/guition_knob/projects/Basic_Audio_Multiviz/src/audio_pipeline.cpp \
        devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp
git commit -m "Multiviz: extract audio pipeline (mic PDM + FFT + bands + transient)"
```

---

## Task 4: viz_spectrum_radial (#1) — port du viz existant

**Files:**
- Create: `src/viz_spectrum_radial.cpp`

- [ ] **Step 1: Écrire `viz_spectrum_radial.cpp`**

```cpp
#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BANDS  = 13;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int BAR_RMIN = 40;
static constexpr int BAR_RMAX = 172;
static constexpr int BAR_WIDTH = 16;

static lv_obj_t   *bars[N_BANDS];
static lv_point_t  bar_pts[N_BANDS][2];
static lv_style_t  bar_styles[N_BANDS];

static void viz_init() {
    for (int i = 0; i < N_BANDS; i++) {
        float ang = -90.0f + (360.0f * i / N_BANDS);
        float r   = ang * (float)M_PI / 180.0f;
        float c = cosf(r), s = sinf(r);

        bar_pts[i][0].x = CENTER_X + (int)(BAR_RMIN * c);
        bar_pts[i][0].y = CENTER_Y + (int)(BAR_RMIN * s);
        bar_pts[i][1].x = CENTER_X + (int)((BAR_RMIN + 4) * c);
        bar_pts[i][1].y = CENTER_Y + (int)((BAR_RMIN + 4) * s);

        uint16_t hue = (uint16_t)((float)i / N_BANDS * 280.0f);  // 0..280°
        lv_color_t col = lv_color_hsv_to_rgb(hue, 100, 100);

        lv_style_init(&bar_styles[i]);
        lv_style_set_line_width(&bar_styles[i], BAR_WIDTH);
        lv_style_set_line_color(&bar_styles[i], col);
        lv_style_set_line_rounded(&bar_styles[i], true);

        bars[i] = lv_line_create(lv_scr_act());
        lv_line_set_points(bars[i], bar_pts[i], 2);
        lv_obj_add_style(bars[i], &bar_styles[i], 0);
    }
}

static void viz_render(const AudioFrame& af) {
    for (int i = 0; i < N_BANDS; i++) {
        float ang = -90.0f + (360.0f * i / N_BANDS);
        float r   = ang * (float)M_PI / 180.0f;
        float c = cosf(r), s = sinf(r);

        int len = BAR_RMIN + (int)((BAR_RMAX - BAR_RMIN) * af.bands[i]);
        bar_pts[i][1].x = CENTER_X + (int)(len * c);
        bar_pts[i][1].y = CENTER_Y + (int)(len * s);
        lv_line_set_points(bars[i], bar_pts[i], 2);

        uint16_t hue = (uint16_t)((float)i / N_BANDS * 0.78f * 65535.0f);
        uint8_t  val = (uint8_t)(af.bands[i] * 255.0f);
        rgb_ring_set_hsv(i, hue, 255, val);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_BANDS; i++) {
        if (bars[i]) { lv_obj_del(bars[i]); bars[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_SPECTRUM_RADIAL = {
    "Spectrum Radial", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Build (encore non utilisé, on vérifie juste qu'il compile)**

Run: `./build.sh guition_knob Basic_Audio_Multiviz`
Expected: warning unused `VIZ_SPECTRUM_RADIAL` peut apparaître, c'est OK.

- [ ] **Step 3: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/viz_spectrum_radial.cpp
git commit -m "Multiviz: add viz #1 Spectrum Radial (port from Basic_Audio_Visualizer)"
```

---

## Task 5: main.cpp — registry + encoder + dispatch (avec viz #1 seule)

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Réécrire `main.cpp` complet**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_lvgl.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"
#include "rgb_ring.h"
#include "viz_api.h"
#include "audio_pipeline.h"

// Instance globale de l'anneau (déclarée extern dans rgb_ring.h)
Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

// Forward decls des vizs (chacune dans son propre .cpp)
extern const Visualizer VIZ_SPECTRUM_RADIAL;

// Registry — ordre = ordre de cyclage
static const Visualizer* visualizers[] = {
    &VIZ_SPECTRUM_RADIAL,
};
static constexpr int N_VIZ = sizeof(visualizers) / sizeof(visualizers[0]);

static int current_viz = 0;

// Encoder delta (modifié dans ISR-context via callback, lu/reset dans loop)
static volatile int32_t enc_delta = 0;

static int32_t encoder_consume_delta() {
    int32_t d = enc_delta;
    enc_delta = 0;
    return d;
}

static void switch_viz(int delta) {
    visualizers[current_viz]->deinit();
    current_viz = (current_viz + delta + N_VIZ) % N_VIZ;
    visualizers[current_viz]->init();
    Serial.printf("Switch -> %d/%d  %s\n", current_viz + 1, N_VIZ,
                  visualizers[current_viz]->name);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Basic_Audio_Multiviz starting...");

    guition_lvgl_init(72);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    rgb_ring_init(200);

    audio_pipeline_init();

    // Encoder
    knob_config_t enc_cfg = {
        .gpio_encoder_a = PIN_ENC_A,
        .gpio_encoder_b = PIN_ENC_B,
    };
    knob_handle_t knob = iot_knob_create(&enc_cfg);
    iot_knob_register_cb(knob, KNOB_RIGHT, [](void *, void *) { enc_delta++; }, NULL);
    iot_knob_register_cb(knob, KNOB_LEFT,  [](void *, void *) { enc_delta--; }, NULL);

    // Démarre sur viz #1
    visualizers[current_viz]->init();

    Serial.printf("Ready. Active viz: %s\n", visualizers[current_viz]->name);
}

void loop() {
    const AudioFrame& af = audio_pipeline_tick();

    int32_t d = encoder_consume_delta();
    if (d != 0) switch_viz(d);

    visualizers[current_viz]->render(af);
    rgb_ring_show();

    lv_timer_handler();
}
```

- [ ] **Step 2: Build + Flash**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
```

Expected build : succeed. Expected runtime : écran affiche les 13 barres radiales réactives au mic, identique à `Basic_Audio_Visualizer`. Tourner l'encodeur log `Switch -> 1/1 ...` (puisqu'il n'y a qu'une viz, on revient toujours sur elle, mais ça vérifie que l'encoder + le dispatch fonctionnent).

- [ ] **Step 3: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp
git commit -m "Multiviz: wire main loop (audio pipeline + encoder + viz dispatch)"
```

---

## Task 6: OSD — label fade in/out

**Files:**
- Create: `src/osd.h`
- Create: `src/osd.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `osd.h`**

```cpp
#pragma once

void osd_init(int total_vizs);
void osd_show(int viz_index, const char* viz_name);
void osd_tick();  // appelée chaque frame, gère le fade
```

- [ ] **Step 2: Écrire `osd.cpp`**

```cpp
#include "osd.h"

#include <Arduino.h>
#include "lvgl.h"

static lv_obj_t* container = nullptr;
static lv_obj_t* label     = nullptr;
static int       total     = 0;
static uint32_t  shown_at  = 0;
static bool      active    = false;

static constexpr uint32_t HOLD_MS   = 800;
static constexpr uint32_t FADE_MS   = 400;
static constexpr uint32_t TOTAL_MS  = HOLD_MS + FADE_MS;

void osd_init(int total_vizs) {
    total = total_vizs;

    // Container fond noir semi-transparent sur layer top
    container = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, 220, 40);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_radius(container, 8, 0);
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(container, 6, 0);
    lv_obj_set_style_border_width(container, 0, 0);

    label = lv_label_create(container);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "");

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
}

void osd_show(int viz_index, const char* viz_name) {
    if (!container) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "%d/%d  -  %s", viz_index + 1, total, viz_name);
    lv_label_set_text(label, buf);
    lv_obj_set_style_bg_opa(container, LV_OPA_80, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    shown_at = millis();
    active   = true;
}

void osd_tick() {
    if (!active || !container) return;
    uint32_t dt = millis() - shown_at;
    if (dt >= TOTAL_MS) {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        active = false;
        return;
    }
    if (dt > HOLD_MS) {
        // fade out linéaire 80%..0
        float t = (float)(dt - HOLD_MS) / FADE_MS;  // 0..1
        uint8_t bg_opa  = (uint8_t)((1.0f - t) * LV_OPA_80);
        uint8_t txt_opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_bg_opa(container, bg_opa, 0);
        lv_obj_set_style_text_opa(label, txt_opa, 0);
    }
}
```

- [ ] **Step 3: Modifier `main.cpp` pour intégrer l'OSD**

Ajouter en haut :

```cpp
#include "osd.h"
```

Dans `setup()`, après `audio_pipeline_init()` :

```cpp
osd_init(N_VIZ);
```

Dans `switch_viz()`, après `init()` du nouveau viz :

```cpp
osd_show(current_viz, visualizers[current_viz]->name);
```

Dans `loop()`, avant `lv_timer_handler()` :

```cpp
osd_tick();
```

- [ ] **Step 4: Build + Flash + Observe**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
```

Tourner l'encodeur. L'OSD `1/1 — Spectrum Radial` doit apparaître en haut, rester ~800 ms, puis fade out ~400 ms.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/osd.h \
        devices/guition_knob/projects/Basic_Audio_Multiviz/src/osd.cpp \
        devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp
git commit -m "Multiviz: add OSD label with fade on viz switch"
```

---

## Task 7: Haptic tick sur switch

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Ajouter DRV2605 dans `main.cpp`**

En haut, après `#include <Arduino.h>` :

```cpp
#include <Wire.h>
#include <Adafruit_DRV2605.h>
```

Globals après les forward decls :

```cpp
static Adafruit_DRV2605 drv;
static bool drv_ok = false;
```

Dans `setup()`, après `audio_pipeline_init()` (et avant `osd_init`) :

```cpp
Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
if (drv.begin()) {
    drv.useLRA();
    drv.selectLibrary(6);
    drv.setMode(DRV2605_MODE_INTTRIG);
    drv_ok = true;
    Serial.println("DRV2605 OK");
} else {
    Serial.println("DRV2605 not found — haptics disabled");
}
```

Dans `switch_viz()`, après `osd_show()` :

```cpp
if (drv_ok) {
    drv.setWaveform(0, 7);  // Soft Bump
    drv.setWaveform(1, 0);
    drv.go();
}
```

- [ ] **Step 2: Build + Flash + Observe**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
```

Au boot, lire le serial : `DRV2605 OK` ou `DRV2605 not found — haptics disabled`. Si OK, tourner l'encodeur et sentir un petit tick à chaque switch.

- [ ] **Step 3: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp
git commit -m "Multiviz: haptic tick on viz switch (DRV2605 probe + Soft Bump)"
```

---

## Task 8: viz_oscillo (#2)

**Files:**
- Create: `src/viz_oscillo.cpp`
- Modify: `src/main.cpp` (ajouter à la registry)

- [ ] **Step 1: Écrire `viz_oscillo.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_POINTS = 512;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int AMPLITUDE = 140;  // ± pixels autour du centre

static lv_obj_t*   line = nullptr;
static lv_point_t  pts[N_POINTS];
static lv_style_t  style;

static void viz_init() {
    lv_style_init(&style);
    lv_style_set_line_width(&style, 2);
    lv_style_set_line_color(&style, lv_color_make(0, 255, 80));  // vert phosphor
    lv_style_set_line_rounded(&style, true);

    for (int i = 0; i < N_POINTS; i++) {
        pts[i].x = (int)((float)i / (N_POINTS - 1) * (LCD_H_RES - 1));
        pts[i].y = CENTER_Y;
    }
    line = lv_line_create(lv_scr_act());
    lv_line_set_points(line, pts, N_POINTS);
    lv_obj_add_style(line, &style, 0);
}

static void viz_render(const AudioFrame& af) {
    for (int i = 0; i < N_POINTS; i++) {
        int16_t s = af.wave[i];
        // Scale s (~int16) en pixels — diviseur empirique, à tuner.
        int dy = (int)((float)s * AMPLITUDE / 12000.0f);
        if (dy > AMPLITUDE)  dy = AMPLITUDE;
        if (dy < -AMPLITUDE) dy = -AMPLITUDE;
        pts[i].y = CENTER_Y + dy;
    }
    lv_line_set_points(line, pts, N_POINTS);

    uint8_t v = (uint8_t)(af.rms * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        rgb_ring_set(i, 0, v, (uint8_t)(v / 4));
    }
}

static void viz_deinit() {
    if (line) { lv_obj_del(line); line = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_OSCILLO = {
    "Oscilloscope", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry dans `main.cpp`**

Ajouter le forward decl :

```cpp
extern const Visualizer VIZ_OSCILLO;
```

Ajouter à `visualizers[]` :

```cpp
static const Visualizer* visualizers[] = {
    &VIZ_SPECTRUM_RADIAL,
    &VIZ_OSCILLO,
};
```

- [ ] **Step 3: Build + Flash + Observe**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
```

Tourner l'encodeur 1 cran : passage à l'oscillo (ligne verte horizontale qui ondule au son). Tourner encore : retour à spectrum radial. OSD doit afficher `2/2 — Oscilloscope`.

- [ ] **Step 4: Commit**

```bash
git add devices/guition_knob/projects/Basic_Audio_Multiviz/src/viz_oscillo.cpp \
        devices/guition_knob/projects/Basic_Audio_Multiviz/src/main.cpp
git commit -m "Multiviz: add viz #2 Oscilloscope (raw waveform, green phosphor)"
```

---

## Task 9: viz_spectrum_bars (#3)

**Files:**
- Create: `src/viz_spectrum_bars.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_spectrum_bars.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BARS    = 13;
static constexpr int BAR_W     = 22;
static constexpr int GAP       = 4;
static constexpr int BAR_H_MAX = 300;
static constexpr int BASELINE_Y = LCD_V_RES - 30;  // bas de l'écran

static lv_obj_t* bars[N_BARS];
static lv_obj_t* caps[N_BARS];
static float     cap_y_norm[N_BARS] = { 0 };
static float     cap_vel[N_BARS]    = { 0 };

static void viz_init() {
    int total_w = N_BARS * BAR_W + (N_BARS - 1) * GAP;
    int x0      = (LCD_H_RES - total_w) / 2;

    for (int i = 0; i < N_BARS; i++) {
        int x = x0 + i * (BAR_W + GAP);

        bars[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(bars[i]);
        lv_obj_set_size(bars[i], BAR_W, 4);
        lv_obj_set_pos(bars[i], x, BASELINE_Y - 4);
        lv_obj_set_style_radius(bars[i], 3, 0);
        uint16_t hue = (uint16_t)((float)i / N_BARS * 280.0f);
        lv_obj_set_style_bg_color(bars[i], lv_color_hsv_to_rgb(hue, 100, 100), 0);
        lv_obj_set_style_bg_opa(bars[i], LV_OPA_COVER, 0);

        caps[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(caps[i]);
        lv_obj_set_size(caps[i], BAR_W, 3);
        lv_obj_set_pos(caps[i], x, BASELINE_Y - 4);
        lv_obj_set_style_bg_color(caps[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(caps[i], LV_OPA_COVER, 0);
    }
}

static void viz_render(const AudioFrame& af) {
    int x0 = (LCD_H_RES - (N_BARS * BAR_W + (N_BARS - 1) * GAP)) / 2;
    for (int i = 0; i < N_BARS; i++) {
        float h_norm = af.bands[i];
        int   h_px   = (int)(h_norm * BAR_H_MAX);
        if (h_px < 4) h_px = 4;

        lv_obj_set_size(bars[i], BAR_W, h_px);
        lv_obj_set_pos(bars[i], x0 + i * (BAR_W + GAP), BASELINE_Y - h_px);

        // Cap : suit le haut de la barre, retombe à 0.6 px/frame quand la barre descend
        if (h_norm > cap_y_norm[i]) {
            cap_y_norm[i] = h_norm;
            cap_vel[i]    = 0;
        } else {
            cap_vel[i]    += 0.0008f;  // gravité
            cap_y_norm[i] -= cap_vel[i];
            if (cap_y_norm[i] < 0) { cap_y_norm[i] = 0; cap_vel[i] = 0; }
        }
        int cap_y = BASELINE_Y - (int)(cap_y_norm[i] * BAR_H_MAX) - 3;
        lv_obj_set_pos(caps[i], x0 + i * (BAR_W + GAP), cap_y);

        uint16_t hue = (uint16_t)((float)i / N_BARS * 0.78f * 65535.0f);
        rgb_ring_set_hsv(i, hue, 255, (uint8_t)(h_norm * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_BARS; i++) {
        if (bars[i]) { lv_obj_del(bars[i]); bars[i] = nullptr; }
        if (caps[i]) { lv_obj_del(caps[i]); caps[i] = nullptr; }
        cap_y_norm[i] = 0;
        cap_vel[i]    = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_SPECTRUM_BARS = {
    "Spectrum Bars", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_SPECTRUM_BARS;
// ...
static const Visualizer* visualizers[] = {
    &VIZ_SPECTRUM_RADIAL,
    &VIZ_OSCILLO,
    &VIZ_SPECTRUM_BARS,
};
```

- [ ] **Step 3: Build + Flash + Observe**

Tourner pour atteindre Spectrum Bars. 13 barres verticales depuis le bas, avec petits caps blancs qui retombent doucement.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #3 Spectrum Bars (Winamp classic with peak caps)"
```

---

## Task 10: viz_peak_meter (#4)

**Files:**
- Create: `src/viz_peak_meter.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_peak_meter.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static lv_obj_t* disc  = nullptr;
static lv_obj_t* flash = nullptr;
static uint32_t  flash_until = 0;
static float     hue_deg = 0;

static void viz_init() {
    disc = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 60, 60);
    lv_obj_align(disc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);

    flash = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(flash);
    lv_obj_set_size(flash, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(flash, 0, 0);
    lv_obj_set_style_bg_color(flash, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(flash, LV_OPA_TRANSP, 0);
    flash_until = 0;
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.5f;  // rotation lente
    if (hue_deg >= 360) hue_deg -= 360;

    int size = 30 + (int)(af.rms * 280);  // 30..310 px
    lv_obj_set_size(disc, size, size);
    lv_obj_align(disc, LV_ALIGN_CENTER, 0, 0);
    lv_color_t col = lv_color_hsv_to_rgb((uint16_t)hue_deg, 100, 100);
    lv_obj_set_style_bg_color(disc, col, 0);

    if (af.transient) flash_until = af.t_ms + 80;
    if (af.t_ms < flash_until) {
        lv_obj_set_style_bg_opa(flash, LV_OPA_50, 0);
    } else {
        lv_obj_set_style_bg_opa(flash, LV_OPA_TRANSP, 0);
    }

    uint32_t rgb = lv_color_to32(col);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    uint8_t v = (uint8_t)(af.rms * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        rgb_ring_set(i, (r * v) / 255, (g * v) / 255, (b * v) / 255);
    }
}

static void viz_deinit() {
    if (disc)  { lv_obj_del(disc);  disc  = nullptr; }
    if (flash) { lv_obj_del(flash); flash = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_PEAK_METER = {
    "Peak Meter", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_PEAK_METER;
// ...
static const Visualizer* visualizers[] = {
    &VIZ_SPECTRUM_RADIAL,
    &VIZ_OSCILLO,
    &VIZ_SPECTRUM_BARS,
    &VIZ_PEAK_METER,
};
```

- [ ] **Step 3: Build + Flash + Observe**

Disque qui pulse au RMS, couleur qui tourne, flash blanc sur kicks/claps.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #4 Peak Meter (pulsing disc + transient flash)"
```

---

## Task 11: viz_lissajous (#5)

**Files:**
- Create: `src/viz_lissajous.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_lissajous.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_POINTS = 256;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int RADIUS   = 150;

static lv_obj_t*  line = nullptr;
static lv_point_t pts[N_POINTS];
static lv_style_t style;
static float      hue_deg = 0;

static void viz_init() {
    lv_style_init(&style);
    lv_style_set_line_width(&style, 2);
    lv_style_set_line_color(&style, lv_color_make(0, 220, 255));  // cyan
    lv_style_set_line_rounded(&style, true);

    for (int i = 0; i < N_POINTS; i++) {
        pts[i].x = CENTER_X;
        pts[i].y = CENTER_Y;
    }
    line = lv_line_create(lv_scr_act());
    lv_line_set_points(line, pts, N_POINTS);
    lv_obj_add_style(line, &style, 0);
}

static void viz_render(const AudioFrame& af) {
    for (int i = 0; i < N_POINTS; i++) {
        int16_t x = af.wave[2 * i];
        int16_t y = af.wave[2 * i + 1];
        pts[i].x = CENTER_X + (int)((float)x * RADIUS / 16000.0f);
        pts[i].y = CENTER_Y + (int)((float)y * RADIUS / 16000.0f);
    }
    lv_line_set_points(line, pts, N_POINTS);

    // Anneau : dégradé HSV qui tourne
    hue_deg += 1.5f;
    if (hue_deg >= 360) hue_deg -= 360;
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t h = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, h, 255, 200);
    }
}

static void viz_deinit() {
    if (line) { lv_obj_del(line); line = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_LISSAJOUS = {
    "Lissajous", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_LISSAJOUS;
// ...
    &VIZ_LISSAJOUS,
```

- [ ] **Step 3: Build + Flash + Observe**

Figure XY cyan qui change de forme avec le signal mic. Sans son, ligne sur la diagonale (X≈Y).

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #5 Lissajous (XY scope cyan, ring HSV gradient)"
```

---

## Task 12: viz_tunnel (#6)

**Files:**
- Create: `src/viz_tunnel.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_tunnel.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_RINGS  = 8;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int R_BASE   = 30;
static constexpr int R_STEP   = 20;

// Indices des bandes utilisées (réparties dans le spectre)
static const int band_idx[N_RINGS] = { 0, 1, 3, 5, 7, 9, 11, 12 };

static lv_obj_t* arcs[N_RINGS];
static float     hue_deg = 0;

static void viz_init() {
    for (int i = 0; i < N_RINGS; i++) {
        arcs[i] = lv_arc_create(lv_scr_act());
        int r = R_BASE + i * R_STEP;
        lv_obj_set_size(arcs[i], r * 2, r * 2);
        lv_obj_center(arcs[i]);
        lv_arc_set_bg_angles(arcs[i], 0, 360);
        lv_arc_set_angles(arcs[i], 0, 360);
        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_color(arcs[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arcs[i], 4, LV_PART_INDICATOR);
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.8f;
    if (hue_deg >= 360) hue_deg -= 360;

    for (int i = 0; i < N_RINGS; i++) {
        float b = af.bands[band_idx[i]];
        int width = 2 + (int)(b * 30);  // 2..32 px
        lv_obj_set_style_arc_width(arcs[i], width, LV_PART_INDICATOR);

        float h = hue_deg + i * (360.0f / N_RINGS);
        if (h >= 360) h -= 360;
        lv_obj_set_style_arc_color(arcs[i],
            lv_color_hsv_to_rgb((uint16_t)h, 100, (uint8_t)(40 + b * 60)),
            LV_PART_INDICATOR);
    }

    // Anneau LED : wash hue qui rotate
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_RINGS; i++) {
        if (arcs[i]) { lv_obj_del(arcs[i]); arcs[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_TUNNEL = {
    "Tunnel", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_TUNNEL;
// ...
    &VIZ_TUNNEL,
```

- [ ] **Step 3: Build + Flash + Observe**

8 anneaux concentriques qui changent d'épaisseur avec les bandes, hue qui tourne.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #6 Tunnel (8 concentric rings, width per band)"
```

---

## Task 13: viz_beat_bloom (#7)

**Files:**
- Create: `src/viz_beat_bloom.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_beat_bloom.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BLOOMS = 4;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int R_MAX    = 175;
static constexpr uint32_t LIFE_MS = 600;

static lv_obj_t* circles[N_BLOOMS];
static uint32_t  born_at[N_BLOOMS] = { 0 };
static uint16_t  hue_at[N_BLOOMS]  = { 0 };
static int       next_slot = 0;

static int dominant_band(const float* bands, int n) {
    int best = 0; float v = bands[0];
    for (int i = 1; i < n; i++) if (bands[i] > v) { v = bands[i]; best = i; }
    return best;
}

static void viz_init() {
    for (int i = 0; i < N_BLOOMS; i++) {
        circles[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(circles[i]);
        lv_obj_set_size(circles[i], 1, 1);
        lv_obj_align(circles[i], LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(circles[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(circles[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(circles[i], 4, 0);
        lv_obj_set_style_border_opa(circles[i], LV_OPA_TRANSP, 0);
        born_at[i] = 0;
    }
    next_slot = 0;
}

static void viz_render(const AudioFrame& af) {
    if (af.transient) {
        int band = dominant_band(af.bands, 13);
        hue_at[next_slot]  = (uint16_t)((float)band / 13 * 280.0f);
        born_at[next_slot] = af.t_ms;
        next_slot = (next_slot + 1) % N_BLOOMS;
    }

    for (int i = 0; i < N_BLOOMS; i++) {
        if (born_at[i] == 0) continue;
        uint32_t age = af.t_ms - born_at[i];
        if (age > LIFE_MS) {
            lv_obj_set_style_border_opa(circles[i], LV_OPA_TRANSP, 0);
            born_at[i] = 0;
            continue;
        }
        float t = (float)age / LIFE_MS;     // 0..1
        int   r = (int)(t * R_MAX);
        lv_obj_set_size(circles[i], r * 2, r * 2);
        lv_obj_align(circles[i], LV_ALIGN_CENTER, 0, 0);
        uint8_t opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_border_color(circles[i],
            lv_color_hsv_to_rgb(hue_at[i], 100, 100), 0);
        lv_obj_set_style_border_opa(circles[i], opa, 0);
    }

    // Anneau : flash blanc sur transient, sinon noir
    uint8_t v = af.transient ? 255 : 0;
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) rgb_ring_set(i, v, v, v);
}

static void viz_deinit() {
    for (int i = 0; i < N_BLOOMS; i++) {
        if (circles[i]) { lv_obj_del(circles[i]); circles[i] = nullptr; }
        born_at[i] = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_BEAT_BLOOM = {
    "Beat Bloom", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_BEAT_BLOOM;
// ...
    &VIZ_BEAT_BLOOM,
```

- [ ] **Step 3: Build + Flash + Observe**

À chaque kick/clap : un cercle qui s'étend du centre, couleur ∝ bande dominante, anneau LED flashe blanc.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #7 Beat Bloom (transient-triggered expanding rings)"
```

---

## Task 14: viz_starfield (#8)

**Files:**
- Create: `src/viz_starfield.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_starfield.cpp`**

```cpp
#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_PARTS  = 24;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr float R_MAX  = 175.0f;

static lv_obj_t* parts[N_PARTS];
static float     r_pos[N_PARTS];  // 0..R_MAX
static float     ang[N_PARTS];    // radians
static uint32_t  boost_until = 0;

static void viz_init() {
    for (int i = 0; i < N_PARTS; i++) {
        parts[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(parts[i]);
        lv_obj_set_size(parts[i], 6, 6);
        lv_obj_set_style_radius(parts[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(parts[i], LV_OPA_COVER, 0);
        ang[i]   = (float)i * 2 * (float)M_PI / N_PARTS;
        r_pos[i] = (float)(i * 7 % (int)R_MAX);  // étalé au départ
        uint16_t hue = (uint16_t)((float)i / N_PARTS * 360.0f);
        lv_obj_set_style_bg_color(parts[i],
            lv_color_hsv_to_rgb(hue, 100, 100), 0);
    }
}

static void viz_render(const AudioFrame& af) {
    if (af.transient) boost_until = af.t_ms + 200;
    float bass = af.bands[0] + af.bands[1] + af.bands[2];
    float speed = 1.0f + bass * 6.0f;  // ~1..3 px/frame
    if (af.t_ms < boost_until) speed *= 1.5f;

    for (int i = 0; i < N_PARTS; i++) {
        r_pos[i] += speed;
        if (r_pos[i] > R_MAX) r_pos[i] = 0;
        int x = CENTER_X + (int)(r_pos[i] * cosf(ang[i])) - 3;
        int y = CENTER_Y + (int)(r_pos[i] * sinf(ang[i])) - 3;
        lv_obj_set_pos(parts[i], x, y);
    }

    // Anneau : 13 bandes (mirror)
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hue = (uint16_t)((float)i / RGB_RING_LED_COUNT * 0.78f * 65535.0f);
        rgb_ring_set_hsv(i, hue, 255, (uint8_t)(af.bands[i] * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_PARTS; i++) {
        if (parts[i]) { lv_obj_del(parts[i]); parts[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_STARFIELD = {
    "Starfield", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_STARFIELD;
// ...
    &VIZ_STARFIELD,
```

- [ ] **Step 3: Build + Flash + Observe**

24 particules colorées qui jaillissent du centre, vitesse ∝ basses, accélération sur transients.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #8 Starfield (radial particles, speed ~ bass)"
```

---

## Task 15: viz_geiss_stripes (#9)

**Files:**
- Create: `src/viz_geiss_stripes.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_geiss_stripes.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_STRIPES = 13;
static constexpr int STRIPE_H  = LCD_V_RES / N_STRIPES;  // ~27 px

static lv_obj_t* stripes[N_STRIPES];
static float     hue_deg = 0;

static void viz_init() {
    for (int i = 0; i < N_STRIPES; i++) {
        stripes[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(stripes[i]);
        lv_obj_set_size(stripes[i], LCD_H_RES, STRIPE_H + 1);
        lv_obj_set_pos(stripes[i], 0, i * STRIPE_H);
        lv_obj_set_style_bg_opa(stripes[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(stripes[i], lv_color_black(), 0);
        lv_obj_set_style_radius(stripes[i], 0, 0);
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.7f;
    if (hue_deg >= 360) hue_deg -= 360;

    for (int i = 0; i < N_STRIPES; i++) {
        float h = hue_deg + i * (360.0f / N_STRIPES);
        if (h >= 360) h -= 360;
        uint8_t val = (uint8_t)(20 + af.bands[i] * 235.0f);
        lv_obj_set_style_bg_color(stripes[i],
            lv_color_hsv_to_rgb((uint16_t)h, 100, val), 0);
    }

    // Anneau : mirror bandes
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, (uint8_t)(af.bands[i] * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_STRIPES; i++) {
        if (stripes[i]) { lv_obj_del(stripes[i]); stripes[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_GEISS_STRIPES = {
    "Geiss Stripes", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_GEISS_STRIPES;
// ...
    &VIZ_GEISS_STRIPES,
```

- [ ] **Step 3: Build + Flash + Observe**

13 bandes horizontales pleines avec gradient HSV qui rotate, brightness modulée par bandes audio. Effet "hublot" attendu (rogné par le bord rond).

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #9 Geiss Stripes (13 horizontal bands, HSV gradient)"
```

---

## Task 16: viz_g_wave (#10)

**Files:**
- Create: `src/viz_g_wave.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_g_wave.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_WAVES = 6;
static constexpr int R_MAX   = 180;
static constexpr uint32_t LIFE_MS = 2000;
static constexpr uint32_t IDLE_EMIT_MS = 400;

static lv_obj_t* arcs[N_WAVES];
static uint32_t  born_at[N_WAVES] = { 0 };
static uint16_t  hue_at[N_WAVES]  = { 0 };
static int       next_slot = 0;
static uint32_t  last_emit = 0;
static float     hue_emit  = 0;

static void emit_wave(uint32_t now) {
    born_at[next_slot] = now;
    hue_at[next_slot]  = (uint16_t)hue_emit;
    hue_emit += 47;
    if (hue_emit >= 360) hue_emit -= 360;
    next_slot = (next_slot + 1) % N_WAVES;
    last_emit = now;
}

static void viz_init() {
    for (int i = 0; i < N_WAVES; i++) {
        arcs[i] = lv_arc_create(lv_scr_act());
        lv_obj_set_size(arcs[i], 1, 1);
        lv_obj_center(arcs[i]);
        lv_arc_set_bg_angles(arcs[i], 0, 360);
        lv_arc_set_angles(arcs[i], 0, 360);
        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arcs[i], 6, LV_PART_INDICATOR);
        born_at[i] = 0;
    }
    next_slot = 0;
    last_emit = 0;
}

static void viz_render(const AudioFrame& af) {
    if (af.transient || (af.t_ms - last_emit) > IDLE_EMIT_MS) {
        emit_wave(af.t_ms);
    }

    for (int i = 0; i < N_WAVES; i++) {
        if (born_at[i] == 0) continue;
        uint32_t age = af.t_ms - born_at[i];
        if (age > LIFE_MS) {
            lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
            born_at[i] = 0;
            continue;
        }
        float t = (float)age / LIFE_MS;
        int   r = (int)(t * R_MAX);
        lv_obj_set_size(arcs[i], r * 2, r * 2);
        lv_obj_center(arcs[i]);
        uint8_t opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_arc_color(arcs[i],
            lv_color_hsv_to_rgb(hue_at[i], 100, 100), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arcs[i], opa, LV_PART_INDICATOR);
    }

    // Anneau : wash qui rotate sur la même hue d'émission
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_emit + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_WAVES; i++) {
        if (arcs[i]) { lv_obj_del(arcs[i]); arcs[i] = nullptr; }
        born_at[i] = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_G_WAVE = {
    "G-Wave", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_G_WAVE;
// ...
    &VIZ_G_WAVE,
```

- [ ] **Step 3: Build + Flash + Observe**

Anneaux concentriques colorés qui s'étendent du centre, émis sur transients (et toutes les 400 ms en mode calme). Couleurs changent à chaque émission.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #10 G-Wave (expanding rings on transients, lens-flare feel)"
```

---

## Task 17: viz_kaleidoscope (#11)

**Files:**
- Create: `src/viz_kaleidoscope.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_kaleidoscope.cpp`**

```cpp
#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_SEGS    = 8;
static constexpr int N_LINES   = 5;  // par segment
static constexpr int CENTER_X  = LCD_H_RES / 2;
static constexpr int CENTER_Y  = LCD_V_RES / 2;
static constexpr int R_MAX     = 170;

static lv_obj_t*  lines[N_SEGS * N_LINES];
static lv_point_t pts[N_SEGS * N_LINES][2];
static lv_style_t styles[N_SEGS * N_LINES];
static float      hue_deg = 0;

static void viz_init() {
    for (int s = 0; s < N_SEGS; s++) {
        for (int l = 0; l < N_LINES; l++) {
            int i = s * N_LINES + l;
            lv_style_init(&styles[i]);
            lv_style_set_line_width(&styles[i], 2);
            lv_style_set_line_color(&styles[i], lv_color_white());
            lv_style_set_line_rounded(&styles[i], true);

            lines[i] = lv_line_create(lv_scr_act());
            pts[i][0].x = CENTER_X;
            pts[i][0].y = CENTER_Y;
            pts[i][1].x = CENTER_X;
            pts[i][1].y = CENTER_Y;
            lv_line_set_points(lines[i], pts[i], 2);
            lv_obj_add_style(lines[i], &styles[i], 0);
        }
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 1.2f;
    if (hue_deg >= 360) hue_deg -= 360;

    float bass = af.bands[0] + af.bands[1];  // 0..2
    float amp  = 0.2f + 0.8f * (bass / 2.0f);  // 0.2..1.0

    for (int s = 0; s < N_SEGS; s++) {
        float seg_ang = (float)s * 2 * (float)M_PI / N_SEGS;
        for (int l = 0; l < N_LINES; l++) {
            int i = s * N_LINES + l;
            float r0 = (float)(l + 1) / (N_LINES + 1) * R_MAX * amp;
            float r1 = r0 + 24;
            float a0 = seg_ang;
            float a1 = seg_ang + (float)M_PI / N_SEGS * (0.5f + 0.5f * sinf(af.t_ms * 0.001f + l));

            pts[i][0].x = CENTER_X + (int)(r0 * cosf(a0));
            pts[i][0].y = CENTER_Y + (int)(r0 * sinf(a0));
            pts[i][1].x = CENTER_X + (int)(r1 * cosf(a1));
            pts[i][1].y = CENTER_Y + (int)(r1 * sinf(a1));
            lv_line_set_points(lines[i], pts[i], 2);

            float h = hue_deg + l * 40;
            if (h >= 360) h -= 360;
            lv_obj_set_style_line_color(lines[i],
                lv_color_hsv_to_rgb((uint16_t)h, 100, 100), 0);
        }
    }

    // Anneau : hue qui rotate
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_SEGS * N_LINES; i++) {
        if (lines[i]) { lv_obj_del(lines[i]); lines[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_KALEIDOSCOPE = {
    "Kaleidoscope", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_KALEIDOSCOPE;
// ...
    &VIZ_KALEIDOSCOPE,
```

- [ ] **Step 3: Build + Flash + Observe**

8 segments en symétrie radiale, motif qui se déforme avec les basses, couleurs qui rotent.

- [ ] **Step 4: Commit**

```bash
git commit -am "Multiviz: add viz #11 Kaleidoscope (8-fold radial symmetry, deforms with bass)"
```

---

## Task 18: viz_matrix_rain (#12)

**Files:**
- Create: `src/viz_matrix_rain.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Écrire `viz_matrix_rain.cpp`**

```cpp
#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_COLS    = 18;
static constexpr int COL_W     = LCD_H_RES / N_COLS;  // 20 px
static constexpr int CHAR_H    = 18;
static constexpr int CHARS_PER_COL = LCD_V_RES / CHAR_H + 1;  // ~21

static lv_obj_t*  cols[N_COLS];
static lv_style_t col_styles[N_COLS];
static float      head_y[N_COLS];   // 0..LCD_V_RES (position de la "tête")
static float      vel[N_COLS];      // px/frame
// Chaque entrée stocke CHARS_PER_COL caractères séparés par '\n' + nul terminal.
static char       buf[N_COLS][CHARS_PER_COL * 2 + 1];

static char random_char() {
    static const char alpha[] = "0123456789ABCDEFGHJKLMNPRSTUVWXYZ";
    return alpha[esp_random() % (sizeof(alpha) - 1)];
}

static void viz_init() {
    for (int i = 0; i < N_COLS; i++) {
        lv_style_init(&col_styles[i]);
        lv_style_set_text_font(&col_styles[i], &lv_font_montserrat_14);
        lv_style_set_text_color(&col_styles[i], lv_color_make(0, 255, 80));
        lv_style_set_text_line_space(&col_styles[i], 4);

        cols[i] = lv_label_create(lv_scr_act());
        lv_obj_add_style(cols[i], &col_styles[i], 0);
        lv_obj_set_pos(cols[i], i * COL_W + 2, -CHAR_H * (esp_random() % CHARS_PER_COL));
        head_y[i] = -CHAR_H * (esp_random() % CHARS_PER_COL);
        vel[i]    = 1.0f + (esp_random() % 100) / 100.0f;

        for (int j = 0; j < CHARS_PER_COL; j++) {
            buf[i][j * 2]     = random_char();
            buf[i][j * 2 + 1] = '\n';
        }
        buf[i][CHARS_PER_COL * 2 - 1] = '\0';
        lv_label_set_text(cols[i], buf[i]);
    }
}

static void viz_render(const AudioFrame& af) {
    // Vitesse = bandes aigus (8..12) moyennées
    float high = 0;
    for (int i = 8; i < 13; i++) high += af.bands[i];
    high /= 5.0f;
    float global_speed = 0.5f + high * 6.0f;  // 0.5..6.5

    for (int i = 0; i < N_COLS; i++) {
        head_y[i] += vel[i] * global_speed;
        if (head_y[i] > LCD_V_RES + CHAR_H * CHARS_PER_COL) {
            head_y[i] = -CHAR_H * CHARS_PER_COL;
            // Renouvelle quelques chars en haut
            for (int j = 0; j < CHARS_PER_COL; j++) {
                buf[i][j * 2] = random_char();
            }
            lv_label_set_text(cols[i], buf[i]);
        }
        lv_obj_set_pos(cols[i], i * COL_W + 2, (int)(head_y[i] - CHAR_H * CHARS_PER_COL));
    }

    // Anneau : vert, brightness ∝ high
    uint8_t v = (uint8_t)(high * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) rgb_ring_set(i, 0, v, 0);
}

static void viz_deinit() {
    for (int i = 0; i < N_COLS; i++) {
        if (cols[i]) { lv_obj_del(cols[i]); cols[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_MATRIX_RAIN = {
    "Matrix Rain", viz_init, viz_render, viz_deinit
};
```

- [ ] **Step 2: Ajouter à la registry**

```cpp
extern const Visualizer VIZ_MATRIX_RAIN;
// ...
    &VIZ_MATRIX_RAIN,
```

- [ ] **Step 3: Build + Flash + Observe**

18 colonnes de chars verts qui tombent ; vitesse globale augmente sur sons aigus (sifflets, hi-hats).

- [ ] **Step 4: Profilage rapide**

Si framerate visiblement < 15 fps sur Matrix Rain (chars semblent saccader), réduire `N_COLS` à 12 ou throttler le `lv_label_set_text` (seulement sur wrap, déjà fait dans le code).

- [ ] **Step 5: Commit**

```bash
git commit -am "Multiviz: add viz #12 Matrix Rain (vertical falling chars, speed ~ highs)"
```

---

## Task 19: README — pointer la nouvelle démo

**Files:**
- Modify: `devices/guition_knob/README.md`

- [ ] **Step 1: Repérer la section "Projects"**

```bash
grep -n "Basic_Audio_Visualizer" devices/guition_knob/README.md
```

- [ ] **Step 2: Ajouter une ligne pour `Basic_Audio_Multiviz` juste après**

Inserer après la ligne mentionnant `Basic_Audio_Visualizer` :

```
- `Basic_Audio_Multiviz` — 12 visualisations audio cyclées à la molette (esprit Winamp AVS) : spectrum radial, oscillo, bars, peak meter, lissajous, tunnel, beat bloom, starfield, geiss stripes, g-wave, kaléidoscope, matrix rain. Avec OSD et tick haptique sur switch.
```

- [ ] **Step 3: Commit**

```bash
git commit -am "Guition README: list Basic_Audio_Multiviz"
```

---

## Task 20: Test final — cycling stress + memory leak

**Files:** aucun (vérification seule)

- [ ] **Step 1: Ajouter temporairement un log heap dans `setup()`**

Dans `main.cpp`, en fin de `setup()` :

```cpp
Serial.printf("Free heap at boot: %u\n", ESP.getFreeHeap());
```

Et dans `switch_viz()`, après tout :

```cpp
Serial.printf("Free heap after switch: %u\n", ESP.getFreeHeap());
```

- [ ] **Step 2: Flash + tester un cycling intensif**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
pio device monitor -b 115200
```

Tourner l'encodeur ~50 fois (cycler les 12 vizs ≥ 4 tours complets). Observer :
- Pas de freeze visible sur les switches.
- Free heap reste stable (variation < 1 KB d'un switch à l'autre — sinon fuite).
- OSD affiche correctement chaque viz.
- Tick haptique régulier (si DRV présent).
- Pas de crash / reboot.

- [ ] **Step 3: Retirer les logs heap temporaires**

Supprimer les 2 lignes `Serial.printf` ajoutées au Step 1.

- [ ] **Step 4: Build final + Flash**

```bash
./build.sh guition_knob Basic_Audio_Multiviz --upload
```

- [ ] **Step 5: Commit propre**

```bash
git commit -am "Multiviz: final cleanup (remove debug heap logs)"
```

---

## Definition of done (recap)

- [ ] Build sans warning critique.
- [ ] Les 12 vizs sont accessibles via rotation de l'encodeur, wrap circulaire.
- [ ] OSD affiche `"N/12 — Nom"` pendant ~1.2 s sur chaque switch.
- [ ] Tick haptique sur switch si DRV2605 présent (sinon silencieux, sans crash).
- [ ] Cycling intensif (50+ switches) sans fuite mémoire (free heap stable).
- [ ] `Basic_Audio_Visualizer` reste intact (build et fonctionne identiquement).
- [ ] README pointe la nouvelle démo.
