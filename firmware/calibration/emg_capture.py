"""
Laptop-side companion to the C EMG calibration firmware (control/src/emg_cal.c).

The Pico has no filesystem, so it streams tagged CSV lines over USB serial
and this script saves them in the current folder:
    CAL,...  -> calibration.csv   (one row per calibration)
    LOG,...  -> emg_log.csv       (every logged sample, all trials)
    SUM,...  -> emg_summary.csv   (one row per trial)
Everything you type is sent to the Pico when you press Enter.

Usage:  python emg_capture.py          (auto-detects the Pico)
        python emg_capture.py COM6     (or give the port)
Needs:  pip install pyserial
"""

import os
import sys
import threading

import serial
from serial.tools import list_ports

PICO_VID = 0x2E8A

in_trial = threading.Event()   # set while LOG lines are arriving
trial_done = threading.Event()  # set when the SUM line arrives

FILES = {
    "CAL": ("calibration.csv", "baseline_v,noise_v,mvc_v,threshold_v"),
    "LOG": ("emg_log.csv", "label,t_s,volts,pct_mvc,active"),
    "SUM": ("emg_summary.csv",
            "label,duration_s,mean_pct_mvc,peak_pct_mvc,pct_time_active,baseline_v,mvc_v"),
}


def find_port():
    for p in list_ports.comports():
        if p.vid == PICO_VID:
            return p.device
    return None


def append_row(tag, row):
    path, header = FILES[tag]
    new_file = not os.path.exists(path)
    with open(path, "a", newline="") as f:
        if new_file:
            f.write(header + "\n")
        f.write(row + "\n")


def reader(ser):
    while True:
        try:
            raw = ser.readline()
        except serial.SerialException:
            print("\nPico disconnected.")
            os._exit(1)
        if not raw:
            continue
        line = raw.decode(errors="replace").strip()
        tag, _, row = line.partition(",")

        if tag not in FILES:
            print(line)
            continue

        append_row(tag, row)
        if tag == "LOG":
            in_trial.set()
            _, _, volts, pct, active = row.split(",")
            print("%s V   %6s %%MVC   %s" % (volts, pct, "ACTIVE" if active == "1" else "rest"))
        elif tag == "CAL":
            print("(calibration saved to calibration.csv)")
        elif tag == "SUM":
            in_trial.clear()
            trial_done.set()
            print("(saved to emg_log.csv and emg_summary.csv)")


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if port is None:
        print("No Pico found. Plug it in, or pass the port: python emg_capture.py COM6")
        sys.exit(1)

    ser = serial.Serial(port, 115200, timeout=0.5)  # opening sets DTR, which the Pico waits for
    print("Connected to %s. Saving files in %s" % (port, os.getcwd()))
    print("Ctrl+C to quit.\n")
    threading.Thread(target=reader, args=(ser,), daemon=True).start()

    try:
        while True:
            ser.write((input() + "\n").encode())
    except (KeyboardInterrupt, EOFError):
        if in_trial.is_set():
            # Stop the trial properly so its SUM row is still saved.
            print("\nStopping trial...")
            ser.write(b"q\n")
            trial_done.wait(timeout=2)
        print("\nBye.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
