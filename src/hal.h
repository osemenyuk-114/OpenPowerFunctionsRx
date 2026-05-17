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

// ============================================================================
// Board & feature configuration
// ============================================================================
// ATTINY84             : ATtiny84 board (2-channel, 16-bit Timer1, PORTA outputs)
// ATTINY85             : ATtiny85 board (1-channel, PB0/PB1, IR_POWER, WDT sleep)
#if !defined(ATTINY84) && !defined(ATTINY85)
#define ATTINY85
#endif

// ============================================================================
// Common definitions
// ============================================================================
#define ExternalInterruptFalling 0b10
// Clear timer and restart counting.
#define RESET_IR_TIMER TCNT0 = 0
#define DISABLE_IR_INT GIMSK &= ~(_BV(INT0))
#define ENABLE_IR_INT GIMSK |= _BV(INT0)

// ============================================================================
// Board 84: ATtiny84
// ============================================================================
#if defined(ATTINY84)

#define NumberOfOutputChannels 2 // Should always be 2 for ATtiny84 because it has enough pins.
#define StartButtonEnabled
typedef uint16_t pwm_reg_t;

#define PWMCLK (_BV(CS11) | _BV(CS10))
#define TIMER_105US TIM0_COMPA_vect
#define EXTERNAL_INTERRUPT EXT_INT0_vect
#define PWMTIMER_PERIODSTART TIM1_CAPT_vect
#define PWMTIMER_PWMA_INTERRUPT TIM1_COMPA_vect
#define ENABLE_TIMER_105US_INTERRUPT TIMSK0 |= _BV(OCIE0A)
#define DISABLE_PWMA_INTERRUPT TIMSK1 &= ~(_BV(OCIE1A))
#define ENABLE_PWMA_INTERRUPT TIMSK1 |= _BV(OCIE1A)
#define WDT_CTRL WDTCSR
#define WDT_VECT WATCHDOG_vect

#if (NumberOfOutputChannels == 2)
#define PWMTIMER_PWMB_INTERRUPT TIM1_COMPB_vect
#define DISABLE_PWMB_INTERRUPT TIMSK1 &= ~(_BV(OCIE1B))
#define ENABLE_PWMB_INTERRUPT TIMSK1 |= _BV(OCIE1B)
#endif

#define A_PORT PORTA
#define A_DDR DDRA
#define A_C1 _BV(PORTA0)
#define A_C2 _BV(PORTA1)

#if (NumberOfOutputChannels == 2)
#define B_C1 _BV(PORTA2)
#define B_C2 _BV(PORTA3)
#endif

#ifdef StartButtonEnabled
#define IR_POWER _BV(PORTA7)
#define ENABLE_IR_POWER A_PORT |= IR_POWER
#define DISABLE_IR_POWER A_PORT &= ~IR_POWER
#endif

#define BUTTON_PORT PORTB
#define BUTTON_PIN PINB
#define BUTTON_DDR DDRB
#define CHBUTTON _BV(PINB2)

#ifdef StartButtonEnabled
#define STBUTTON _BV(PINB1)
#define PCINT_MASK PCMSK1
#define STBUTTON_INTERRUPT PCINT1_vect
#define DISABLE_STBUTTON_INTERRUPT GIMSK &= ~(_BV(PCIE1))
#define ENABLE_STBUTTON_INTERRUPT GIMSK |= _BV(PCIE1)
#endif

// ============================================================================
// Board 85: ATtiny85
// ============================================================================
#elif defined(ATTINY85)

#define NumberOfOutputChannels 1
#define StartButtonEnabled
typedef uint8_t pwm_reg_t;

#define PWMCLK (_BV(CS12) | _BV(CS11))
#define TIMER_105US TIMER0_COMPA_vect
#define EXTERNAL_INTERRUPT INT0_vect
#define PWMTIMER_PERIODSTART TIMER1_OVF_vect
#define PWMTIMER_PWMA_INTERRUPT TIMER1_COMPA_vect
#define ENABLE_TIMER_105US_INTERRUPT TIMSK |= _BV(OCIE0A)
#define DISABLE_PWMA_INTERRUPT TIMSK &= ~(_BV(OCIE1A))
#define ENABLE_PWMA_INTERRUPT TIMSK |= _BV(OCIE1A)
#define WDT_CTRL WDTCR
#define WDT_VECT WDT_vect

#if (NumberOfOutputChannels == 2)
#define PWMTIMER_PWMB_INTERRUPT TIMER1_COMPB_vect
#define DISABLE_PWMB_INTERRUPT TIMSK &= ~(_BV(OCIE1B))
#define ENABLE_PWMB_INTERRUPT TIMSK |= _BV(OCIE1B)
#endif

#define A_PORT PORTB
#define A_DDR DDRB
#define A_C1 _BV(PORTB0)
#define A_C2 _BV(PORTB1)

#if (NumberOfOutputChannels == 2)
#define B_C1 _BV(PORTB3)
#define B_C2 _BV(PORTB4)
#endif

#if defined(StartButtonEnabled) && (NumberOfOutputChannels == 1)
#define IR_POWER _BV(PORTB4)
#define ENABLE_IR_POWER A_PORT |= IR_POWER
#define DISABLE_IR_POWER A_PORT &= ~IR_POWER
#endif

#define BUTTON_PORT PORTB
#define BUTTON_PIN PINB
#define BUTTON_DDR DDRB
#define CHBUTTON _BV(PINB2)

#ifdef StartButtonEnabled
#if (NumberOfOutputChannels == 2)
#define STBUTTON _BV(PINB5)
#else
#define STBUTTON _BV(PINB3)
#endif
#define PCINT_MASK PCMSK
#define STBUTTON_INTERRUPT PCINT0_vect
#define DISABLE_STBUTTON_INTERRUPT GIMSK &= ~(_BV(PCIE))
#define ENABLE_STBUTTON_INTERRUPT GIMSK |= _BV(PCIE)
#endif

#else
#error "Define one of: ATTINY84, ATTINY85"
#endif

#ifdef CHBUTTON
#define CHBUTTON_PUSHED ((~BUTTON_PIN) & CHBUTTON)
#endif

#ifdef STBUTTON
#define STBUTTON_PUSHED ((~BUTTON_PIN) & STBUTTON)
#endif

// ============================================================================
// Function declarations
// ============================================================================
void SetupExternalInterrupt();
void Setup105usclock(); // 105 us clock tick
void IoInit();
void SetupPWMTimer();
