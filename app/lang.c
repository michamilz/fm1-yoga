/* SPDX-License-Identifier: MPL-2.0 */
/* The screen's words in three languages: Deutsch (the default), English, and Sanskrit. The screen's fonts are Latin
 * only, and the panel draws a glyph at a time (no shaping): Sanskrit is transliterated, in capitals, without
 * diacritics. It names what is yoga's (the programs, their stages, the pages, the breath, silence); the technical
 * words (levels, reverb, saving) are English there. Part of the unity build (app.c). */

enum { LG_DE, LG_EN, LG_SA, NLANG };
static const char *const LANG_NAME[NLANG] = {"DEUTSCH", "ENGLISH", "SAMSKRTAM"};

enum {
    S_BEREIT, S_PAUSE, S_ANKOMMEN, S_VERTIEFEN, S_RUHEN, S_AUFWACHEN, S_NAMASTE,
    S_KLANG, S_GARTEN, S_SETUP,
    S_DRONE, S_ATEM, S_SCHALEN, S_RAUM, S_PADS, S_GLOCKEN, S_BASS, S_ECHO, S_ALLES,
    S_SPIEL, S_KLANG_SUB,
    S_PEGEL, S_ANTEIL, S_LEDATEM, S_DIMMEN, S_SPRACHE,
    S_AN, S_AUS, S_NIE, S_AUTO, S_ENDLOS, S_MIN, S_STUMM, S_TEILSTUMM,
    S_HINT_SUB, S_HINT_BACK, S_WELLE, S_TON,
    S_PROGRAMM, S_DAUER, S_KOPFHOERER, S_LAUTSPRECHER, S_START, S_WEITER, S_STOPP, S_GESPEICHERT,
    S_SPEICHERN, S_FEHLER, S_GRUNDTON, S_UEBERSICHT,
    S_SYSTEM, S_TAKT, S_TEMPO, S_ANZEIGE, S_DISPLAY, S_BEIDE,
    NSTR
};
static const char *const STR[NLANG][NSTR] = {
    [LG_DE] = {
        "BEREIT", "PAUSE", "ANKOMMEN", "VERTIEFEN", "RUHEN", "AUFWACHEN", "NAMASTE",
        "KLANG", "GARTEN", "SETUP",
        "DRONE", "ATEM", "SCHALEN", "RAUM", "PADS", "GLOCKEN", "BASS", "ECHO", "ALLES",
        "SPIEL", "KLANG",
        "PEGEL", "ANTEIL", "LED-ATEM", "DIMMEN", "SPRACHE",
        "AN", "AUS", "NIE", "AUTO", "ENDLOS", "MIN", "STUMM", "TEIL-STUMM",
        "SELECT BLAETTERT   SAVE SPEICHERT", "SAVE SPEICHERT   HOME ZURUECK", "WELLE", "TON",
        "PROGRAMM", "DAUER", "KOPFHOERER", "LAUTSPRECHER", "START", "WEITER", "STOPP", "GESPEICHERT",
        "SPEICHERN", "FEHLER", "GRUNDTON", "UEBERSICHT",
        "SYSTEM", "TAKT", "TEMPO", "ANZEIGE", "DISPLAY", "BEIDE",
    },
    [LG_EN] = {
        "READY", "PAUSE", "ARRIVE", "DEEPEN", "REST", "AWAKEN", "NAMASTE",
        "SOUND", "GARDEN", "SETUP",
        "DRONE", "BREATH", "BOWLS", "SPACE", "PADS", "BELLS", "BASS", "ECHO", "ALL",
        "PLAY", "TONE",
        "LEVEL", "AMOUNT", "LED BREATH", "DIM", "LANGUAGE",
        "ON", "OFF", "NEVER", "AUTO", "ENDLESS", "MIN", "MUTE", "PART MUTE",
        "SELECT: PAGE   SAVE: STORE", "SAVE: STORE   HOME: BACK", "WAVE", "TONE",
        "PROGRAM", "DURATION", "HEADPHONES", "SPEAKERS", "START", "RESUME", "STOP", "SAVED",
        "SAVE", "FAILED", "CARRIER", "OVERVIEW",
        "SYSTEM", "BEAT", "TEMPO", "SHOW ON", "SCREEN", "BOTH",
    },
    [LG_SA] = {
        "SAJJA", "VIRAMA", "AGAMANA", "DHYANA", "VISHRAMA", "PRABODHA", "NAMASTE",
        "NADA", "UDYANA", "SETUP",
        "SHRUTI", "PRANA", "PATRA", "AKASHA", "PADS", "GHANTA", "BASS", "PRATIDHVANI", "SARVA",
        "VADANA", "NADA",
        "LEVEL", "AMOUNT", "LED PRANA", "DIM", "BHASHA",
        "ON", "OFF", "NEVER", "AUTO", "ANANTA", "MIN", "MAUNA", "ARDHA MAUNA",
        "SELECT: PAGE   SAVE: STORE", "SAVE: STORE   HOME: BACK", "WAVE", "TONE",
        "PROGRAM", "DURATION", "HEADPHONES", "SPEAKERS", "ARAMBHA", "RESUME", "STOP", "SAVED",
        "SAVE", "FAILED", "CARRIER", "OVERVIEW",
        "SYSTEM", "TALA", "TEMPO", "SHOW ON", "SCREEN", "BOTH",
    },
};
static const char *const PROG_NAME[NLANG][YP_NPROG] = {
    {"ERDEN", "LOSLASSEN", "HERZ", "WANDLUNG", "REISE", "MANUELL"},
    {"EARTH", "RELEASE", "HEART", "TRANSFORM", "JOURNEY", "MANUAL"},
    {"PRITHVI", "VAIRAGYA", "ANAHATA", "PARINAMA", "YATRA", "SVATANTRA"},
};
static const char *const INST_NAME[NLANG][YI_N] = {
    {"HANDPAN", "KALIMBA", "ZUNGE", "SCHALEN", "KOSHI"},
    {"HANDPAN", "KALIMBA", "TONGUE", "BOWLS", "KOSHI"},
    {"HANDPAN", "KALIMBA", "TONGUE", "PATRA", "KOSHI"},
};
/* the details' labels (yoga_program.h YX_*); Sanskrit's are English */
static const char *const YX_LABEL[2][YX_N] = {
    {"OBERTOENE", "SCHWEBUNG", "PULS (ISO)", "TEMPO", "FARBE", "WELLE", "GLOECKCHEN", "TONLAGE", "NACHKLANG",
     "GROESSE", "ECHO", "FARBE", "HELLIGKEIT", "AKKORDE", "DICHTE", "KLANG", "OKTAVE", "OKTAVE", "GLEITEN",
     "QUINTE", "ZEIT", "HALL", "BEWEGUNG", "INSTRUMENT", "SKALA", "DING", "NACHKLANG", "HALL", "ECHO",
     "HELLIGKEIT", "DYNAMIK"},
    {"OVERTONES", "BEATING", "PULSE (ISO)", "RATE", "COLOUR", "SWELL", "CHIMES", "REGISTER", "RING",
     "SIZE", "ECHO", "COLOUR", "BRIGHTNESS", "CHORDS", "DENSITY", "TONE", "OCTAVE", "OCTAVE", "GLIDE",
     "FIFTH", "TIME", "REVERB", "MOTION", "INSTRUMENT", "SCALE", "DING", "RING", "REVERB", "ECHO",
     "BRIGHTNESS", "DYNAMICS"},
};
_Static_assert(sizeof YX_LABEL[0] / sizeof YX_LABEL[0][0] == YX_N, "a label for every detail");
static const char *const PITCH_NAME[2][3] = {{"TIEF", "MITTEL", "HOCH"}, {"LOW", "MIDDLE", "HIGH"}};
static const char *const ETIME_NAME[2][6] = {{"1/4.", "1/2", "1/2.", "1 TAKT", "2 TAKTE", "4 TAKTE"},
                                             {"1/4.", "1/2", "1/2.", "1 BAR", "2 BARS", "4 BARS"}};

