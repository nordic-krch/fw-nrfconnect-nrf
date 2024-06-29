/*$$$LICENCE_NORDIC_STANDARD<2014>$$$*/
/** @file
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 * This file contains the source code for a sample application to blink LEDs.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <hal/nrf_gpio.h>

#define LED_0 13
/**
 * @brief Function for application main entry.
 */
int main(void)
{
	/* Configure board. */
	nrf_gpio_cfg_output(LED_0);

	/* Toggle LEDs. */
	while (true) {
		nrf_gpio_pin_toggle(LED_0);
		k_busy_wait(500 * 1000);
	}
}

/**
 *@}
 **/
