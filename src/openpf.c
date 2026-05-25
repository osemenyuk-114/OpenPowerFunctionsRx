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

#include "openpf.h"

uint8_t OpenPfRxOutputModePwmLUT[16][2] = // Used for Single Output and Combo PWM modes.
    {
        {PWM_OFF, OM_FLOAT},
        {PWM_STEP1, OM_FWD},
        {PWM_STEP2, OM_FWD},
        {PWM_STEP3, OM_FWD},
        {PWM_STEP4, OM_FWD},
        {PWM_STEP5, OM_FWD},
        {PWM_STEP6, OM_FWD},
        {PWM_STEP7, OM_FWD},
        {PWM_BRAKE_THEN_FLOAT, OM_BRAKE_THEN_FLOAT},
        {PWM_STEP7, OM_BWD},
        {PWM_STEP6, OM_BWD},
        {PWM_STEP5, OM_BWD},
        {PWM_STEP4, OM_BWD},
        {PWM_STEP3, OM_BWD},
        {PWM_STEP2, OM_BWD},
        {PWM_STEP1, OM_BWD},
};

uint8_t OpenPfRxComboDirectModeLUT[4][2] =
    {
        {PWM_FLOAT, OM_FLOAT},
        {PWM_FULL, OM_FWD},
        {PWM_FULL, OM_BWD},
        {PWM_BRAKE_THEN_FLOAT, OM_BRAKE_THEN_FLOAT}};

uint8_t OpenPfRx_pwmvalues[] = {
    OpenPfRx_MIN_PWM_VALUE,
    (OpenPfRx_PWM_VALUE_STEP * 2), // PWM_STEP1
    (OpenPfRx_PWM_VALUE_STEP * 3), // PWM_STEP2
    (OpenPfRx_PWM_VALUE_STEP * 4), // PWM_STEP3
    (OpenPfRx_PWM_VALUE_STEP * 5), // PWM_STEP4
    (OpenPfRx_PWM_VALUE_STEP * 6), // PWM_STEP5
    (OpenPfRx_PWM_VALUE_STEP * 7), // PWM_STEP6
    OpenPfRx_MAX_PWM_VALUE         // PWM_STEP7
};

void OpenPfRx_channel_init(volatile struct OpenPfRx_channel *channel, uint8_t number)
{
    channel->channel_number = number;
    channel->channel_output[RED].pwmvalue = PWM_OFF;
    channel->channel_output[BLUE].pwmvalue = PWM_OFF;
    channel->channel_output[RED].pwmindex = 0;
    channel->channel_output[BLUE].pwmindex = 0;
    channel->channel_output[RED].output_mode = OM_FLOAT;
    channel->channel_output[BLUE].output_mode = OM_FLOAT;
    channel->channel_output[RED].brakethenfloatcount = 0;
    channel->channel_output[BLUE].brakethenfloatcount = 0;
    channel->timeout_limit = 11428;
    channel->timeout_action = 0;
    channel->toggle = 0;
}

void OpenPfRxPinInterruptState()
{
    switch (OpenPfRx_rx.state)
    {

    case IDLE:
    {
        OpenPfRx_rx.state = WAIT_FOR_START;
        break;
    }

    case WAIT_FOR_START:
    {
        if (OpenPfRx_rx.periodcounter >= IR_LENGTH_START_STOP) // No flanks detected between IDLE and START/STOP threshold.
        {
            OpenPfRx_rx.state = WAIT_FOR_BIT; // Start reading data.
            OpenPfRx_rx.bit_count = 0;
            OpenPfRx_rx.rxdata = 0;
        }
        else
            OpenPfRx_rx.state = IDLE; // Reset state machine on unexpected flank.

        break;
    }

    case WAIT_FOR_BIT:
    {
        if ((OpenPfRx_rx.periodcounter >= IR_LENGTH_LO) && (OpenPfRx_rx.periodcounter <= IR_LENGTH_START_STOP)) // Pulse length is within expected data range.
        {
            if (OpenPfRx_rx.periodcounter > IR_LENGTH_HI)
                OpenPfRx_rx.rxdata |= 1; // Set bit in the current position.

            OpenPfRx_rx.bit_count++;

            if (OpenPfRx_rx.bit_count == 16) // Last bit received.
                OpenPfRx_rx.state = WAIT_FOR_STOP;
            else
                OpenPfRx_rx.rxdata <<= 1;
        }
        else
            OpenPfRx_rx.state = IDLE;

        break;
    }

    case WAIT_FOR_STOP: // During stop bit, no IR data should be received; timer ISR checks duration.
    {
        OpenPfRx_rx.state = IDLE;
        break;
    }

    default:
        OpenPfRx_rx.state = IDLE;
    }

    OpenPfRx_rx.periodcounter = IR_LENGTH_INIT;
}

