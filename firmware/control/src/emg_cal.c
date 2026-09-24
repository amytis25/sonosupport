/*
 * emg_cal.c
 *
 * Description: MyoWare 2.0 EMG calibration and trial logging.
 *              See emg_cal.h for the contract and serial line formats.
 *              Smoothing runs at FS_HZ in both calibration and trials so
 *              %MVC is measured with the same filter it was calibrated
 *              with; trials are then logged at LOG_HZ.
 * Inputs:  EMG HAL (emg.h); 'q' on stdin stops a trial
 * Outputs: prompts and tagged CSV lines on stdout
 * Author:  Kanika
 * Created: 2026-09-23
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "emg.h"
#include "emg_cal.h"

/* ---- Settings (same meaning as the old MicroPython script) ---------- */
#define FS_HZ              100u   /* samples per second */
#define ALPHA              0.2f   /* smoothing factor (0-1, lower = smoother) */
#define N_AVG              8u     /* ADC reads averaged per sample */
#define REST_SECONDS       5u     /* baseline recording length */
#define MVC_SECONDS        3u     /* length of each contraction */
#define MVC_REST_SECONDS   5u     /* rest between contractions */
#define THRESH_K           3.0f   /* threshold = baseline + K * noise std */
#define LOG_HZ             10u    /* logged samples per second in a trial */

#define SATURATION_V       3.2f   /* peak above this = gain too high */
#define MIN_RANGE_V        0.5f   /* MVC - baseline below this = gain too low */
#define MAX_SPREAD_PCT     20.0f  /* rep-to-rep variation warning */

#define SAMPLE_PERIOD_MS   (1000u / FS_HZ)
#define LOG_EVERY          (FS_HZ / LOG_HZ)

/* Running mean / std / max, so recordings need no sample buffer. */
typedef struct {
    uint32_t n;
    float mean;
    float m2;
    float max;
} stats_t;

static void stats_add(stats_t *s, float x) {
    s->n++;
    float d = x - s->mean;
    s->mean += d / s->n;
    s->m2 += d * (x - s->mean);
    if (s->n == 1 || x > s->max) {
        s->max = x;
    }
}

static float stats_std(const stats_t *s) {
    return s->n ? sqrtf(s->m2 / s->n) : 0.0f;
}

/* ENV pin in volts, averaged over several ADC reads. */
static float read_avg_v(void) {
    float total = 0.0f;
    for (uint32_t i = 0; i < N_AVG; i++) {
        total += emg_read_voltage();
    }
    return total / N_AVG;
}

/* Records smoothed voltage for `seconds` into running stats. */
static void record(uint32_t seconds, stats_t *s) {
    *s = (stats_t){0};
    float smooth = read_avg_v();
    absolute_time_t next = get_absolute_time();
    for (uint32_t i = 0; i < seconds * FS_HZ; i++) {
        smooth = ALPHA * read_avg_v() + (1.0f - ALPHA) * smooth;
        stats_add(s, smooth);
        next = delayed_by_ms(next, SAMPLE_PERIOD_MS);
        sleep_until(next);
    }
}

static void countdown(const char *msg, int n) {
    printf("%s\n", msg);
    for (int i = n; i > 0; i--) {
        printf("  %d ...\n", i);
        sleep_ms(1000);
    }
}

