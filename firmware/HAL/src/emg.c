/*
 * emg.c
 *
 * Description: MyoWare 2.0 EMG sensor HAL implementation.
 *              See emg.h for the contract. Configures GP26 as an ADC
 *              input and reads the sensor's ENV (envelope) output.
 * Inputs:  analog voltage on GP26 (ADC0), from MyoWare 2.0 ENV pin
 * Outputs: raw 12-bit ADC reading; voltage in volts
 * Author:  Kanika
 * Created: 2026-09-15
 */

#include "emg.h"
#include "hardware/adc.h"

void emg_init(void) {
    adc_init();
    adc_gpio_init(PIN_EMG_ENV);
    adc_select_input(EMG_ADC_CHANNEL);
}

uint16_t emg_read_raw(void) {
    return adc_read();
}

float emg_read_voltage(void) {
    uint16_t raw = emg_read_raw();
    return raw * 3.3f / 4095.0f;
}