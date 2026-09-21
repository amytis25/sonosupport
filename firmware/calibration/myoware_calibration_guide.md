# MyoWare 2.0 EMG Calibration Guide (Pico 2 W)

Used to validate the sonographer arm support by measuring muscle load with and without the device.

## What "calibration" means here

The MyoWare 2.0 doesn't need factory-style calibration. What you calibrate is the signal **for each person and each electrode placement**:

1. **Resting baseline**: what the signal looks like when the muscle is relaxed.
2. **MVC (maximum voluntary contraction)**: the signal at the strongest effort that person can produce.

Every reading is then expressed as **%MVC**, so results are comparable across people and sessions:

```
%MVC = (V - baseline) / (MVC - baseline) x 100
```

## Wiring

| MyoWare 2.0 | Pico 2 W |
|---|---|
| `+` | 3V3(OUT) (pin 36) |
| `-` | GND |
| `ENV` | GP26 / ADC0 (pin 31) |

- Use **ENV** (smoothed envelope), not RAW. It is already rectified and filtered.
- Power at 3.3V so ENV swings 0 to 3.3V, matching the Pico ADC range.

**Safety:** while electrodes are on a person, run your laptop on battery power (or use a USB isolator) so there is no mains-connected path through the body.

## One-time setup: flash MicroPython

You don't drag your `.py` onto the Pico. You drag over the **MicroPython firmware**, then send the script separately.

1. Download the Pico 2 W `.uf2` from micropython.org/download/RPI_PICO2_W.
2. Hold **BOOTSEL** while plugging in the USB cable. A drive called `RP2350` appears.
3. Drag the `.uf2` onto that drive. The Pico reboots by itself.

This replaces any C++ `.uf2` on the board, so re-flash that later for your other code.

Install the terminal tool on your laptop (Windows PowerShell, not WSL, since USB serial doesn't show up in WSL by default):

```powershell
pip install mpremote
```

## Step-by-step

### 1. Prepare the skin and place electrodes
- Clean the skin (and shave if needed).
- Place the sensor on the muscle belly, parallel to the muscle fibers.
- Put the reference electrode on a bony or neutral spot.
- **Mark the placement.** Moving it later invalidates the calibration.

### 2. Set the gain
- The MyoWare 2.0 has a gain potentiometer.
- Do a hard squeeze and adjust so the peak reaches roughly **2.0 to 3.0 V** without hitting the 3.3V rail.
- If it hits the rail (saturates), you lose information. The script warns you about this.

### 3. Run the script
Open PowerShell in the folder that contains `myoware_calibrate.py`:

```powershell
mpremote run myoware_calibrate.py
```

- The prompts print in the terminal. Follow them.
- If it says "no device", run `mpremote connect list` to find the port, then `mpremote connect COM5 run myoware_calibrate.py` (use your port).
- Prefer no terminal? Open the file in Thonny, choose **MicroPython (Raspberry Pi Pico)** as the interpreter, and press Run.
- `mpremote run` doesn't copy the script to the Pico, so it only runs while your laptop is connected. To run it on every power-up, use `mpremote cp myoware_calibrate.py :main.py`. Only do that once you're happy with it, because it starts calibrating right away on boot.

### 4. Baseline (5 seconds)
- Relax the muscle completely and stay still.
- The script records the **mean** (baseline) and **standard deviation** (noise level).

### 5. MVC (3 reps)
- Three maximal contractions of about 3 seconds each, with 5 seconds of rest between.
- The script takes the **peak** of each rep and averages them.
- Averaging gives a more repeatable MVC than a single squeeze.

### 6. Sanity checks (automatic)
- **Saturating** (peak above 3.2 V): turn the gain down and redo.
- **Weak range** (MVC minus baseline under 0.5 V): turn the gain up or check placement.
- **Rep variation** above about 20%: try for more consistent effort.

### 7. Threshold and save
- Active threshold = `baseline + 3 x noise std`.
- Values are saved to `calibration.json` on the Pico.

### 8. Log a trial
- The script asks for a **trial label**, for example `with_support` or `without_support`.
- It then logs 10 samples per second and prints live voltage, %MVC, and ACTIVE/rest state.
- Press **Ctrl+C** to stop. It prints a summary: duration, mean %MVC, peak %MVC, and % of time active.
- Data is written to the Pico as CSV files (flushed every second).

### 9. Copy the data to your laptop
```powershell
mpremote cp :emg_log.csv .
mpremote cp :emg_summary.csv .
```

- `emg_log.csv` has every sample (`label, t_s, volts, pct_mvc, active`). It appends on each run, so all your trials end up in one file.
- `emg_summary.csv` has one row per trial (mean %MVC, peak %MVC, % time active, plus the baseline and MVC used).
- Both open directly in Excel.
- To start fresh, delete the old files on the Pico: `mpremote rm :emg_log.csv :emg_summary.csv`.

## Tips for validation

- Calibrate **per session and per person**. Sweat, skin temperature, and electrode aging shift values.
- Redo the MVC a few times to check repeatability (around 10% variation is reasonable).
- Compare %MVC **with vs without** the arm support during the same simulated scanning task (use labels like `with_support` and `without_support`, then compare the rows in `emg_summary.csv`).
- Keep the same electrode placement between trials. If you re-calibrate, do it before both trials you want to compare.

## Settings you can tweak (top of the script)

| Setting | Default | Meaning |
|---|---|---|
| `FS` | 100 | Samples per second |
| `ALPHA` | 0.2 | Smoothing (lower = smoother) |
| `REST_SECONDS` | 5 | Baseline length |
| `MVC_REPS` | 3 | Number of maximal contractions |
| `MVC_SECONDS` | 3 | Length of each contraction |
| `MVC_REST_SECONDS` | 5 | Rest between contractions |
| `THRESH_K` | 3 | Threshold multiplier on noise std |
| `LOG_HZ` | 10 | Logged samples per second |
| `LOG_FILE` | `emg_log.csv` | File for every sample |
| `SUMMARY_FILE` | `emg_summary.csv` | File for one row per trial |