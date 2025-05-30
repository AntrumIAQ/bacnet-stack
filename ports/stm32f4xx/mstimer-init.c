/**
 * @file
 * @brief Generate a periodic timer tick for use by generic timers in the code.
 * @author Steve Karg
 * @date 2011
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include "bacnet/basic/sys/mstimer.h"
#include "bacnet/basic/sys/debug.h"
#include "stm32f4xx.h"
#include "led.h"

/* counter for the various timers */
static volatile unsigned long Millisecond_Counter;

/**
 * Handles the interrupt from the timer
 */
void SysTick_Handler(void)
{
    /* increment the tick count */
    Millisecond_Counter++;
    /* run any callbacks */
    mstimer_callback_handler();
}

/**
 * Returns the continuous milliseconds count, which rolls over
 *
 * @return the current milliseconds count
 */
unsigned long mstimer_now(void)
{
    return Millisecond_Counter;
}

/**
 * Timer setup for 1 millisecond timer
 */
void mstimer_init(void)
{
    /* Setup SysTick Timer for 1ms interrupts  */
    if (SysTick_Config(SystemCoreClock / 1000)) {
        while (1) {
            /* config error? return  1 = failed, 0 = successful */
        }
    }
}
