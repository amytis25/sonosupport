#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "emg.h"
#include "emg_cal.h"

#define LABEL_MAX 32

/*
 * Reads one line from USB serial (blocking). Accepts \n, \r or \r\n
 * endings. Spaces become '_' and commas are dropped so the label is
 * safe inside a CSV field.
 */
static void read_label(char *buf, size_t size) {
    static bool last_was_cr = false;
    size_t len = 0;

    while (true) {
        int c = getchar();
        if (c == '\n' && last_was_cr) {  /* second half of \r\n */
            last_was_cr = false;
            continue;
        }
        last_was_cr = (c == '\r');
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == ',') {
            continue;
        }
        if (len + 1 < size) {
            buf[len++] = (c == ' ') ? '_' : (char)c;
        }
    }
    buf[len] = '\0';
}

int main() {
    stdio_init_all();
    emg_init();

    /* Wait for a serial terminal so the first prompts are not lost. */
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(500);

    emg_cal_t cal;
    bool have_cal = false;
    char label[LABEL_MAX];

    while (true) {
        if (!have_cal) {
            printf("\nPress Enter to start calibration.\n");
            read_label(label, sizeof label);
            have_cal = emg_cal_run(&cal);
            continue;
        }

        printf("\nTrial label (e.g. with_support / without_support), or c to recalibrate:\n");
        read_label(label, sizeof label);

        if (strcmp(label, "c") == 0) {
            have_cal = false;
            continue;
        }
        if (label[0] == '\0') {
            strcpy(label, "trial");
        }
        emg_cal_trial(&cal, label);
    }

    return 0;
}
