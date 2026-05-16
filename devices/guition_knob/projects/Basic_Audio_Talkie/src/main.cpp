#include <Arduino.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"
#include "bidi_switch_knob.h"
#include "TalkiePCM.h"
#include "Vocab_Soundbites.h"

// TTS robotique LPC (TI TMS5220 des '80s — Speak & Spell). 4 phrases iconic
// du cinema US tournent en boucle, le PCM 8 kHz mono est genere par TalkiePCM
// et streame vers le PCM5100A via callback I2S. Volume via encoder (21 crans,
// 0=mute, defaut 4). I2S configure a 8 kHz (sample rate natif Talkie) — le
// PCM5100A auto-detecte la cadence.

static constexpr uint32_t SAMPLE_RATE     = 8000;
static constexpr float    GAIN_MAX        = 0.50f;
static constexpr int      VOL_STEPS       = 20;
static constexpr int      VOL_DEFAULT     = 4;
static constexpr uint32_t PHRASE_PAUSE_MS = 1500;

static i2s_chan_handle_t i2s_tx;
static knob_handle_t     knob     = nullptr;
static int               vol_step = VOL_DEFAULT;
static volatile float    cur_gain = ((float)VOL_DEFAULT / VOL_STEPS) * GAIN_MAX;

static TalkiePCM voice;

static void talkie_to_i2s(int16_t* data, int len) {
    // len = total samples (frames * channels). data est interleaved L/R quand
    // setChannels(2). On applique le gain en place puis on push en I2S.
    float g = cur_gain;
    for (int i = 0; i < len; i++) {
        data[i] = (int16_t)((float)data[i] * g);
    }
    size_t written;
    i2s_channel_write(i2s_tx, data, len * sizeof(int16_t), &written, portMAX_DELAY);
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

static void poll_volume() {
    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        vol_step += delta;
        if (vol_step < 0)         vol_step = 0;
        if (vol_step > VOL_STEPS) vol_step = VOL_STEPS;
        cur_gain = ((float)vol_step / VOL_STEPS) * GAIN_MAX;
        Serial.printf("Vol : %d/%d\n", vol_step, VOL_STEPS);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Guition JC3636K718 — Basic_Audio_Talkie (LPC robot voice)");

    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    i2s_setup();

    voice.setDataCallback(talkie_to_i2s);
    voice.setChannels(2);    // stéréo interleaved (L=R, l'ampli somme)

    Serial.printf("Vol initial : %d/%d (0=mute, tourner pour ajuster)\n", vol_step, VOL_STEPS);
}

void loop() {
    poll_volume();
    voice.say(spHASTA_LA_VISTA);
    voice.silence(PHRASE_PAUSE_MS);

    poll_volume();
    voice.say(spONE_SMALL_STEP);
    voice.silence(PHRASE_PAUSE_MS);

    poll_volume();
    voice.say(spWHAT_IS_THY_BIDDING);
    voice.silence(PHRASE_PAUSE_MS);

    poll_volume();
    voice.say(spHMMM_BEER);
    voice.silence(PHRASE_PAUSE_MS);
}
