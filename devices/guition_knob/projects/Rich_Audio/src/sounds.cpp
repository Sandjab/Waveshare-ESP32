#include "sounds.h"
#include "sound_harp.h"
#include "sound_ping.h"
#include "sound_toneseq.h"
#include "sound_dualtone.h"

// Library de sons, organisée en catégories (= pages de l'UI).
//
//  - Rich       : générateurs de synthèse dédiés (Harp, Ping), validés sur device.
//  - UI / FX    : presets « système » ORIGINAUX (ToneSeqSound) dans le style des
//                 sons d'interface / jeu — pas des copies de sons propriétaires.
//  - Telephony  : tonalités d'appel FIDÈLES aux normes (faits techniques non
//                 protégeables : Bellcore Precise Tone Plan, ITU-T Q.23 DTMF).
//  - Classics   : mélodies du DOMAINE PUBLIC (compositeurs morts depuis >100 ans),
//                 rendues comme suites de notes — célèbres comme sonneries d'appareils.
//  - Windows/iOS: sons ORIGINAUX évocateurs du style de la plateforme. Les vrais
//                 sons de marque sont protégés et ne sont volontairement pas reproduits.
//
// Le registre g_registry[] est PLAT et ordonné par catégorie ; g_categories[] en
// décrit les tranches contiguës. Garder les deux synchronisés (le test natif vérifie
// que les catégories pavent exactement le registre).

static HarpSound s_harp;
static PingSound s_ping;

// Fréquences notes (Hz) pour lisibilité des tables.
// B3 246.94  C4 261.63  D4 293.66  D#4 311.13  E4 329.63  F4 349.23  F#4 369.99
// G4 392.00  G#4 415.30  A4 440.00  A#4 466.16  B4 493.88  C5 523.25  C#5 554.37
// D5 587.33  D#5 622.25  E5 659.25  F5 698.46  F#5 739.99  G5 783.99  G#5 830.61
// A5 880.00  B5 987.77   C6 1046.50 C#6 1108.73 D6 1174.66 E6 1318.51 G6 1567.98

