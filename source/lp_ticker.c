/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cmsis.h"
#include "lp_ticker_api.h"
#include "mbed_assert.h"
#include "sleep_api.h"
#include "compiler.h"
#include "sysclk.h"
#include "tc.h"
#include "us_ticker_api.h"

uint8_t lp_ticker_inited = 0;
extern uint8_t us_ticker_inited;
extern uint8_t g_sys_init;
extern volatile uint32_t overflow32bitcounter;
static uint16_t lp_ticker_interrupt_counter;
static uint16_t lp_ticker_interrupt_offset;
static uint32_t lastsetval = 0;

#define TICKER_COUNTER_lp        TC0
#define TICKER_COUNTER_CHANNEL2  2
#define TICKER_COUNTER_IRQn2     TC2_IRQn
#define TICKER_COUNTER_Handlr2   TC2_Handler

#define OVERFLOW_16bit_VALUE_LP    0xFFFF


void TICKER_COUNTER_Handlr2(void)
{
    uint32_t status=tc_get_status(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);
    uint32_t interrupmask=tc_get_interrupt_mask(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);

    if (((status & interrupmask)  & TC_IER_CPCS)) {
        if(lp_ticker_interrupt_counter) {
            lp_ticker_interrupt_counter--;
        } else {
            if(lp_ticker_interrupt_offset) {
                tc_stop(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);
                tc_write_rc(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2, (uint32_t)lp_ticker_interrupt_offset);
                tc_start(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);
                lp_ticker_interrupt_offset=0;
            }
        }
    }
}

void lp_ticker_init(void)
{
    if(lp_ticker_inited)
        return;
    if (!us_ticker_inited)
        us_ticker_init();

    tc_init(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2, TC_CMR_TCCLKS_TIMER_CLOCK5);
    lp_ticker_inited = 1;
}

uint32_t lp_ticker_read()
{
    if (!lp_ticker_inited)
        lp_ticker_init();
    return us_ticker_read();
}

uint32_t lp_ticker_get_overflows_counter(void)
{
    return overflow32bitcounter;
}

uint32_t lp_ticker_get_compare_match(void)
{
    if (!lp_ticker_inited)
        lp_ticker_init();
    return lastsetval;
}

void lp_ticker_set_interrupt(uint32_t now, uint32_t time)
{
    uint32_t cur_time;
    int32_t delta;

    cur_time = us_ticker_read();
    delta = (int32_t)((uint32_t)time - now);
    if (delta < 0) {
        /* Event already occurred in past */
        return;
    }

    uint16_t interruptat=0;
    lastsetval = time;

    if(delta > OVERFLOW_16bit_VALUE_LP) {
        lp_ticker_interrupt_counter= (delta/OVERFLOW_16bit_VALUE_LP) -1;
        lp_ticker_interrupt_offset=delta%OVERFLOW_16bit_VALUE_LP;
        interruptat=OVERFLOW_16bit_VALUE_LP;
    } else {
        lp_ticker_interrupt_counter=0;
        lp_ticker_interrupt_offset=0;
        interruptat=delta;
    }

    NVIC_DisableIRQ(TICKER_COUNTER_IRQn2);
    NVIC_SetVector(TICKER_COUNTER_IRQn2, (uint32_t)TICKER_COUNTER_Handlr2);

    tc_write_rc(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2, (uint32_t)interruptat);

    NVIC_ClearPendingIRQ(TICKER_COUNTER_IRQn2);
    NVIC_SetPriority(TICKER_COUNTER_IRQn2, 0);
    NVIC_EnableIRQ(TICKER_COUNTER_IRQn2);
    tc_enable_interrupt(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2, TC_IDR_CPCS );

    tc_start(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);
}

void lp_ticker_disable_interrupt(void)
{
    tc_stop(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2);
    tc_disable_interrupt(TICKER_COUNTER_lp, TICKER_COUNTER_CHANNEL2, TC_IDR_CPCS);
    NVIC_DisableIRQ(TICKER_COUNTER_IRQn2);
}

void lp_ticker_clear_interrupt(void)
{
    NVIC_ClearPendingIRQ(TICKER_COUNTER_IRQn2);
}

void lp_ticker_sleep_until(uint32_t now, uint32_t time)
{
    sleep_t sleep_obj;
    lp_ticker_set_interrupt(now, time);
    mbed_enter_sleep(&sleep_obj);
}
