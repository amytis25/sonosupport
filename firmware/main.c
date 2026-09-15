#include <stdio.h>
#include "pico/stdlib.h"
#include "emg.h"

int main() {
    stdio_init_all();
    emg_init();

    while (true) {
        uint16_t raw = emg_read_raw();
        float voltage = emg_read_voltage();

        printf("raw: %u  voltage: %.3f\n", raw, voltage);

        sleep_ms(50); // ~20 samples/sec, adjust as needed
    }

    return 0;
}