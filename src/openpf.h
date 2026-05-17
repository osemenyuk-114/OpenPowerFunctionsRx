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

#pragma once

#include <avr/io.h>

#define OpenPfRx_CHANNEL_MASK 0x3000
#define OpenPfRx_ESCAPE_MASK 0x4000
#define OpenPfRx_MODE_MASK 0x0700
#define OpenPfRx_TOGGLE_MASK 0x8000
#define OpenPfRx_ADDRESS_MASK 0x0800
#define OpenPfRx_SINGLEOUTPUT_CSTIDMODE_MASK 0x0200
#define OpenPfRx_SINGLEOUTPUT_DDDD_MASK 0x00F0
#define OpenPfRx_SINGLEOUTPUT_OUTPUT_MASK 0x0100
#define OpenPfRx_OUTPUTA_MASK 0x00F0
#define OpenPfRx_OUTPUTB_MASK 0x0F00
#define OpenPfRx_EXTENDED_MODE 0
#define OpenPfRx_COMBO_DIRECT_MODE 1
#define OpenPfRx_RESERVED_1 2
#define OpenPfRx_RESERVED_2 3
#define OpenPfRx_SINGLE_OUTPUT_MODE 4
#define OpenPfRx_DATA_MASK 0x00F0

#if defined(ATTINY84)
#define OpenPfRx_MAX_PWM_VALUE 108
#else
#define OpenPfRx_MAX_PWM_VALUE 217
#endif
#define OpenPfRx_MIN_PWM_VALUE 0
#define OpenPfRx_PWM_VALUE_STEP (OpenPfRx_MAX_PWM_VALUE / 8)

enum OpenPfRx_RECEIVER_STATE
{
    IDLE,
    WAIT_FOR_START,
    WAIT_FOR_BIT,
    WAIT_FOR_STOP
};

enum OpenPfRx_IR_LENGTH_VALUES
{
    IR_LENGTH_INIT = 0,
    IR_LENGTH_LO = 3,
    IR_LENGTH_HI = 5,
    IR_LENGTH_START_STOP = 9,
    IR_LENGTH_TIMEOUT = 11
}; // Minimum number of 105 us periods for each value.

enum OpenPfRx_MODES
{
    EXTENDED_MODE,
    COMBO_DIRECT_MODE,
    RESERVED,
    SINGLE_OUTPUT_MODE,
    COMBO_PWM_MODE
};

enum OpenPfRx_PWM_INDEX
{
    PWM_OFF = 0,
    PWM_FLOAT = 0,
    PWM_BRAKE_THEN_FLOAT = 0,
    PWM_STEP1,
    PWM_STEP2,
    PWM_STEP3,
    PWM_STEP4,
    PWM_STEP5,
    PWM_STEP6,
    PWM_STEP7,
    PWM_FULL = 7
};

enum OpenPfRx_OUTPUT_MODES
{
    OM_FWD,
    OM_BWD,
    OM_FLOAT,
    OM_BRAKE,
    OM_BRAKE_THEN_FLOAT,
    OM_INDEPENDENT
};

enum OpenPfRx_Channel_Color
{
    RED,
    BLUE
};

struct OpenPfRx_receiver
{
    uint8_t bit_count;
    uint8_t state;
    uint8_t periodcounter; // Count each timer tick.
    volatile uint16_t rxdata;
    volatile uint8_t newdata;
};

struct OpenPfRx_output
{
    // Store both index and value: numerical PWM uses pwmvalue, stepped PWM uses pwmindex.
    uint8_t pwmvalue;
    enum OpenPfRx_PWM_INDEX pwmindex;
    enum OpenPfRx_OUTPUT_MODES output_mode;
    uint16_t brakethenfloatcount;
    int8_t C1;
    int8_t C2;
};

struct OpenPfRx_channel
{
    uint16_t timeout;
    uint16_t timeout_limit;
    uint8_t timeout_action;
    uint8_t channel_number;
    uint8_t toggle;
    struct OpenPfRx_output channel_output[2];
};

// Global instance
struct OpenPfRx_receiver OpenPfRx_rx;

// function declarations
void OpenPfRx_channel_init(volatile struct OpenPfRx_channel *, uint8_t);
uint8_t OpenPfRxVerifyChecksum(uint16_t);
void OpenPfRxPinInterruptState();
void OpenPfRxInterpreter(const uint16_t *, volatile struct OpenPfRx_channel *);
void OpenPfRxComboPWMMode(const uint16_t *, volatile struct OpenPfRx_channel *);
void OpenPfRxExtendedMode(const uint16_t *, volatile struct OpenPfRx_channel *);
void OpenPfRxComboDirectMode(const uint16_t *, volatile struct OpenPfRx_channel *);
void OpenPfRxSingleOutputMode(const uint16_t *, volatile struct OpenPfRx_channel *);
uint8_t OpenPfRxVerifyToggleBit(const uint16_t *, volatile struct OpenPfRx_channel *);
uint8_t OpenPfRxGetChannelNumber(uint16_t);

static inline void OpenPfRx105usState()
{
    if (OpenPfRx_rx.periodcounter > IR_LENGTH_TIMEOUT)
    {
        if (OpenPfRx_rx.state == WAIT_FOR_STOP) // stop bit successfully received; no IR during pause
            OpenPfRx_rx.newdata = 1;

        OpenPfRx_rx.state = IDLE;
    }

    OpenPfRx_rx.periodcounter++;
}