static int lang(void) { return set.lang < NLANG ? set.lang : LG_DE; }
static int lang2(void) { return lang() == LG_DE ? 0 : 1; }         /* German, or English (Sanskrit's technical) */
static const char *T(int id) { return STR[lang()][id]; }
static const char *prog_name(int i) { return PROG_NAME[lang()][(unsigned)i % YP_NPROG]; }
static const char *inst_name(int i) { return INST_NAME[lang()][(unsigned)i % YI_N]; }
static const char *yx_label(int i) { return YX_LABEL[lang2()][(unsigned)i % YX_N]; }
/* a detail's value in words, where it is a word that changes with the language (else 0: yx_text) */
static const char *yx_word(int i, int v)
{
    if (v == YX_AUTO)
        return T(S_AUTO);
    if (i == YX_HINST)
        return inst_name(v);
    if (i == YX_BPITCH)
        return PITCH_NAME[lang2()][clamp(v + 1, 0, 2)];
    if (i == YX_ETIME)
        return ETIME_NAME[lang2()][clamp(v, 0, 5)];
    return 0;
}
/* a stage (yoga_program.h YST_*) */
static const char *stage_name(int st)
{
    switch (st) {
    case YST_ANKOMMEN: return T(S_ANKOMMEN);
    case YST_VERTIEFEN: return T(S_VERTIEFEN);
    case YST_RUHEN: return T(S_RUHEN);
    case YST_AUFWACHEN: return T(S_AUFWACHEN);
    case YST_NAMASTE: return T(S_NAMASTE);
    case YST_C0: case YST_C1: case YST_C2: case YST_C3: return prog_name(st - YST_C0);   /* REISE: the carrier's */
    default: return "";
    }
}
