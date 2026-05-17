// Copyright (c) 2012, vsluiter <info-at- hackvandedam.nl>
// Copyright (c) 2026, osemenyuk-114 (github.com/osemenyuk-114)
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "hal.h"
#include "openpf.h"

/*
Code is intended for 8 MHz operation.
LEGO Power Functions RC v1.2.
*/

#define EEPROM_ADDRESS_CHANNEL (uint8_t *)0
#define EEPROM_ADDRESS_CHANNEL_OUTPUT (uint8_t *)1

// Button timing constants
#define BUTTON_LONG_PRESS_TICKS 99
#define BUTTON_VERY_LONG_PRESS_TICKS 9523

// EEPROM helpers
static inline uint8_t eeprom_read_u8(const uint8_t *addr)
{
    eeprom_busy_wait();
    return eeprom_read_byte((uint8_t *)addr);
}

static inline void eeprom_write_u8(const uint8_t *addr, uint8_t value)
{
    eeprom_busy_wait();
    eeprom_write_byte((uint8_t *)addr, value);
}

volatile uint8_t pwmport;
volatile pwm_reg_t pwma;
struct OpenPfRx_channel channel_pwm;
volatile uint8_t ocr1_mask_a = 0xFF;

#if (NumberOfOutputChannels == 2)
volatile pwm_reg_t pwmb;
volatile uint8_t ocr1_mask_b = 0xFF;
volatile uint8_t ocr1_mask_both = 0xFF;
#endif

volatile uint8_t externalint = 0;
volatile uint8_t timerflag105us = 0;

static void UpdateOutputValues(struct OpenPfRx_output *);
static void ResetPWMChannel(struct OpenPfRx_channel *);

#if defined(StartButtonEnabled)
volatile uint8_t sleepcounter = 0;
#endif

volatile uint16_t ChButtonHoldTime = 0;
volatile uint8_t ChButtonState = 0;

// ============================================================================
// Interrupt Service Routines
// ============================================================================

ISR(EXTERNAL_INTERRUPT, ISR_NOBLOCK) // External interrupt handler
{
    // DISABLE_IR_INT; // Disable pin-change interrupt; timer ISR re-enables it.
    RESET_IR_TIMER;

    if (ChButtonState == 0)
        ChButtonState = 1;

    OpenPfRxPinInterruptState(); // Process counter/pin state to gather IR data.
}

ISR(TIMER_105US, ISR_NOBLOCK)
{
    OpenPfRx105usState();
    timerflag105us = 1;
}

#if defined(StartButtonEnabled)
ISR(STBUTTON_INTERRUPT)
{
}

ISR(WDT_VECT)
{
    sleepcounter++;

    if (sleepcounter > 15)
    {
        ResetPWMChannel(&channel_pwm);
        DISABLE_IR_INT;
#ifdef IR_POWER
        DISABLE_IR_POWER;
#endif
#if (NumberOfOutputChannels == 1)
        A_PORT = (A_PORT & (~(A_C1 | A_C2))) | (pwmport & (A_C1 | A_C2));
#else
        A_PORT = (A_PORT & (~(A_C1 | A_C2 | B_C1 | B_C2))) | (pwmport & (A_C1 | A_C2 | B_C1 | B_C2));
#endif

        BUTTON_PORT &= ~CHBUTTON; // Disable pull-up on CH button pin.
        PCINT_MASK = STBUTTON;
        ENABLE_STBUTTON_INTERRUPT;
        WDT_CTRL = _BV(WDCE) | _BV(WDE); // Disable watchdog.
        WDT_CTRL = 0x00;
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sei();
        sleep_cpu();
        // Sleeping...
        sleep_disable();
        cli();
        WDT_CTRL = _BV(WDCE) | _BV(WDE); // Enable watchdog reset.
        WDT_CTRL = _BV(WDE);             // Timeout 16 ms, reset.

        while (1)
            ;
    }
}
#else
ISR(WDT_VECT)
{
}
#endif

