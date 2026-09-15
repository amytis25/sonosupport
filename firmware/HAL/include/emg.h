/*
 * emg.h
 *
 * Description: MyoWare 2.0 EMG sensor (DEV-27924) HAL contract.
 *              Reads the sensor's ENV (envelope) output via the Pico's
 *              onboard ADC. See emg.c for the implementation.
 * Inputs:  analog voltage on GP26 (ADC0), from MyoWare 2.0 ENV pin
 * Outputs: raw 12-bit ADC reading; voltage in volts
 * Author:  Kanika
 * Created: 2026-09-15
 */

#ifndef EMG_H
#define EMG_H

#include <stdint.h>
#include "pins.h"

// Initializes the ADC and GPIO pin for the EMG sensor
void emg_init(void);

// Returns the raw 12-bit ADC reading (0-4095)
uint16_t emg_read_raw(void);

// Returns the reading converted to volts (0.0-3.3V)
float emg_read_voltage(void);

#endif // EMG_H