/**
 * Equalizer Implementation - 5-band parametric EQ
 *
 * Band 0: 60Hz  low-shelf    (Sub Bass)
 * Band 1: 250Hz peaking Q=1  (Bass)
 * Band 2: 1kHz  peaking Q=1  (Mid)
 * Band 3: 4kHz  peaking Q=1  (Treble)
 * Band 4: 16kHz high-shelf   (Air)
 *
 * Biquad IIR filters per Audio EQ Cookbook (Robert Bristow-Johnson).
 * Sample rate assumed 44100 Hz (SDL_mixer default).
 */

#include "equalizer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define EQ_STEP_DB  2
#define SAMPLE_RATE 44100.0

// Filter type per band
typedef enum {
    FILTER_LOWSHELF,
    FILTER_PEAKING,
    FILTER_HIGHSHELF
} FilterType;

// Band definition
typedef struct {
    double freq;
    FilterType type;
    const char *label;
} BandDef;

static const BandDef BANDS[EQ_BAND_COUNT] = {
    {   60.0, FILTER_LOWSHELF,  "60Hz"  },
    {  250.0, FILTER_PEAKING,   "250Hz" },
    { 1000.0, FILTER_PEAKING,   "1kHz"  },
    { 4000.0, FILTER_PEAKING,   "4kHz"  },
    {16000.0, FILTER_HIGHSHELF, "16kHz" },
};

// Biquad filter state (stereo: 2 channels)
typedef struct {
    double b0, b1, b2;  // Numerator coefficients
    double a1, a2;      // Denominator coefficients (a0 normalized to 1)
    double x1[2], x2[2]; // Input history (per channel)
    double y1[2], y2[2]; // Output history (per channel)
} BiquadFilter;

// EQ state
static int g_band_db[EQ_BAND_COUNT];
static BiquadFilter g_filters[EQ_BAND_COUNT];
static char g_band_str[16];

/**
 * Reset filter history (prevents clicks when changing settings)
 */
static void reset_filter_history(BiquadFilter *f) {
    f->x1[0] = f->x1[1] = 0.0;
    f->x2[0] = f->x2[1] = 0.0;
    f->y1[0] = f->y1[1] = 0.0;
    f->y2[0] = f->y2[1] = 0.0;
}

/**
 * Set filter to flat passthrough
 */
static void set_passthrough(BiquadFilter *f) {
    f->b0 = 1.0; f->b1 = 0.0; f->b2 = 0.0;
    f->a1 = 0.0; f->a2 = 0.0;
    reset_filter_history(f);
}

/**
 * Compute low-shelf biquad coefficients
 */
static void compute_lowshelf(BiquadFilter *f, double freq, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * freq / SAMPLE_RATE;
    double cosw0 = cos(w0);
    double sinw0 = sin(w0);
    double alpha = sinw0 / 2.0 * sqrt((A + 1.0/A) * (1.0/0.9 - 1.0) + 2.0);

    double a0 = (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrt(A) * alpha;

    f->b0 = (A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrt(A) * alpha)) / a0;
    f->b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
    f->b2 = (A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrt(A) * alpha)) / a0;
    f->a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
    f->a2 = ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrt(A) * alpha) / a0;
}

/**
 * Compute high-shelf biquad coefficients
 */
static void compute_highshelf(BiquadFilter *f, double freq, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * freq / SAMPLE_RATE;
    double cosw0 = cos(w0);
    double sinw0 = sin(w0);
    double alpha = sinw0 / 2.0 * sqrt((A + 1.0/A) * (1.0/0.9 - 1.0) + 2.0);

    double a0 = (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrt(A) * alpha;

    f->b0 = (A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrt(A) * alpha)) / a0;
    f->b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
    f->b2 = (A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrt(A) * alpha)) / a0;
    f->a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
    f->a2 = ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrt(A) * alpha) / a0;
}

/**
 * Compute peaking EQ biquad coefficients (Q = 1.0)
 */
static void compute_peaking(BiquadFilter *f, double freq, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * freq / SAMPLE_RATE;
    double cosw0 = cos(w0);
    double sinw0 = sin(w0);
    double alpha = sinw0 / 2.0;  // Q = 1.0 → alpha = sin(w0)/2

    double a0 = 1.0 + alpha / A;

    f->b0 = (1.0 + alpha * A) / a0;
    f->b1 = (-2.0 * cosw0) / a0;
    f->b2 = (1.0 - alpha * A) / a0;
    f->a1 = (-2.0 * cosw0) / a0;
    f->a2 = (1.0 - alpha / A) / a0;
}

/**
 * Update filter coefficients for a specific band
 */
static void update_band_filter(int band) {
    if (band < 0 || band >= EQ_BAND_COUNT) return;

    if (g_band_db[band] == 0) {
        set_passthrough(&g_filters[band]);
        return;
    }

    double freq = BANDS[band].freq;
    double db = (double)g_band_db[band];

    switch (BANDS[band].type) {
        case FILTER_LOWSHELF:
            compute_lowshelf(&g_filters[band], freq, db);
            break;
        case FILTER_PEAKING:
            compute_peaking(&g_filters[band], freq, db);
            break;
        case FILTER_HIGHSHELF:
            compute_highshelf(&g_filters[band], freq, db);
            break;
    }
}