// PWM timer period start ISR
ISR(PWMTIMER_PERIODSTART)
{
#if (NumberOfOutputChannels == 1)
    A_PORT = (A_PORT & (~(A_C1 | A_C2))) | (pwmport & (A_C1 | A_C2));
#else
    A_PORT = (A_PORT & (~(A_C1 | A_C2 | B_C1 | B_C2))) | (pwmport & (A_C1 | A_C2 | B_C1 | B_C2));
#endif

    OCR1A = pwma;

#if (NumberOfOutputChannels == 2)
    OCR1B = pwmb;

    if (ocr1_mask_a == ocr1_mask_b)
    {
        ocr1_mask_both = ocr1_mask_a & ocr1_mask_b;
        OCR1B = pwmb + 1; // Always execute the PWMA interrupt routine first.
    }
    else
        ocr1_mask_both = 0xFF;
#endif
}

ISR(PWMTIMER_PWMA_INTERRUPT)
{
#if (NumberOfOutputChannels == 1)
    A_PORT &= ocr1_mask_a;
#else
    A_PORT &= (ocr1_mask_a & ocr1_mask_both);
#endif
}

#if (NumberOfOutputChannels == 2)
ISR(PWMTIMER_PWMB_INTERRUPT)
{
    A_PORT &= (ocr1_mask_b & ocr1_mask_both);
}
#endif

// ============================================================================
// Main
// ============================================================================

