#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_DRV2605.h>
#include "lvgl.h"
#include "guition_lvgl.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"
#include "rgb_ring.h"
#include "viz_api.h"
#include "audio_pipeline.h"
#include "osd.h"

// Instance globale de l'anneau (déclarée extern dans rgb_ring.h)
Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

static Adafruit_DRV2605 drv;
static bool drv_ok = false;

// Forward decls des vizs (chacune dans son propre .cpp)
extern const Visualizer VIZ_SPECTRUM_RADIAL;
extern const Visualizer VIZ_OSCILLO;
extern const Visualizer VIZ_SPECTRUM_BARS;
extern const Visualizer VIZ_PEAK_METER;
extern const Visualizer VIZ_LISSAJOUS;
extern const Visualizer VIZ_TUNNEL;
extern const Visualizer VIZ_BEAT_BLOOM;
extern const Visualizer VIZ_STARFIELD;
extern const Visualizer VIZ_GEISS_STRIPES;

// Registry — ordre = ordre de cyclage
static const Visualizer* visualizers[] = {
    &VIZ_SPECTRUM_RADIAL,
    &VIZ_OSCILLO,
    &VIZ_SPECTRUM_BARS,
    &VIZ_PEAK_METER,
    &VIZ_LISSAJOUS,
    &VIZ_TUNNEL,
    &VIZ_BEAT_BLOOM,
    &VIZ_STARFIELD,
    &VIZ_GEISS_STRIPES,
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
    osd_show(current_viz, visualizers[current_viz]->name);
    if (drv_ok) {
        drv.setWaveform(0, 7);  // Soft Bump
        drv.setWaveform(1, 0);
        drv.go();
    }
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

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (drv.begin()) {
        drv.useLRA();
        drv.selectLibrary(6);
        drv.setMode(DRV2605_MODE_INTTRIG);
        drv_ok = true;
        Serial.println("DRV2605 OK");
    } else {
        Serial.println("DRV2605 not found - haptics disabled");
    }

    osd_init(N_VIZ);

    // Encoder
    knob_config_t enc_cfg = {
        .gpio_encoder_a = PIN_ENC_A,
        .gpio_encoder_b = PIN_ENC_B,
    };
    knob_handle_t knob = iot_knob_create(&enc_cfg);
    iot_knob_register_cb(knob, KNOB_RIGHT, [](void *, void *) { enc_delta = enc_delta + 1; }, NULL);
    iot_knob_register_cb(knob, KNOB_LEFT,  [](void *, void *) { enc_delta = enc_delta - 1; }, NULL);

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

    osd_tick();
    lv_timer_handler();
}