void OpenPfRxInterpreter(const uint16_t *rxdata, volatile struct OpenPfRx_channel *channel)
{
    if (*rxdata & OpenPfRx_ESCAPE_MASK) // Extended mode when E bit is set.
        OpenPfRxComboPWMMode(rxdata, channel);
    else
    {
        uint8_t mode = (*rxdata & OpenPfRx_MODE_MASK) >> 8;

        if (mode == OpenPfRx_EXTENDED_MODE)
            OpenPfRxExtendedMode(rxdata, channel);
        else if (mode == OpenPfRx_COMBO_DIRECT_MODE)
            OpenPfRxComboDirectMode(rxdata, channel);
        else if (mode & OpenPfRx_SINGLE_OUTPUT_MODE)
            OpenPfRxSingleOutputMode(rxdata, channel);
        // Reserved modes 2 and 3 are ignored
    }
}

void OpenPfRxComboPWMMode(const uint16_t *rxdata, volatile struct OpenPfRx_channel *channel)
{
    // Use lookup tables for Combo PWM mode.
    uint8_t dataRED = (*rxdata & OpenPfRx_OUTPUTA_MASK) >> 4;
    uint8_t dataBLUE = (*rxdata & OpenPfRx_OUTPUTB_MASK) >> 8;
    channel->timeout_action = 1;
    // channel->timeout = channel->timeout_limit;
    channel->channel_output[RED].pwmindex = OpenPfRxOutputModePwmLUT[dataRED][0];
    channel->channel_output[RED].output_mode = OpenPfRxOutputModePwmLUT[dataRED][1];
    channel->channel_output[RED].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[RED].pwmindex];
    channel->channel_output[BLUE].pwmindex = OpenPfRxOutputModePwmLUT[dataBLUE][0];
    channel->channel_output[BLUE].output_mode = OpenPfRxOutputModePwmLUT[dataBLUE][1];
    channel->channel_output[BLUE].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[BLUE].pwmindex];
}

void OpenPfRxExtendedMode(const uint16_t *rxdata, volatile struct OpenPfRx_channel *channel)
{
    // Data determines function used
    // Toggle bit is verified.
    // No timeout.
    uint8_t function = (*rxdata & OpenPfRx_DATA_MASK) >> 4;

    if (!OpenPfRxVerifyToggleBit(rxdata, channel))
        return; // Skip if toggle bit matches previous message; timeout is not observed.

    switch (function)
    {

    case 0b0000: // brake then float output A
    {
        channel->timeout_action = 0;
        channel->channel_output[RED].pwmindex = PWM_BRAKE_THEN_FLOAT;
        channel->channel_output[RED].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[RED].pwmindex];
        channel->channel_output[RED].output_mode = OM_BRAKE_THEN_FLOAT;
        break;
    }

    case 0b0001: // increment speed on output A
    {
        channel->timeout_action = 0;

        if (channel->channel_output[RED].pwmindex < PWM_FULL)
        {
            channel->channel_output[RED].pwmindex++;
            channel->channel_output[RED].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[RED].pwmindex];

            if (!(channel->channel_output[RED].output_mode == OM_BWD || channel->channel_output[RED].output_mode == OM_FWD)) // If output is not already driving power.
                channel->channel_output[RED].output_mode = OM_FWD;
        }

        break;
    }

    case 0b0010: // decrement next_speed on output A
    {
        channel->timeout_action = 0;

        if (channel->channel_output[RED].pwmindex > PWM_OFF)
        {
            channel->channel_output[RED].pwmindex--;
            channel->channel_output[RED].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[RED].pwmindex];
        }

        break;
    }

    case 0b0100: // Toggle forward/float on output B
    {
        channel->timeout_action = 0;

        if (channel->channel_output[BLUE].pwmindex == PWM_FLOAT)
        {
            channel->channel_output[BLUE].pwmindex = PWM_FULL;
            channel->channel_output[BLUE].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[BLUE].pwmindex];
            channel->channel_output[BLUE].output_mode = OM_FWD;
        }
        else
        {
            channel->channel_output[BLUE].pwmindex = PWM_FLOAT;
            channel->channel_output[BLUE].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[BLUE].pwmindex];
            channel->channel_output[BLUE].output_mode = OM_FLOAT;
        }

        break;
    }

    case 0b0110: // Toggle address bit.
    {
        uint8_t temp = channel->channel_number;

        if (temp & 0b100)
            temp = temp & 0b0011;
        else
            temp = temp | 0b100;

        OpenPfRx_channel_init(channel, temp); // Reset channel.
        break;
    }

    case 0b0111: // Align toggle bit; already done in function prologue.
        break;

    default:
        OpenPfRx_channel_init(channel, channel->channel_number); // Reset channel.
    }
}