int main()
{
    uint8_t channelnumber;
    uint8_t channeloutput;
    uint16_t legochannel[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#if defined(StartButtonEnabled)
    MCUSR &= ~(_BV(WDRF));
    WDT_CTRL = _BV(WDCE) | _BV(WDE);
    WDT_CTRL = _BV(WDIE) | _BV(WDP3) | _BV(WDP0); // 8-second timeout, watchdog interrupt enabled.
#endif

    IoInit();

    power_adc_disable();
    power_usi_disable();

    // --- Channel / output initialization ---
    sleep_disable();
    
    // Initialize EEPROM on first startup (detect uninitialized state = 0xFF)
    uint8_t eeprom_channel_raw = eeprom_read_u8(EEPROM_ADDRESS_CHANNEL);
    uint8_t eeprom_output_raw = eeprom_read_u8(EEPROM_ADDRESS_CHANNEL_OUTPUT);
    
    if (eeprom_channel_raw == 0xFF && eeprom_output_raw == 0xFF)
    {
        // First startup: initialize with defaults (ch1 = 0, red output = 0)
        eeprom_write_u8(EEPROM_ADDRESS_CHANNEL, 0);
        eeprom_write_u8(EEPROM_ADDRESS_CHANNEL_OUTPUT, 0);
        channelnumber = 0;
        channeloutput = 0;
    }
    else
    {
        channelnumber = (eeprom_channel_raw & 0x03);
        channeloutput = (eeprom_output_raw & 0x01);
    }

#if (NumberOfOutputChannels == 2)
    uint8_t secondchannel = channeloutput ^ 0x01;
#endif

    OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, channelnumber);
    SetupExternalInterrupt();

#ifdef IR_POWER
    ENABLE_IR_POWER;
#endif

    ENABLE_IR_INT;
    SetupPWMTimer();
    ENABLE_PWMA_INTERRUPT;

#if (NumberOfOutputChannels == 2)
    ENABLE_PWMB_INTERRUPT;
#endif

    Setup105usclock();
    OpenPfRx_rx.newdata = 0;
    sei(); // Enable interrupts

    // ========================================================================
    // Main loop
    // ========================================================================
    for (;;)
    {
#if defined(StartButtonEnabled)
        if (STBUTTON_PUSHED)
        {
            cli();
            A_PORT = (A_PORT & (~(A_C2))) | A_C1; // OM_FWD

            while (STBUTTON_PUSHED)
                ;

            A_PORT = A_PORT & (~(A_C1 | A_C2)); // OM_FLOAT
            sleepcounter = 0;
            ResetPWMChannel(&channel_pwm);
            sei();
        }
#endif

        // --- Process new IR data ---
        if (OpenPfRx_rx.newdata)
        {
            OpenPfRx_rx.newdata = 0;

            if (OpenPfRxVerifyChecksum(OpenPfRx_rx.rxdata))
            {
                channelnumber = OpenPfRxGetChannelNumber(OpenPfRx_rx.rxdata);
                legochannel[channelnumber] = OpenPfRx_rx.rxdata;

                if (channelnumber == channel_pwm.channel_number)
                {
                    uint8_t a_mask = 0xFF;
                    pwm_reg_t a_pwm = 0x0;
                    uint8_t enablepwma = 1;

#if (NumberOfOutputChannels == 1)
                    uint8_t temp_var = ~(A_C1 | A_C2);
#else
                    uint8_t b_mask = 0xFF;
                    pwm_reg_t b_pwm = 0x0;
                    uint8_t enablepwmb = 1;
                    uint8_t temp_var = ~(A_C1 | A_C2 | B_C1 | B_C2);
#endif

#if defined(StartButtonEnabled)
                    sleepcounter = 0;
#endif

                    channel_pwm.timeout = channel_pwm.timeout_limit;
                    OpenPfRxInterpreter((const uint16_t *)&legochannel[channelnumber], &channel_pwm);

                    if (channel_pwm.channel_output[channeloutput].output_mode == (OM_FWD) || channel_pwm.channel_output[channeloutput].output_mode == (OM_BWD))
                    {
                        if (channel_pwm.channel_output[channeloutput].pwmvalue == OpenPfRx_MIN_PWM_VALUE || channel_pwm.channel_output[channeloutput].pwmvalue == OpenPfRx_MAX_PWM_VALUE)
                            enablepwma = 0;
                    }
                    else
                        enablepwma = 0;

#if (NumberOfOutputChannels == 2)
                    if (channel_pwm.channel_output[secondchannel].output_mode == (OM_FWD) || channel_pwm.channel_output[secondchannel].output_mode == (OM_BWD))
                    {
                        if (channel_pwm.channel_output[secondchannel].pwmvalue == OpenPfRx_MIN_PWM_VALUE || channel_pwm.channel_output[secondchannel].pwmvalue == OpenPfRx_MAX_PWM_VALUE)
                            enablepwmb = 0;
                    }
                    else
                        enablepwmb = 0;
#endif

                    UpdateOutputValues(&channel_pwm.channel_output[channeloutput]);

#if (NumberOfOutputChannels == 2)
                    UpdateOutputValues(&channel_pwm.channel_output[secondchannel]);
#endif

                    if (channel_pwm.channel_output[channeloutput].C1)
                        temp_var |= A_C1;

                    if (channel_pwm.channel_output[channeloutput].C2)
                        temp_var |= A_C2;

#if (NumberOfOutputChannels == 2)
                    if (channel_pwm.channel_output[secondchannel].C1)
                        temp_var |= B_C1;

                    if (channel_pwm.channel_output[secondchannel].C2)
                        temp_var |= B_C2;
#endif

                    uint8_t c_sreg = SREG;

                    if (enablepwma)
                    {
                        a_mask = ~(A_C1 | A_C2);
                        a_pwm = (channel_pwm.channel_output[channeloutput].pwmvalue);
                    }
                    else
                    {
                        a_mask = 0xFF;
                        a_pwm = 0x0;
                    }

#if (NumberOfOutputChannels == 2)
                    if (enablepwmb)
                    {
                        b_mask = ~(B_C1 | B_C2);
                        b_pwm = (channel_pwm.channel_output[secondchannel].pwmvalue);
                    }
                    else
                    {
                        b_mask = 0xFF;
                        b_pwm = 0x0;
                    }
#endif

                    cli();

                    pwma = a_pwm;
                    ocr1_mask_a = a_mask;

#if (NumberOfOutputChannels == 2)
                    pwmb = b_pwm;
                    ocr1_mask_b = b_mask;
#endif

                    pwmport = temp_var;
                    SREG = c_sreg;
                }
            }
        }

        // --- 105us timer tick processing ---
        if (timerflag105us)
        {
            timerflag105us = 0;
            channel_pwm.channel_output[channeloutput].brakethenfloatcount++;

#if (NumberOfOutputChannels == 2)
            channel_pwm.channel_output[secondchannel].brakethenfloatcount++;
#endif

            if (channel_pwm.timeout)
                --channel_pwm.timeout;

            if (ChButtonState == 1)
            {
                ChButtonState = 2;
                ChButtonHoldTime = 0;
            }

            if (ChButtonState == 2)
            {
                if (CHBUTTON_PUSHED)
                    ChButtonHoldTime++;
                else
                {
                    if (ChButtonHoldTime > BUTTON_LONG_PRESS_TICKS)
                    {
                        uint8_t sreg = SREG;
                        cli();
                        uint8_t temp_channel;

                        if (ChButtonHoldTime > BUTTON_VERY_LONG_PRESS_TICKS)
                        {
                            channeloutput ^= 0x01;
#if (NumberOfOutputChannels == 2)
                            secondchannel = channeloutput ^ 0x01;
#endif
                            temp_channel = (channel_pwm.channel_number) & 0x03;
                            eeprom_write_u8(EEPROM_ADDRESS_CHANNEL_OUTPUT, channeloutput);
                        }
                        else
                        {
                            temp_channel = (channel_pwm.channel_number + 1) & 0x03;
                            eeprom_write_u8(EEPROM_ADDRESS_CHANNEL, temp_channel);
                        }

                        OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, temp_channel);
                        ResetPWMChannel(&channel_pwm);
                        SREG = sreg;
                    }

                    ChButtonState = 0;
                }
            }
        }

        // --- Timeout handling ---
        if (channel_pwm.timeout == 0 && channel_pwm.timeout_action)
            ResetPWMChannel(&channel_pwm);

        // --- Brake-then-float handling ---
        if ((channel_pwm.channel_output[channeloutput].output_mode == OM_BRAKE_THEN_FLOAT) && channel_pwm.channel_output[channeloutput].brakethenfloatcount >= 2000)
        {
            channel_pwm.channel_output[channeloutput].output_mode = OM_FLOAT;
            channel_pwm.channel_output[channeloutput].pwmindex = PWM_FLOAT;
            pwmport &= ~(A_C1 | A_C2);
            ocr1_mask_a = 0xFF;
        }

#if (NumberOfOutputChannels == 2)
        if ((channel_pwm.channel_output[secondchannel].output_mode == OM_BRAKE_THEN_FLOAT) && channel_pwm.channel_output[secondchannel].brakethenfloatcount >= 2000)
        {
            channel_pwm.channel_output[secondchannel].output_mode = OM_FLOAT;
            channel_pwm.channel_output[secondchannel].pwmindex = PWM_FLOAT;
            pwmport &= ~(B_C1 | B_C2);
            ocr1_mask_b = 0xFF;
        }
#endif
    }
}

