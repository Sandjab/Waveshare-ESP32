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
