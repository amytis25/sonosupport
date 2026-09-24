/*
 * emg_cal.h
 *
 * Description: MyoWare 2.0 EMG calibration and trial logging (C port of
 *              calibration/myoware_calibrate.py). Records a resting
 *              baseline and MVC, then logs trials as %MVC. The Pico has
 *              no filesystem, so all data is streamed over USB serial as
 *              tagged CSV lines that calibration/emg_capture.py saves:
 *                CAL,baseline_v,noise_v,mvc_v,threshold_v
 *                LOG,label,t_s,volts,pct_mvc,active
 *                SUM,label,duration_s,mean_pct_mvc,peak_pct_mvc,
 *                    pct_time_active,baseline_v,mvc_v
 * Inputs:  EMG HAL (emg.h); 'q' on stdin stops a trial
 * Outputs: prompts and tagged CSV lines on stdout
 * Author:  Kanika
 * Created: 2026-09-23
 */

#ifndef EMG_CAL_H
#define EMG_CAL_H

#include <stdbool.h>

#define EMG_CAL_MVC_REPS 3u   /* number of maximal contractions */

typedef struct {
    float baseline;                 /* resting mean (V) */
    float noise;                    /* resting std dev (V) */
    float mvc;                      /* average of rep peaks (V) */
    float threshold;                /* ACTIVE when above this (V) */
    float peaks[EMG_CAL_MVC_REPS];  /* peak of each MVC rep (V) */
} emg_cal_t;

// Runs the interactive baseline + MVC calibration and prints a CAL line.
// Returns false if the result is unusable (MVC not above baseline).
bool emg_cal_run(emg_cal_t *cal);

// Logs a trial with the given label until 'q' is received on stdin,
// then prints a summary and a SUM line.
void emg_cal_trial(const emg_cal_t *cal, const char *label);

#endif // EMG_CAL_H
