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
#if defined(ATTINY85)
#include <avr/wdt.h>
#endif
#include "hal.h"
#include "openpf.h"

/*
Code is meant for 8MHz operation
Lego PowerFunctions RC v1.2
*/

#define EEPROM_ADDRESS_CHANNEL (uint8_t *)0
#if !defined(ATTINY84)
#define EEPROM_ADDRESS_CHANNEL_OUTPUT (uint8_t *)1
#endif

uint8_t pwmport;
pwm_reg_t pwma;
struct OpenPfRx_channel channel_pwm;
volatile uint8_t ocr1_mask_a = 0xFF;

#if (NumberOfOutputChannels == 2)
pwm_reg_t pwmb;
volatile uint8_t ocr1_mask_b = 0xFF;
volatile uint8_t ocr1_mask_both = 0xFF;
#endif

volatile uint8_t externalint = 0;
volatile uint8_t timerflag105us = 0;

static void UpdateOutputValues(struct OpenPfRx_output *);
static void ResetPWMChannel(struct OpenPfRx_channel *);

#if defined(ATTINY85) && defined(StartButtonEnabled)
uint8_t sleepcounter = 0;
#endif

#if defined(ATTINY85)
uint16_t ChButtonHoldTime = 0;
uint8_t ChButtonState = 0;
#endif

// ============================================================================
// Interrupt Service Routines
// ============================================================================

ISR(EXTERNAL_INTERRUPT, ISR_NOBLOCK) // External Interrupt Handler
{
    // DISABLE_IR_INT; // Disable pin change interrupt. Enabled in timer interrupt routine.
    RESET_IR_TIMER;

#if defined(ATTINY85)
    if (ChButtonState == 0)
        ChButtonState = 1;

    OpenPfRxPinInterruptState(); // process counter/pin state to gather IR data
#else
    externalint = 1;
#endif
}

ISR(TIMER_105US, ISR_NOBLOCK)
{
    OpenPfRx105usState();
    timerflag105us = 1;
}

#if defined(ATTINY85) && defined(StartButtonEnabled)
ISR(STBUTTON_INTERRUPT)
{
}

ISR(WDT_VECT)
{
    sleepcounter++;

    if (sleepcounter > 15)
    {
        ResetPWMChannel(&channel_pwm);

#if (NumberOfOutputChannels == 1)
        DISABLE_IR_INT;
        DISABLE_IR_POWER;
        A_PORT = (A_PORT & (~(A_C1 | A_C2))) | (pwmport & (A_C1 | A_C2));
#else
        A_PORT = (A_PORT & (~(A_C1 | A_C2 | B_C1 | B_C2))) | (pwmport & (A_C1 | A_C2 | B_C1 | B_C2));
#endif

        BUTTON_PORT &= ~CHBUTTON; // disable pull-up on CH button pin
        PCMSK = STBUTTON;
        ENABLE_STBUTTON_INTERRUPT;
        WDTCR = _BV(WDCE) | _BV(WDE); // disable watchdog
        WDTCR = 0x00;
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sei();
        sleep_cpu();
        // sleeping...
        sleep_disable();
        cli();
        WDTCR = _BV(WDCE) | _BV(WDE); // enable watchdog to reset
        WDTCR = _BV(WDE);             // time-out 16ms, reset

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
#if defined(ATTINY84) || defined(ATTINY85_DUPLO_TRAIN)
    STOP_PWM_TIMER;
#endif

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
        OCR1B = pwmb + 1; // always execute interrupt routine for PWMA first
    }
    else
        ocr1_mask_both = 0xFF;
#endif

#if defined(ATTINY84) || defined(ATTINY85_DUPLO_TRAIN)
    RESET_PWM_TIMER;
    START_PWM_TIMER;
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

#if defined(ATTINY85) && defined(StartButtonEnabled)
    MCUSR &= ~(_BV(WDRF));
    WDTCR = _BV(WDCE) | _BV(WDE);
    WDTCR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0); // time-out 8 second, watchdog interrupt enabled
#endif

    IoInit();

#if defined(ATTINY84)
    while (BUTTON_PUSHED)
        ;
#endif

    power_adc_disable();
    power_usi_disable();

    // --- Channel / output initialization ---
#if defined(ATTINY84)
    eeprom_busy_wait();
    channelnumber = (eeprom_read_byte(EEPROM_ADDRESS_CHANNEL) & 0x03);
    channeloutput = 0; // A = RED, B = BLUE (fixed on board 84)
#elif defined(ATTINY85)
    sleep_disable();
    eeprom_busy_wait();
    channelnumber = (eeprom_read_byte(EEPROM_ADDRESS_CHANNEL) & 0x03);
    eeprom_busy_wait();
    channeloutput = (eeprom_read_byte(EEPROM_ADDRESS_CHANNEL_OUTPUT)) & 0x01;
