#ifndef PINS_H
#define PINS_H

/*
 * pins.h
 *
 * Description: Single source of truth for every Pico 2 W (RP2350) GPIO
 *              assignment on SonoSupport. Pin numbers appear HERE and
 *              nowhere else in the firmware - every module includes this
 *              header instead of writing a pin number inline.
 *              EMG (MyoWare 2.0) ENV output sits on one of the three
 *              ADC-capable pins (GP26/27/28); it is the only signal this
 *              build reads through the RP2350's onboard ADC.
 * Inputs:  none (compile-time constants only)
 * Outputs: none (compile-time constants only)
 * Author:  Kanika
 * Created: 2026-09-15
 */

/* ---- MyoWare 2.0 EMG sensor (onboard ADC0) ------------------------------
 * GP26/27/28 are the only three RP2350 GPIOs wired to the chip's ADC
 * input mux - every other pin is digital-only. GP26 is ADC channel 0,
 * NOT "ADC26"; the channel number is passed to adc_select_input(), the
 * GPIO number is passed to adc_gpio_init(). Both are needed and they are
 * not interchangeable.
 *
 * VIN is deliberately powered from the Pico's 3V3(OUT) rail, not 5V.
 * The ENV output swings 0-VIN, and the RP2350 ADC input is only rated
 * up to 3.3V - powering the sensor at 5V would put the ADC pin out of
 * spec.
 */
#define PIN_EMG_ENV             26u   /* ADC0 input <- MyoWare ENV (0-3.3V) */
#define EMG_ADC_CHANNEL          0u   /* adc_select_input() channel for GP26 */

#endif /* PINS_H */