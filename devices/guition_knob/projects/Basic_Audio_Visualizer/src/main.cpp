#include <Arduino.h>
#include <math.h>
#include <arduinoFFT.h>
#include "driver/i2s_pdm.h"

#include "guition_lvgl.h"
#include "rgb_ring.h"

// Visualizer audio mic-reactif.
//
// Lit le micro PDM (SCK=5 / Data=4), FFT 512 echantillons a 16 kHz, regroupe
// en 13 bandes log-spaced (100 Hz - 7 kHz) et affiche :
//   - 13 barres radiales sur le LCD rond 360x360, hue rouge (graves) -> violet
//     (aigus), longueur proportionnelle a la magnitude de la bande
//   - 13 LEDs de l'anneau, une par bande, meme couleur, brightness ∝ magnitude
//
// Le mic du Guition n'est utilise par aucune demo vendor : les 2 broches
// (SCK + Data, pas de WS) suggerent une interface PDM, hypothese retenue ici.
// Si le silence ambiant ne produit pas de spectre plat (ou si toute frequence
// reste a 0), c'est probablement un mic I2S Philips a la place — remplacer
// mic_init() par un i2s_channel_init_std_mode().

Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

// --- Audio config ---
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t   FFT_SIZE    = 512;                       // ~32 ms window
static constexpr size_t   N_BANDS     = RGB_RING_LED_COUNT;        // 13

// --- Visual config ---
static constexpr int CENTER_X  = LCD_H_RES / 2;                    // 180
static constexpr int CENTER_Y  = LCD_V_RES / 2;
static constexpr int BAR_RMIN  = 40;
static constexpr int BAR_RMAX  = 172;                              // marge ecran rond
static constexpr int BAR_WIDTH = 16;

// --- FFT buffers ---
static double v_re[FFT_SIZE];
static double v_im[FFT_SIZE];
static ArduinoFFT<double> fft(v_re, v_im, FFT_SIZE, SAMPLE_RATE);

// --- Band layout (range of FFT bins for each band) ---
static int   band_lo[N_BANDS];
static int   band_hi[N_BANDS];
static float band_mag[N_BANDS] = { 0 };
static float peak_running       = 100.0f;

// --- PDM mic channel ---
static i2s_chan_handle_t rx_chan;

// --- LVGL objects ---
static lv_obj_t   *bars[N_BANDS];
static lv_point_t  bar_pts[N_BANDS][2];
static lv_style_t  bar_styles[N_BANDS];

static void compute_band_layout() {
    // 14 frontieres log-spacees -> 13 bandes
    constexpr float F_MIN = 100.0f;
    constexpr float F_MAX = 7000.0f;
    constexpr float BIN_HZ = (float)SAMPLE_RATE / FFT_SIZE;
    for (size_t i = 0; i <= N_BANDS; i++) {
        float f   = F_MIN * powf(F_MAX / F_MIN, (float)i / N_BANDS);
        int   bin = (int)roundf(f / BIN_HZ);
        if (i < N_BANDS) band_lo[i]     = bin;
        if (i > 0)       band_hi[i - 1] = bin;
    }
}

static void mic_init() {
    // PDM RX is only supported on I2S_NUM_0 on the ESP32-S3.
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
}

static void build_ui() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    for (size_t i = 0; i < N_BANDS; i++) {
        float ang_deg = -90.0f + (360.0f * i / N_BANDS);
        float ang_rad = ang_deg * (float)M_PI / 180.0f;
        float c = cosf(ang_rad);
        float s = sinf(ang_rad);

        // Init : barre a longueur minimale (juste un petit point)
        bar_pts[i][0].x = CENTER_X + (int)(BAR_RMIN * c);
        bar_pts[i][0].y = CENTER_Y + (int)(BAR_RMIN * s);
        bar_pts[i][1].x = CENTER_X + (int)((BAR_RMIN + 4) * c);
        bar_pts[i][1].y = CENTER_Y + (int)((BAR_RMIN + 4) * s);

        // Hue : 0 (rouge) -> 0.78 (violet) selon la bande
        uint16_t hue_lvgl = (uint16_t)((float)i / N_BANDS * 280.0f);   // 0..280°
        lv_color_t col    = lv_color_hsv_to_rgb(hue_lvgl, 100, 100);

        lv_style_init(&bar_styles[i]);
        lv_style_set_line_width(&bar_styles[i], BAR_WIDTH);
        lv_style_set_line_color(&bar_styles[i], col);
        lv_style_set_line_rounded(&bar_styles[i], true);

        bars[i] = lv_line_create(lv_scr_act());
        lv_line_set_points(bars[i], bar_pts[i], 2);
        lv_obj_add_style(bars[i], &bar_styles[i], 0);
    }
}