#elif defined(ChannelButtonEnabled)
    while (CHBUTTON_PUSHED)
        ;

    eeprom_busy_wait();
    channelnumber = (eeprom_read_byte(EEPROM_ADDRESS_CHANNEL) & 0x03);
    eeprom_busy_wait();
    channeloutput = (eeprom_read_byte(EEPROM_ADDRESS_CHANNEL_OUTPUT)) & 0x01;
#else
    channelnumber = 0;
    channeloutput = 0;
#endif

#if (NumberOfOutputChannels == 2)
    uint8_t secondchannel = channeloutput ^ 0x01;
#endif

    OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, channelnumber);
    SetupExternalInterrupt();

#if defined(ATTINY85) && (NumberOfOutputChannels == 1)
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

#if !defined(ATTINY85)
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
#endif

    // ========================================================================
    // Main loop
    // ========================================================================
    for (;;)
    {
#if defined(ATTINY84)
        static uint16_t red_led_downcounter = 0;

        if (red_led_downcounter == 0)
            BICOLOR_GREEN;
        else
            red_led_downcounter--;
#endif

#if defined(ATTINY85) && defined(StartButtonEnabled)
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
#if defined(ATTINY84)
                BICOLOR_RED;
                red_led_downcounter = 10000;
#endif
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

#if defined(ATTINY85) && defined(StartButtonEnabled)
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
                    sei();
                    SREG = c_sreg;
                }
            }
        }

        // --- Process deferred external interrupt (boards 84, 45) ---
#if !defined(ATTINY85)
        if (externalint)
        {
            externalint = 0;
            OpenPfRxPinInterruptState();
        }
#endif

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

#if defined(ATTINY85)
            // --- Board 85: interrupt-driven channel button state machine ---
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
                    if (ChButtonHoldTime > 95)
                    {
                        cli();

                        uint8_t temp_channel;

                        if (ChButtonHoldTime > 9523)
                        {
                            channeloutput ^= 0x01;

#if (NumberOfOutputChannels == 2)
                            secondchannel = channeloutput ^ 0x01;
#endif

                            temp_channel = (channel_pwm.channel_number) & 0x03;

                            eeprom_busy_wait();
                            eeprom_write_byte(EEPROM_ADDRESS_CHANNEL_OUTPUT, channeloutput);
                        }
                        else
                        {
                            temp_channel = (channel_pwm.channel_number + 1) & 0x03;
                            eeprom_busy_wait();
                            eeprom_write_byte(EEPROM_ADDRESS_CHANNEL, temp_channel);
                        }

                        OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, temp_channel);
                        ResetPWMChannel(&channel_pwm);
                        sei();
                    }

                    ChButtonState = 0;
                }
            }
#endif // defined(ATTINY85)
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

        // --- Board 84: polling channel button ---
#if defined(ATTINY84)
        if (BUTTON_PUSHED)
        {
            uint8_t push_8ms = 0;
            cli();

            while (BUTTON_PUSHED)
            {
                for (uint16_t tempvar = 0; tempvar < 65000; tempvar++)
                    ;

                if (push_8ms <= 100)
                    ++push_8ms;
            }

            if (push_8ms > 99)
                ; // long press: do nothing on board 84
            else
            {
                uint8_t temp_channel = (channel_pwm.channel_number + 1) & 0x03;
                OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, temp_channel);
                eeprom_busy_wait();
                eeprom_write_byte(EEPROM_ADDRESS_CHANNEL, temp_channel);
                ResetPWMChannel(&channel_pwm);
            }

            sei();
        }
#endif

        // --- Board 45: polling channel button with output swap ---
#if defined(ATTINY85_DUPLO_TRAIN) && defined(ChannelButtonEnabled)
        if (CHBUTTON_PUSHED)
        {
            uint8_t push_8ms = 0;
            cli();

            while (CHBUTTON_PUSHED)
            {
                for (uint16_t tempvar = 0; tempvar < 65000; tempvar++)
                    ;

                if (push_8ms <= 100)
                    ++push_8ms;
            }

            uint8_t temp_channel;

            if (push_8ms > 99)
            {
                channeloutput ^= 0x01;

#if (NumberOfOutputChannels == 2)
                secondchannel = channeloutput ^ 0x01;
#endif

                temp_channel = (channel_pwm.channel_number) & 0x03;

                eeprom_busy_wait();
                eeprom_write_byte(EEPROM_ADDRESS_CHANNEL_OUTPUT, channeloutput);
            }
            else
            {
                temp_channel = (channel_pwm.channel_number + 1) & 0x03;
                eeprom_busy_wait();
                eeprom_write_byte(EEPROM_ADDRESS_CHANNEL, temp_channel);
            }

            OpenPfRx_channel_init((struct OpenPfRx_channel *)&channel_pwm, temp_channel);

            ResetPWMChannel(&channel_pwm);
            sei();
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