// ============================================================================
// Helper functions
// ============================================================================

static void ResetPWMChannel(struct OpenPfRx_channel *channel)
{
    channel->timeout_action = 0;
    channel->channel_output[RED].output_mode = OM_FLOAT;
    channel->channel_output[BLUE].output_mode = OM_FLOAT;
    channel->channel_output[RED].pwmindex = PWM_FLOAT;
    channel->channel_output[BLUE].pwmindex = PWM_FLOAT;
    ocr1_mask_a = 0xFF;

#if (NumberOfOutputChannels == 1)
    pwmport &= ~(A_C1 | A_C2);
#else
    ocr1_mask_b = 0xFF;
    pwmport &= ~(A_C1 | A_C2 | B_C1 | B_C2);
#endif
}

static void UpdateOutputValues(struct OpenPfRx_output *output)
{
    switch (output->output_mode)
    {

    case OM_FWD:
    {
        if (output->pwmvalue != OpenPfRx_MIN_PWM_VALUE)
        {
            output->C1 = 1;
            output->C2 = 0;
        }
        else
        {
            // FLOAT
            output->C1 = 0;
            output->C2 = 0;
        }

        break;
    }

    case OM_BWD:
    {
        if (output->pwmvalue != OpenPfRx_MIN_PWM_VALUE)
        {
            output->C1 = 0;
            output->C2 = 1;
        }
        else
        {
            // FLOAT
            output->C1 = 0;
            output->C2 = 0;
        }

        break;
    }

    case OM_FLOAT:
    {
        output->C1 = 0;
        output->C2 = 0;
        break;
    }

    case OM_BRAKE_THEN_FLOAT:
        output->brakethenfloatcount = 0;

    case OM_BRAKE:
    {
        output->C1 = 1;
        output->C2 = 1;
        break;
    }

    case OM_INDEPENDENT:

    default:
        break;
    }
}