bool emg_cal_run(emg_cal_t *cal) {
    stats_t s;

    printf("=== MyoWare 2.0 calibration ===\n");
    printf("Make sure electrodes are placed and stay in the same spot.\n\n");

    /* Step 1: resting baseline */
    countdown("Step 1: RELAX the muscle completely. Starting in", 3);
    printf("Recording baseline...\n");
    record(REST_SECONDS, &s);
    cal->baseline = s.mean;
    cal->noise = stats_std(&s);
    printf("Baseline: %.3f V   (noise std: %.4f V)\n\n", cal->baseline, cal->noise);

    /* Step 2: MVC */
    float peak_sum = 0.0f;
    float peak_min = 0.0f;
    float peak_max = 0.0f;
    for (uint32_t rep = 0; rep < EMG_CAL_MVC_REPS; rep++) {
        char msg[64];
        snprintf(msg, sizeof msg, "Step 2: rep %lu/%u - get ready to squeeze MAX in",
                 (unsigned long)(rep + 1), EMG_CAL_MVC_REPS);
        countdown(msg, 3);
        printf("SQUEEZE!\n");
        record(MVC_SECONDS, &s);
        cal->peaks[rep] = s.max;
        printf("Peak: %.3f V\n", s.max);

        peak_sum += s.max;
        if (rep == 0 || s.max < peak_min) peak_min = s.max;
        if (rep == 0 || s.max > peak_max) peak_max = s.max;

        if (rep + 1 < EMG_CAL_MVC_REPS) {
            printf("Rest...\n");
            sleep_ms(MVC_REST_SECONDS * 1000u);
        }
    }
    cal->mvc = peak_sum / EMG_CAL_MVC_REPS;
    printf("\nMVC (average of peaks): %.3f V\n", cal->mvc);

    /* Step 3: sanity checks */
    bool ok = true;
    if (peak_max > SATURATION_V) {
        printf("WARNING: signal is saturating. Turn the gain pot DOWN and redo.\n");
        ok = false;
    }
    if ((cal->mvc - cal->baseline) < MIN_RANGE_V) {
        printf("WARNING: weak signal range. Turn gain UP or check electrode placement.\n");
        ok = false;
    }
    float spread = (cal->mvc > 0.0f) ? (peak_max - peak_min) / cal->mvc * 100.0f : 0.0f;
    printf("Peak-to-peak variation across reps: %.1f %%\n", spread);
    if (spread > MAX_SPREAD_PCT) {
        printf("WARNING: reps vary a lot. Try to give a consistent maximal effort.\n");
    }

    /* Step 4: threshold and report */
    cal->threshold = cal->baseline + THRESH_K * cal->noise;
    printf("\nThreshold: %.3f V\n", cal->threshold);
    printf("CAL,%.4f,%.5f,%.4f,%.4f\n", cal->baseline, cal->noise, cal->mvc, cal->threshold);
    if (!ok) {
        printf("\nFix the warnings above and recalibrate for a reliable result.\n");
    }

    if (cal->mvc <= cal->baseline) {
        printf("ERROR: MVC is not above baseline, %%MVC cannot be computed. Recalibrating.\n");
        return false;
    }
    return true;
}

void emg_cal_trial(const emg_cal_t *cal, const char *label) {
    const float range = cal->mvc - cal->baseline;

    printf("\nLogging trial '%s' (type q + Enter to stop)\n", label);

    uint32_t n = 0;
    uint32_t active_n = 0;
    float pct_sum = 0.0f;
    float pct_peak = 0.0f;
    float t = 0.0f;

    float smooth = read_avg_v();
    absolute_time_t start = get_absolute_time();
    absolute_time_t next = start;

    for (uint32_t i = 0; ; i++) {
        int c = getchar_timeout_us(0);
        if (c == 'q' || c == 'Q') {
            break;
        }

        smooth = ALPHA * read_avg_v() + (1.0f - ALPHA) * smooth;

        if (i % LOG_EVERY == 0) {
            float pct = (smooth - cal->baseline) / range * 100.0f;
            if (pct < 0.0f) pct = 0.0f;
            bool active = smooth > cal->threshold;
            t = absolute_time_diff_us(start, get_absolute_time()) / 1e6f;

            printf("LOG,%s,%.2f,%.4f,%.2f,%d\n", label, t, smooth, pct, active ? 1 : 0);
            n++;
            active_n += active ? 1u : 0u;
            pct_sum += pct;
            if (pct > pct_peak) pct_peak = pct;
        }

        next = delayed_by_ms(next, SAMPLE_PERIOD_MS);
        sleep_until(next);
    }

    /* Drop the rest of the "q\n" so it is not read as the next label. */
    while (getchar_timeout_us(20000) != PICO_ERROR_TIMEOUT) {
    }

    if (n == 0) {
        printf("No data recorded.\n");
        return;
    }

    float mean_pct = pct_sum / n;
    float active_pct = (float)active_n / n * 100.0f;
    printf("\nTrial '%s': %.1f s | mean %.1f %%MVC | peak %.1f %%MVC | active %.1f %% of time\n",
           label, t, mean_pct, pct_peak, active_pct);
    printf("SUM,%s,%.1f,%.2f,%.2f,%.1f,%.4f,%.4f\n",
           label, t, mean_pct, pct_peak, active_pct, cal->baseline, cal->mvc);
}