void OpenPfRxComboDirectMode(const uint16_t *rxdata, volatile struct OpenPfRx_channel *channel)
{
    // Timeout is enabled.
    // Toggle bit is not verified.
    // *LEGO8879 uses this mode when both red buttons are pressed simultaneously
    uint8_t data = (*rxdata & OpenPfRx_DATA_MASK) >> 4;
    uint8_t dataRED = data & 0x03;
    uint8_t dataBLUE = (data >> 2) & 0x03;
    channel->timeout_action = 1;
    channel->channel_output[RED].pwmindex = OpenPfRxComboDirectModeLUT[dataRED][0];
    channel->channel_output[RED].output_mode = OpenPfRxComboDirectModeLUT[dataRED][1];
    channel->channel_output[RED].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[RED].pwmindex];
    channel->channel_output[BLUE].pwmindex = OpenPfRxComboDirectModeLUT[dataBLUE][0];
    channel->channel_output[BLUE].output_mode = OpenPfRxComboDirectModeLUT[dataBLUE][1];
    channel->channel_output[BLUE].pwmvalue = OpenPfRx_pwmvalues[channel->channel_output[BLUE].pwmindex];
}

void OpenPfRxSingleOutputMode(const uint16_t *rxdata, volatile struct OpenPfRx_channel *channel)
{
    // Protocol used by LEGO 8879 remote control (inc/dec pwm, break then float)
    // Toggle bit verified for increment / decrement / toggle
    // Timeout ONLY for full forward and full backward
    volatile struct OpenPfRx_output *target_output;

    if (*rxdata & OpenPfRx_SINGLEOUTPUT_OUTPUT_MASK)
        target_output = &(channel->channel_output[BLUE]);
    else
        target_output = &(channel->channel_output[RED]);

    uint8_t verify_toggle = OpenPfRxVerifyToggleBit(rxdata, channel); // take care that toggle bit remains synchronized, even if it isn't used.
    uint8_t DDDD = (*rxdata & OpenPfRx_SINGLEOUTPUT_DDDD_MASK) >> 4;

    if (*rxdata & OpenPfRx_SINGLEOUTPUT_CSTIDMODE_MASK) // Clear/Set/Toggle/Inc/Dec mode
    {
        if (DDDD == 0b0110 || DDDD == 0b0111) // Full Forward and Full Backward have timeout
            channel->timeout_action = 1;
        else // all other commands do not use timeout
            channel->timeout_action = 0;

        switch (DDDD)
        {

        case 0b0000: // Toggle Full Forward
        {
            // PF spec: toggle full forward (Stop <-> Fwd)
            if (!verify_toggle)
                return;

            switch (target_output->output_mode)
            {

            case (OM_FWD):
            {
                target_output->pwmindex = PWM_OFF;
                target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
                target_output->output_mode = OM_BRAKE;
                break;
            }

            default:
            {
                target_output->pwmindex = PWM_FULL;
                target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
                target_output->output_mode = OM_FWD;
            }
            }

            break;
        }

        case 0b0001: // Toggle Direction
        {
            if (!verify_toggle)
                return;

            switch (target_output->output_mode)
            {

            case (OM_FWD):
            {
                // do not change PWM value
                target_output->output_mode = OM_BWD;
                break;
            }

            case (OM_BWD):
            {
                // do not change PWM value
                target_output->output_mode = OM_FWD;
                break;
            }

            default:
                break; // do nothing if not driving, or independent.
            }

            break;
        }

        case 0b0010: // Increment Numerical PWM
        {
            if (!verify_toggle)
                return;

            if (target_output->pwmvalue < OpenPfRx_MAX_PWM_VALUE)
                target_output->pwmvalue++;

            break;
        }

        case 0b0011: // Decrement Numerical PWM
        {
            if (!verify_toggle)
                return;

            if (target_output->pwmvalue > OpenPfRx_MIN_PWM_VALUE)
                target_output->pwmvalue--;

            break;
        }

        case 0b0100: // Increment PWM *LEGO8879
        {
            if (!verify_toggle)
                return;

            if ((target_output->output_mode != OM_FWD) && (target_output->output_mode != OM_BWD)) // if floating, braking, etc.
            {
                target_output->output_mode = OM_FWD; // set output mode on 'running forward'
                target_output->pwmindex = PWM_OFF;   // set pwmindex to 'doing nothing'
            }

            if ((target_output->pwmindex < PWM_FULL) && target_output->output_mode == OM_FWD)
                target_output->pwmindex++; // increase speed forward

            if ((target_output->pwmindex > PWM_OFF) && target_output->output_mode == OM_BWD)
                target_output->pwmindex--; // decrease speed backward
            else if ((target_output->pwmindex == PWM_OFF) && target_output->output_mode == OM_BWD)
            {
                // switch direction
                target_output->pwmindex = PWM_STEP1;
                target_output->output_mode = OM_FWD;
            }

            target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex]; // update also if not changing pwmindex; align to PWM_step if previously 'numerical' value was changed.
            break;
        }

        case 0b0101: // Decrement PWM *LEGO8879
        {
            if (!verify_toggle)
                return;

            if ((target_output->output_mode != OM_FWD) && (target_output->output_mode != OM_BWD))
            {
                target_output->output_mode = OM_BWD;
                target_output->pwmindex = PWM_OFF;
            }

            if ((target_output->pwmindex < PWM_FULL) && target_output->output_mode == OM_BWD)
                target_output->pwmindex++; // increase speed backward

            if ((target_output->pwmindex > PWM_OFF) && target_output->output_mode == OM_FWD)
                target_output->pwmindex--; // decrease speed forward
            else if ((target_output->pwmindex == PWM_OFF) && target_output->output_mode == OM_FWD)
            {
                // switch direction
                target_output->pwmindex = PWM_STEP1;
                target_output->output_mode = OM_BWD;
            }

            target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
            break;
        }

        case 0b0110: // Full Forward (timeout)
        {
            target_output->pwmindex = PWM_FULL;
            target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
            target_output->output_mode = OM_FWD;
            break;
        }

        case 0b0111: // Full Backward (timeout)
        {
            target_output->pwmindex = PWM_FULL;
            target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
            target_output->output_mode = OM_BWD;
            break;
        }

        case 0b1000: // Toggle Full Forward / Backward (default Forward)
        {
            if (!verify_toggle)
                return;

            target_output->pwmindex = PWM_FULL;
            target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];

            if (target_output->output_mode == OM_FWD)
                target_output->output_mode = OM_BWD;
            else
                target_output->output_mode = OM_FWD;

            break;
        }

        case 0b1001: // Clear C1 (negative logic, C1 high)
        {
            // TODO: confirm how C2 should behave after FWD/BWD/BRAKE transitions.
            target_output->output_mode = OM_INDEPENDENT;
            target_output->C1 = 1;
            break;
        }

        case 0b1010: // Set C1 (negative logic, C1 low)
        {
            // TODO: confirm how C2 should behave after FWD/BWD/BRAKE transitions.
            target_output->output_mode = OM_INDEPENDENT;
            target_output->C1 = 0;
            break;
        }

        case 0b1011: // Toggle C1
        {
            if (!verify_toggle)
                return;

            if (target_output->C1)
                target_output->C1 = 0;
            else
                target_output->C1 = 1;

            target_output->output_mode = OM_INDEPENDENT;
            break;
        }

        case 0b1100: // Clear C2 (negative logic, C2 high)
        {
            // TODO: confirm how C1 should behave after FWD/BWD/BRAKE transitions.
            target_output->output_mode = OM_INDEPENDENT;
            target_output->C2 = 1;
            break;
        }

        case 0b1101: // Set C2 (negative logic, C2 low)
        {
            // TODO: confirm how C1 should behave after FWD/BWD/BRAKE transitions.
            target_output->output_mode = OM_INDEPENDENT;
            target_output->C2 = 0;
            break;
        }

        case 0b1110: // Toggle C2
        {
            if (!verify_toggle)
                return;

            if (target_output->C2)
                target_output->C2 = 0;
            else
                target_output->C2 = 1;

            target_output->output_mode = OM_INDEPENDENT;
            break;
        }

        case 0b1111: // Toggle Full Backward
        {
            if (!verify_toggle)
                return;

            switch (target_output->output_mode)
            {

            case (OM_BWD):
            {
                target_output->pwmindex = PWM_OFF;
                target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
                target_output->output_mode = OM_BRAKE;
                break;
            }

            default:
            {
                target_output->pwmindex = PWM_FULL;
                target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
                target_output->output_mode = OM_BWD;
            }
            }

            break;
        }

        default:
            return;
        }
    }
    else // PWM Mode
    {
        target_output->pwmindex = OpenPfRxOutputModePwmLUT[DDDD][0];
        target_output->output_mode = OpenPfRxOutputModePwmLUT[DDDD][1];
        target_output->pwmvalue = OpenPfRx_pwmvalues[target_output->pwmindex];
        channel->timeout_action = 0;
    }
}
