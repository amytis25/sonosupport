"""
MyoWare 2.0 EMG calibration for Raspberry Pi Pico 2 W (MicroPython)

Wiring:
    MyoWare +    -> 3V3(OUT)  (pin 36)
    MyoWare -    -> GND
    MyoWare ENV  -> GP26 / ADC0 (pin 31)

Run from Thonny (or save as main.py). Follow the prompts in the console.
Results are saved to calibration.json on the Pico.
"""

from machine import ADC, Pin
import time
import json
import os

# ---------------- Settings ----------------
ADC_PIN = 26          # GP26 = ADC0
VREF = 3.3            # ADC reference voltage
FS = 100              # samples per second
ALPHA = 0.2           # smoothing factor (0-1, lower = smoother)
N_AVG = 8             # ADC reads averaged per sample
REST_SECONDS = 5      # baseline recording length
MVC_REPS = 3          # number of maximal contractions
MVC_SECONDS = 3       # length of each contraction
MVC_REST_SECONDS = 5  # rest between contractions
THRESH_K = 3          # active threshold = baseline + K * noise std
CAL_FILE = "calibration.json"
LOG_FILE = "emg_log.csv"          # every sample, all trials (appended each run)
SUMMARY_FILE = "emg_summary.csv"  # one row per trial
LOG_HZ = 10                       # logged samples per second

adc = ADC(Pin(ADC_PIN))


# ---------------- Helpers ----------------
def read_v():
    """Read the ENV pin in volts, averaging several ADC reads."""
    total = 0
    for _ in range(N_AVG):
        total += adc.read_u16()
    return (total / N_AVG) * VREF / 65535


def record(seconds):
    """Record smoothed voltage for `seconds`; return list of samples."""
    samples = []
    smooth = read_v()
    for _ in range(int(seconds * FS)):
        smooth = ALPHA * read_v() + (1 - ALPHA) * smooth
        samples.append(smooth)
        time.sleep(1 / FS)
    return samples


def mean(x):
    return sum(x) / len(x)


def std(x):
    m = mean(x)
    return (sum((v - m) ** 2 for v in x) / len(x)) ** 0.5


def countdown(msg, n=3):
    print(msg)
    for i in range(n, 0, -1):
        print("  %d ..." % i)
        time.sleep(1)


# ---------------- Calibration ----------------
def calibrate():
    print("=== MyoWare 2.0 calibration ===")
    print("Make sure electrodes are placed and stay in the same spot.\n")

    # Step 1: resting baseline
    countdown("Step 1: RELAX the muscle completely. Starting in", 3)
    print("Recording baseline...")
    rest = record(REST_SECONDS)
    baseline = mean(rest)
    noise = std(rest)
    print("Baseline: %.3f V   (noise std: %.4f V)\n" % (baseline, noise))

    # Step 2: MVC
    peaks = []
    for rep in range(1, MVC_REPS + 1):
        countdown("Step 2: rep %d/%d - get ready to squeeze MAX in" % (rep, MVC_REPS), 3)
        print("SQUEEZE!")
        burst = record(MVC_SECONDS)
        peaks.append(max(burst))
        print("Peak: %.3f V" % peaks[-1])
        if rep < MVC_REPS:
            print("Rest...")
            time.sleep(MVC_REST_SECONDS)
    mvc = mean(peaks)
    print("\nMVC (average of peaks): %.3f V" % mvc)

    # Step 3: sanity checks
    ok = True
    if max(peaks) > 3.2:
        print("WARNING: signal is saturating. Turn the gain pot DOWN and redo.")
        ok = False
    if (mvc - baseline) < 0.5:
        print("WARNING: weak signal range. Turn gain UP or check electrode placement.")
        ok = False
    spread = (max(peaks) - min(peaks)) / mvc * 100
    print("Peak-to-peak variation across reps: %.1f %%" % spread)
    if spread > 20:
        print("WARNING: reps vary a lot. Try to give a consistent maximal effort.")

    # Step 4: compute and save
    threshold = baseline + THRESH_K * noise
    cal = {
        "baseline": baseline,
        "noise": noise,
        "mvc": mvc,
        "threshold": threshold,
        "peaks": peaks,
    }
    with open(CAL_FILE, "w") as f:
        json.dump(cal, f)
    print("\nSaved to %s:" % CAL_FILE)
    print(cal)
    if not ok:
        print("\nFix the warnings above and re-run for a reliable calibration.")
    return cal


def load_calibration():
    with open(CAL_FILE) as f:
        return json.load(f)


# ---------------- Live readout ----------------
def live(cal, label):
    baseline, mvc, threshold = cal["baseline"], cal["mvc"], cal["threshold"]

    new_file = LOG_FILE not in os.listdir()
    f = open(LOG_FILE, "a")
    if new_file:
        f.write("label,t_s,volts,pct_mvc,active\n")

    print("\nLogging to %s (Ctrl+C to stop)" % LOG_FILE)
    smooth = read_v()
    start = time.ticks_ms()
    last_flush = start
    n = 0
    active_n = 0
    pct_sum = 0
    pct_peak = 0
    t = 0

    try:
        while True:
            smooth = ALPHA * read_v() + (1 - ALPHA) * smooth
            pct = max(0, (smooth - baseline) / (mvc - baseline) * 100)
            active = smooth > threshold
            now = time.ticks_ms()
            t = time.ticks_diff(now, start) / 1000

            f.write("%s,%.2f,%.4f,%.2f,%d\n" % (label, t, smooth, pct, 1 if active else 0))
            n += 1
            active_n += 1 if active else 0
            pct_sum += pct
            pct_peak = max(pct_peak, pct)

            print("%.3f V   %5.1f %%MVC   %s" % (smooth, pct, "ACTIVE" if active else "rest"))

            if time.ticks_diff(now, last_flush) > 1000:   # flush once a second
                f.flush()
                last_flush = now
            time.sleep(1 / LOG_HZ)
    except KeyboardInterrupt:
        pass
    finally:
        f.close()

    if n == 0:
        print("No data recorded.")
        return

    mean_pct = pct_sum / n
    active_pct = active_n / n * 100
    print("\nTrial '%s': %.1f s | mean %.1f %%MVC | peak %.1f %%MVC | active %.1f %% of time"
          % (label, t, mean_pct, pct_peak, active_pct))

    new_summary = SUMMARY_FILE not in os.listdir()
    with open(SUMMARY_FILE, "a") as s:
        if new_summary:
            s.write("label,duration_s,mean_pct_mvc,peak_pct_mvc,pct_time_active,baseline_v,mvc_v\n")
        s.write("%s,%.1f,%.2f,%.2f,%.1f,%.4f,%.4f\n"
                % (label, t, mean_pct, pct_peak, active_pct, baseline, mvc))
    print("Saved to %s and %s" % (LOG_FILE, SUMMARY_FILE))


# ---------------- Main ----------------
try:
    calibration = calibrate()
    trial_label = input("Trial label (e.g. with_support / without_support): ").strip() or "trial"
    trial_label = trial_label.replace(" ", "_").replace(",", "")
    live(calibration, trial_label)
except KeyboardInterrupt:
    print("\nStopped.")