/**
 * Soft clipping to prevent harsh distortion
 */
static inline double soft_clip(double sample) {
    const double threshold = 31000.0;
    if (sample > threshold) {
        return threshold + (32767.0 - threshold) * tanh((sample - threshold) / (32767.0 - threshold));
    } else if (sample < -threshold) {
        return -threshold + (-32767.0 + threshold) * tanh((sample + threshold) / (32767.0 - threshold));
    }
    return sample;
}

/**
 * Apply biquad filter to a sample
 */
static inline double apply_biquad(BiquadFilter *f, double input, int channel) {
    double output = f->b0 * input + f->b1 * f->x1[channel] + f->b2 * f->x2[channel]
                  - f->a1 * f->y1[channel] - f->a2 * f->y2[channel];

    f->x2[channel] = f->x1[channel];
    f->x1[channel] = input;
    f->y2[channel] = f->y1[channel];
    f->y1[channel] = output;

    return output;
}

/**
 * Check if any band is non-flat
 */
static int eq_is_active(void) {
    for (int i = 0; i < EQ_BAND_COUNT; i++) {
        if (g_band_db[i] != 0) return 1;
    }
    return 0;
}

/**
 * Post-mix callback - processes all audio through EQ chain
 */
static void eq_postmix_callback(void *udata, Uint8 *stream, int len) {
    (void)udata;

    if (!eq_is_active()) return;

    int16_t *samples = (int16_t *)stream;
    int sample_count = len / sizeof(int16_t);

    for (int i = 0; i < sample_count; i += 2) {
        double left = (double)samples[i];
        double right = (double)samples[i + 1];

        // Chain all active filters
        for (int b = 0; b < EQ_BAND_COUNT; b++) {
            if (g_band_db[b] != 0) {
                left = apply_biquad(&g_filters[b], left, 0);
                right = apply_biquad(&g_filters[b], right, 1);
            }
        }

        samples[i] = (int16_t)soft_clip(left);
        samples[i + 1] = (int16_t)soft_clip(right);
    }
}

void eq_init(void) {
    for (int i = 0; i < EQ_BAND_COUNT; i++) {
        g_band_db[i] = 0;
        set_passthrough(&g_filters[i]);
    }

    Mix_SetPostMix(eq_postmix_callback, NULL);
    printf("[EQ] Initialized 5-band EQ (all flat)\n");
}

void eq_cleanup(void) {
    Mix_SetPostMix(NULL, NULL);
    printf("[EQ] Cleanup complete\n");
}

int eq_get_band_count(void) {
    return EQ_BAND_COUNT;
}

int eq_get_band_db(int band) {
    if (band < 0 || band >= EQ_BAND_COUNT) return 0;
    return g_band_db[band];
}

void eq_set_band_db(int band, int db) {
    if (band < 0 || band >= EQ_BAND_COUNT) return;
    if (db < EQ_MIN_DB) db = EQ_MIN_DB;
    if (db > EQ_MAX_DB) db = EQ_MAX_DB;

    if (db != g_band_db[band]) {
        g_band_db[band] = db;
        reset_filter_history(&g_filters[band]);
        update_band_filter(band);
        printf("[EQ] %s: %d dB\n", BANDS[band].label, db);
    }
}

void eq_adjust_band(int band, int dir) {
    if (band < 0 || band >= EQ_BAND_COUNT) return;
    int new_db = g_band_db[band] + (dir > 0 ? EQ_STEP_DB : -EQ_STEP_DB);
    eq_set_band_db(band, new_db);
}

const char* eq_get_band_label(int band) {
    if (band < 0 || band >= EQ_BAND_COUNT) return "";
    return BANDS[band].label;
}

const char* eq_get_band_string(int band) {
    if (band < 0 || band >= EQ_BAND_COUNT) return "";
    int db = g_band_db[band];
    if (db == 0) {
        snprintf(g_band_str, sizeof(g_band_str), "0 dB");
    } else if (db > 0) {
        snprintf(g_band_str, sizeof(g_band_str), "+%d dB", db);
    } else {
        snprintf(g_band_str, sizeof(g_band_str), "%d dB", db);
    }
    return g_band_str;
}

void eq_reset(void) {
    for (int i = 0; i < EQ_BAND_COUNT; i++) {
        g_band_db[i] = 0;
        set_passthrough(&g_filters[i]);
    }
    printf("[EQ] Reset to flat\n");
}

/* Legacy compatibility wrappers */
int eq_get_bass(void)   { return eq_get_band_db(0); }
int eq_get_treble(void) { return eq_get_band_db(4); }
void eq_set_bass(int db)   { eq_set_band_db(0, db); }
void eq_set_treble(int db) { eq_set_band_db(4, db); }
void eq_adjust_bass(int dir)   { eq_adjust_band(0, dir); }
void eq_adjust_treble(int dir) { eq_adjust_band(4, dir); }
const char* eq_get_bass_string(void)   { return eq_get_band_string(0); }
const char* eq_get_treble_string(void) { return eq_get_band_string(4); }
