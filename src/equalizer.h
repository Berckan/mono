/**
 * Equalizer - 5-band parametric EQ via biquad IIR filters
 *
 * Bands:
 *   0: 60Hz  (low-shelf)  - Sub Bass
 *   1: 250Hz (peaking)    - Bass
 *   2: 1kHz  (peaking)    - Mid
 *   3: 4kHz  (peaking)    - Treble
 *   4: 16kHz (high-shelf) - Air
 *
 * Range: -12 to +12 dB per band, 2 dB steps.
 * Processing via Mix_SetPostMix() callback for real-time audio.
 */

#ifndef EQUALIZER_H
#define EQUALIZER_H

#define EQ_BAND_COUNT 5
#define EQ_MIN_DB    -12
#define EQ_MAX_DB     12

/**
 * Initialize equalizer (call after Mix_OpenAudio)
 */
void eq_init(void);

/**
 * Cleanup equalizer resources
 */
void eq_cleanup(void);

/**
 * Get number of bands
 */
int eq_get_band_count(void);

/**
 * Get dB level for a band
 * @param band Band index (0 to EQ_BAND_COUNT-1)
 * @return dB value (-12 to +12)
 */
int eq_get_band_db(int band);

/**
 * Set dB level for a band
 * @param band Band index
 * @param db Level in dB (-12 to +12), clamped
 */
void eq_set_band_db(int band, int db);

/**
 * Adjust a band by one step (±2 dB)
 * @param band Band index
 * @param dir +1 to increase, -1 to decrease
 */
void eq_adjust_band(int band, int dir);

/**
 * Get frequency label for a band (e.g. "60Hz", "1kHz")
 * @param band Band index
 * @return Static string
 */
const char* eq_get_band_label(int band);

/**
 * Get dB as display string for a band
 * @param band Band index
 * @return Static string like "+6 dB" or "0 dB"
 */
const char* eq_get_band_string(int band);

/**
 * Reset all bands to 0 dB (flat)
 */
void eq_reset(void);

/* Legacy compatibility - map to band 0 (bass) and band 4 (treble) */
int eq_get_bass(void);
int eq_get_treble(void);
void eq_set_bass(int db);
void eq_set_treble(int db);
void eq_adjust_bass(int dir);
void eq_adjust_treble(int dir);
const char* eq_get_bass_string(void);
const char* eq_get_treble_string(void);

#endif // EQUALIZER_H