static void capture_audio() {
    int16_t buf[FFT_SIZE];
    size_t  bytes_read = 0;
    i2s_channel_read(rx_chan, buf, sizeof(buf), &bytes_read, portMAX_DELAY);

    // DC removal puis cast vers double
    double mean = 0;
    for (size_t i = 0; i < FFT_SIZE; i++) mean += buf[i];
    mean /= FFT_SIZE;
    for (size_t i = 0; i < FFT_SIZE; i++) {
        v_re[i] = (double)buf[i] - mean;
        v_im[i] = 0.0;
    }
}

static void process_fft() {
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();
}

static void update_bands() {
    float new_mags[N_BANDS];
    float max_band = 0;
    for (size_t i = 0; i < N_BANDS; i++) {
        double sum = 0;
        int    n   = band_hi[i] - band_lo[i];
        if (n < 1) n = 1;
        for (int bin = band_lo[i]; bin < band_hi[i]; bin++) sum += v_re[bin];
        new_mags[i] = (float)(sum / n);
        if (new_mags[i] > max_band) max_band = new_mags[i];
    }

    // Peak follower : attack instant, release ~1 s (recupere vite apres un
    // transitoire fort, sinon tout son normal est ecrase pendant des secondes).
    if (max_band > peak_running) peak_running = max_band;
    else                          peak_running = peak_running * 0.97f + max_band * 0.03f;
    // Floor sur peak_running : evite que le silence ambiant ne remonte tout
    // l'affichage (sinon meme 100 unites de magnitude apparaitraient pleines).
    constexpr float PEAK_FLOOR = 20000.0f;
    if (peak_running < PEAK_FLOOR) peak_running = PEAK_FLOOR;

    // Echelle dB : norm = 1 + db/DB_RANGE, ou db = 20*log10(mag / peak_running).
    // Un son a -0 dB (au peak) touche le bord, a -DB_RANGE dB il disparait.
    // Plus naturel a l'oreille que le lineaire — les bandes moyennes restent
    // visibles meme quand une bande domine.
    constexpr float DB_RANGE = 40.0f;
    for (size_t i = 0; i < N_BANDS; i++) {
        float mag = new_mags[i];
        float norm;
        if (mag < 1.0f) {
            norm = 0.0f;
        } else {
            float db = 20.0f * log10f(mag / peak_running);   // <= 0 typiquement
            norm = 1.0f + db / DB_RANGE;
            if (norm > 1.0f) norm = 1.0f;
            if (norm < 0.0f) norm = 0.0f;
        }
        // Peak-meter : attack instant, release lent
        if (norm > band_mag[i]) band_mag[i] = norm;
        else                    band_mag[i] = band_mag[i] * 0.75f + norm * 0.25f;
    }
}

static void render() {
    for (size_t i = 0; i < N_BANDS; i++) {
        float ang_deg = -90.0f + (360.0f * i / N_BANDS);
        float ang_rad = ang_deg * (float)M_PI / 180.0f;
        float c = cosf(ang_rad);
        float s = sinf(ang_rad);

        int len = BAR_RMIN + (int)((BAR_RMAX - BAR_RMIN) * band_mag[i]);
        bar_pts[i][1].x = CENTER_X + (int)(len * c);
        bar_pts[i][1].y = CENTER_Y + (int)(len * s);
        lv_line_set_points(bars[i], bar_pts[i], 2);

        uint16_t hue = (uint16_t)((float)i / N_BANDS * 0.78f * 65535.0f);
        uint8_t  val = (uint8_t)(band_mag[i] * 255.0f);
        rgb_ring_set_hsv(i, hue, 255, val);
    }
    rgb_ring_show();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Basic_Audio_Visualizer starting...");

    guition_lvgl_init(72);
    rgb_ring_init(200);

    compute_band_layout();
    build_ui();
    mic_init();

    Serial.println("Ready.");
}

void loop() {
    capture_audio();
    process_fft();
    update_bands();
    render();
    lv_timer_handler();
}