#define DEFSND(var, dname, notesarr, w, e, g) \
    static const ToneSeqDef var##_def = { dname, notesarr, \
        (uint8_t)(sizeof(notesarr) / sizeof(notesarr[0])), w, e, g }; \
    static ToneSeqSound var(&var##_def)

#define DEFDUAL(var, dname, notesarr, g) \
    static const DualToneDef var##_def = { dname, notesarr, \
        (uint8_t)(sizeof(notesarr) / sizeof(notesarr[0])), g }; \
    static DualToneSound var(&var##_def)

// ---------- UI / FX : presets système originaux (ToneSeqSound) ----------
static const ToneNote N_beep[]      = { {800,150} };
static const ToneNote N_beep2[]     = { {800,90},{0,60},{800,90} };
static const ToneNote N_ding[]      = { {1046.50,350} };
static const ToneNote N_dingdong[]  = { {659.26,260},{523.25,420} };
static const ToneNote N_notify[]    = { {880.00,90},{0,30},{1046.50,240} };
static const ToneNote N_message[]   = { {1046.50,80},{1318.51,220} };
static const ToneNote N_pop[]       = { {783.99,90} };
static const ToneNote N_tick[]      = { {2000.00,18} };
static const ToneNote N_chime[]     = { {783.99,200},{1046.50,200},{1318.51,520} };
static const ToneNote N_bell[]      = { {880.00,750} };
static const ToneNote N_confirm[]   = { {523.25,100},{783.99,260} };
static const ToneNote N_cancel[]    = { {783.99,100},{523.25,280} };
static const ToneNote N_tada[]      = { {523.25,120},{659.26,120},{783.99,120},{1046.50,520} };
static const ToneNote N_error[]     = { {220.00,200},{0,40},{207.65,320} };
static const ToneNote N_coin[]      = { {987.77,80},{1318.51,320} };
static const ToneNote N_powerup[]   = { {523.25,70},{659.26,70},{783.99,70},{1046.50,70},{1318.51,160} };
static const ToneNote N_powerdn[]   = { {1318.51,70},{1046.50,70},{783.99,70},{659.26,70},{523.25,200} };
static const ToneNote N_sweepup[]   = { {400,45},{520,45},{660,45},{820,45},{1000,45},{1250,90} };
static const ToneNote N_sweepdn[]   = { {1250,45},{1000,45},{820,45},{660,45},{520,45},{400,90} };
static const ToneNote N_alarm[]     = { {880,200},{660,200},{880,200},{660,200} };
static const ToneNote N_lowbatt[]   = { {880,150},{0,80},{698.46,360} };
static const ToneNote N_unlock[]    = { {659.26,90},{987.77,240} };

DEFSND(s_beep,     "Beep",       N_beep,     TW_SQUARE, TE_PAD,   0.70f);
DEFSND(s_beep2,    "Beep x2",    N_beep2,    TW_SQUARE, TE_PAD,   0.70f);
DEFSND(s_ding,     "Ding",       N_ding,     TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_dingdong, "Ding Dong",  N_dingdong, TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_notify,   "Notify",     N_notify,   TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_message,  "Message",    N_message,  TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_pop,      "Pop",        N_pop,      TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_tick,     "Tick",       N_tick,     TW_SQUARE, TE_PAD,   0.55f);
DEFSND(s_chime,    "Chime",      N_chime,    TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_bell,     "Bell",       N_bell,     TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_confirm,  "Confirm",    N_confirm,  TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_cancel,   "Cancel",     N_cancel,   TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_tada,     "Tada",       N_tada,     TW_TRI,    TE_PLUCK, 0.85f);
DEFSND(s_error,    "Error",      N_error,    TW_SQUARE, TE_PAD,   0.75f);
DEFSND(s_coin,     "Coin",       N_coin,     TW_SQUARE, TE_PLUCK, 0.65f);
DEFSND(s_powerup,  "Power Up",   N_powerup,  TW_SQUARE, TE_PLUCK, 0.65f);
DEFSND(s_powerdn,  "Power Down", N_powerdn,  TW_SQUARE, TE_PLUCK, 0.65f);
DEFSND(s_sweepup,  "Sweep Up",   N_sweepup,  TW_SINE,   TE_PAD,   0.95f);
DEFSND(s_sweepdn,  "Sweep Down", N_sweepdn,  TW_SINE,   TE_PAD,   0.95f);
DEFSND(s_alarm,    "Alarm",      N_alarm,    TW_SQUARE, TE_PAD,   0.70f);
DEFSND(s_lowbatt,  "Low Batt",   N_lowbatt,  TW_SINE,   TE_PLUCK, 1.00f);
DEFSND(s_unlock,   "Unlock",     N_unlock,   TW_SINE,   TE_PLUCK, 1.00f);

// ---------- Telephony : tonalités fidèles aux normes (dual-tone) ----------
// US Precise Tone Plan (Bellcore) + DTMF (ITU-T Q.23). Cadences réduites à quelques
// cycles pour un déclenchement « one-shot ».
static const DualNote N_dialtone[] = { {350,440,1500} };                                  // tonalité
static const DualNote N_busy[]     = { {480,620,500},{0,0,500},{480,620,500},{0,0,500},{480,620,500} };
static const DualNote N_ringback[] = { {440,480,1500},{0,0,900},{440,480,1500} };          // sonnerie
static const DualNote N_reorder[]  = { {480,620,250},{0,0,250},{480,620,250},{0,0,250},{480,620,250},{0,0,250},{480,620,250} };
static const DualNote N_sit[]      = { {950,0,330},{1400,0,330},{1800,0,330} };            // SIT (ITU 3 tons montants)
static const DualNote N_dtmf1[]    = { {697,1209,220} };                                   // touche 1
static const DualNote N_dtmf5[]    = { {770,1336,220} };                                   // touche 5
static const DualNote N_dtmf0[]    = { {941,1336,220} };                                   // touche 0
static const DualNote N_dtmfhash[] = { {941,1477,220} };                                   // touche #

DEFDUAL(d_dialtone, "Dial Tone", N_dialtone, 0.90f);
DEFDUAL(d_busy,     "Busy",      N_busy,     0.90f);
DEFDUAL(d_ringback, "Ringback",  N_ringback, 0.90f);
DEFDUAL(d_reorder,  "Reorder",   N_reorder,  0.90f);
DEFDUAL(d_sit,      "SIT",       N_sit,      0.95f);
DEFDUAL(d_dtmf1,    "DTMF 1",    N_dtmf1,    0.95f);
DEFDUAL(d_dtmf5,    "DTMF 5",    N_dtmf5,    0.95f);
DEFDUAL(d_dtmf0,    "DTMF 0",    N_dtmf0,    0.95f);
DEFDUAL(d_dtmfhash, "DTMF #",    N_dtmfhash, 0.95f);

// ---------- Classics : mélodies du domaine public (suites de notes) ----------
// Für Elise / Ode to Joy (Beethoven, †1827), Gran Vals (Tárrega, 1902, †1909),
// Westminster Quarters (1793). Compositions toutes dans le domaine public.
static const ToneNote N_furelise[]    = { {659.25,130},{622.25,130},{659.25,130},{622.25,130},{659.25,130},{493.88,130},{587.33,130},{523.25,130},{440.00,380} };
static const ToneNote N_odejoy[]      = { {329.63,260},{329.63,260},{349.23,260},{392.00,260},{392.00,260},{349.23,260},{329.63,260},{293.66,260},{261.63,260},{261.63,260},{293.66,260},{329.63,260},{329.63,360},{293.66,180},{293.66,420} };
static const ToneNote N_granvals[]    = { {659.25,140},{587.33,140},{739.99,300},{830.61,140},{1108.73,140},{987.77,300},{587.33,140},{659.25,300},{987.77,140},{880.00,140},{554.37,300},{659.25,140},{880.00,520} };
static const ToneNote N_westminster[] = { {415.30,450},{369.99,450},{329.63,450},{246.94,750} };

DEFSND(s_furelise,    "Fur Elise",   N_furelise,    TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_odejoy,      "Ode to Joy",  N_odejoy,      TW_SINE, TE_PAD,   0.95f);
DEFSND(s_granvals,    "Gran Vals",   N_granvals,    TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_westminster, "Westminster", N_westminster, TW_SINE, TE_PLUCK, 1.00f);

// ---------- Windows : sons ORIGINAUX évocateurs (pas les sons MS protégés) ----------
static const ToneNote N_winstartup[]  = { {392.00,260},{587.33,260},{783.99,620} };
static const ToneNote N_winshutdown[] = { {783.99,260},{587.33,260},{392.00,620} };
static const ToneNote N_winerror[]    = { {415.30,220},{0,40},{311.13,460} };
static const ToneNote N_winnotify[]   = { {659.25,90},{830.61,90},{1046.50,300} };
static const ToneNote N_winding[]     = { {880.00,120},{1174.66,420} };

DEFSND(s_winstartup,  "Win Startup",  N_winstartup,  TW_TRI,  TE_PAD,   0.85f);
DEFSND(s_winshutdown, "Win Shutdown", N_winshutdown, TW_TRI,  TE_PAD,   0.85f);
DEFSND(s_winerror,    "Win Error",    N_winerror,    TW_TRI,  TE_PAD,   0.85f);
DEFSND(s_winnotify,   "Win Notify",   N_winnotify,   TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_winding,     "Win Ding",     N_winding,     TW_SINE, TE_PLUCK, 1.00f);

// ---------- iOS : sons ORIGINAUX évocateurs (pas les sons Apple protégés) ----------
static const ToneNote N_iosnote[]    = { {1046.50,110},{1567.98,320} };
static const ToneNote N_iostritone[] = { {1396.91,120},{1046.50,120},{1567.98,320} };
static const ToneNote N_iosunlock[]  = { {987.77,80},{1318.51,200} };
static const ToneNote N_iossent[]    = { {600,45},{800,45},{1050,45},{1400,140} };
static const ToneNote N_ioslock[]    = { {659.25,70},{440.00,200} };

DEFSND(s_iosnote,    "iOS Note",    N_iosnote,    TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_iostritone, "iOS Tritone", N_iostritone, TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_iosunlock,  "iOS Unlock",  N_iosunlock,  TW_SINE, TE_PLUCK, 1.00f);
DEFSND(s_iossent,    "iOS Sent",    N_iossent,    TW_SINE, TE_PAD,   0.95f);
DEFSND(s_ioslock,    "iOS Lock",    N_ioslock,    TW_SINE, TE_PLUCK, 1.00f);

// ---------- Registre plat, groupé par catégorie ----------
static Sound* g_registry[] = {
    /* Rich      [0]  */ &s_harp, &s_ping,
    /* UI        [2]  */ &s_beep, &s_beep2, &s_ding, &s_dingdong, &s_notify, &s_message,
                         &s_pop, &s_tick, &s_chime, &s_bell, &s_confirm, &s_cancel,
    /* FX        [14] */ &s_tada, &s_error, &s_coin, &s_powerup, &s_powerdn, &s_sweepup,
                         &s_sweepdn, &s_alarm, &s_lowbatt, &s_unlock,
    /* Telephony [24] */ &d_dialtone, &d_busy, &d_ringback, &d_reorder, &d_sit,
                         &d_dtmf1, &d_dtmf5, &d_dtmf0, &d_dtmfhash,
    /* Classics  [33] */ &s_furelise, &s_odejoy, &s_granvals, &s_westminster,
    /* Windows   [37] */ &s_winstartup, &s_winshutdown, &s_winerror, &s_winnotify, &s_winding,
    /* iOS       [42] */ &s_iosnote, &s_iostritone, &s_iosunlock, &s_iossent, &s_ioslock,
};
static constexpr int kCount = (int)(sizeof(g_registry) / sizeof(g_registry[0]));

static const SoundCategory g_categories[] = {
    { "Rich",       0,  2 },
    { "UI",         2, 12 },
    { "FX",        14, 10 },
    { "Telephony", 24,  9 },
    { "Classics",  33,  4 },
    { "Windows",   37,  5 },
    { "iOS",       42,  5 },
};
static constexpr int kCatCount = (int)(sizeof(g_categories) / sizeof(g_categories[0]));

int sounds_count() { return kCount; }

Sound* sounds_get(int index) {
    if (index < 0 || index >= kCount) return nullptr;
    return g_registry[index];
}

const char* sounds_name(int index) {
    Sound* s = sounds_get(index);
    return s ? s->name() : "";
}

int categories_count() { return kCatCount; }

const SoundCategory* category_get(int index) {
    if (index < 0 || index >= kCatCount) return nullptr;
    return &g_categories[index];
}
