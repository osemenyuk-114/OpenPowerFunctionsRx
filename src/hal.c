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
#include "hal.h"

void SetupExternalInterrupt()
{
    // Init pin interrupt
    MCUCR = (MCUCR & 0xFC) | ExternalInterruptFalling; // Falling edge INT0 generates interrupt; Table 9-2
}

void Setup105usclock()
{
    // Timer0 CTC mode, overrun at OCR0A, CLK/8
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01); // CTC mode, TOP is OCR0A, CLK = CLKio/8
    TCNT0 = 0;
    OCR0A = 105; // 105us
#if defined(ATTINY84)
    TIMSK0 |= _BV(OCIE0A); // Output compare interrupt enabled
#else
    TIMSK |= _BV(OCIE0A); // Output compare interrupt enabled
#endif
}

void IoInit()
{
    // Board-specific pin direction and output setup
#if defined(ATTINY84)
    DDRA = 0;
    DDRB = 0;
    PORTA = 0x00;
    PORTB = 0xFF;
    A_PORT |= (A_C1 | A_C2 | B_C1 | B_C2);
    DDRA |= (A_C1 | A_C2 | B_C1 | B_C2);
    BICOLOR_LED_OUTPUTS;
#elif defined(ATTINY85)
#if (NumberOfOutputChannels == 1)
    DDRB = A_C1 | A_C2 | IR_POWER;
#else
    DDRB = A_C1 | A_C2 | B_C1 | B_C2;
#endif
    PORTB = 0;
#elif defined(ATTINY85_DUPLO_TRAIN)
#if (NumberOfOutputChannels == 1)
    DDRB = A_C1 | A_C2;
#else
    DDRB = A_C1 | A_C2 | B_C1 | B_C2;
#endif
    PORTB = 0;
#endif

    // Button pull-up setup (common for all boards)
#ifdef CHBUTTON
    BUTTON_DDR &= ~CHBUTTON;
    BUTTON_PORT |= CHBUTTON;
#endif
#ifdef STBUTTON
    BUTTON_DDR &= ~STBUTTON;
    BUTTON_PORT |= STBUTTON;
#endif
}

void SetupPWMTimer()
{
#if defined(ATTINY84)
    TCCR1A = 0;                                  // CTC
    TCCR1B = (_BV(WGM13) | _BV(WGM12) | PWMCLK); // IOclk/64
    TCCR1C = 0;
    ICR1 = 108; // 1.15kHz output frequency
    OCR1A = 20; // Value to turn off output A
    OCR1B = 20; // Value to turn off output B
    TCNT1 = 0;
    TIMSK1 = _BV(ICIE1);
#else
    TCCR1 = _BV(PWM1A) | PWMCLK; // PWM mode, IOclk/32
    OCR1C = 217;                 // 1.15kHz output frequency
    OCR1A = 0;                   // Value to turn off output A
#if (NumberOfOutputChannels == 2)
    GTCCR = _BV(PWM1B);
    OCR1B = 0; // Value to turn off output B
#endif
    TCNT1 = 0;
    TIMSK |= _BV(TOIE1);
#endif
}
