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
// ATTINY84             : ATtiny84 board (2ch, 16-bit Timer1, PORTA outputs)
// ATTINY85             : ATtiny85 board (1ch, PB0/PB1, IR_POWER, WDT sleep)
// ATTINY85_DUPLO_TRAIN : ATtiny85 DuploTrain board (1ch, PB3/PB4, PWM restart)
#if !defined(ATTINY84) && !defined(ATTINY85) && !defined(ATTINY85_DUPLO_TRAIN)
#define ATTINY85_DUPLO_TRAIN
#endif

// ============================================================================
// Common definitions
// ============================================================================
#define ExternalInterruptFalling 0b10
// Clear Timer, restart counting
#define RESET_IR_TIMER TCNT0 = 0
#define DISABLE_IR_INT GIMSK &= ~(_BV(INT0))
#define ENABLE_IR_INT GIMSK |= _BV(INT0)

// ============================================================================
// Board 84: ATtiny84
// ============================================================================
#if defined(ATTINY84)

#define NumberOfOutputChannels 2
#define ChannelButtonEnabled
typedef uint16_t pwm_reg_t;

#define PWMCLK (_BV(CS11) | _BV(CS10))
#define START_PWM_TIMER TCCR1B |= PWMCLK
#define RESET_PWM_TIMER TCNT1 = 1;
#define STOP_PWM_TIMER TCCR1B &= 0xF8;
#define TIMER_105US TIM0_COMPA_vect
#define EXTERNAL_INTERRUPT EXT_INT0_vect
#define PWMTIMER_PERIODSTART TIM1_CAPT_vect
#define PWMTIMER_PWMA_INTERRUPT TIM1_COMPA_vect
#define PWMTIMER_PWMB_INTERRUPT TIM1_COMPB_vect
#define DISABLE_PWMA_INTERRUPT TIMSK1 &= ~(_BV(OCIE1A))
#define ENABLE_PWMA_INTERRUPT TIMSK1 |= _BV(OCIE1A)
#define DISABLE_PWMB_INTERRUPT TIMSK1 &= ~(_BV(OCIE1B))
#define ENABLE_PWMB_INTERRUPT TIMSK1 |= _BV(OCIE1B)
#define WDT_VECT WATCHDOG_vect

#define A_PORT PORTA
#define A_C1 _BV(PINA0)
#define A_C2 _BV(PINA1)
#define B_C1 _BV(PINA2)
#define B_C2 _BV(PINA3)
#define BUTTON_PORT PORTA
#define BUTTON_PIN PINA
#define BUTTON_DDR DDRA
#define CHBUTTON _BV(PINA7)
#define CHBUTTON_PUSHED ((~BUTTON_PIN) & CHBUTTON)
#define BICOLOR_LED_OUTPUTS DDRB |= 0x3;
#define BICOLOR_RED PORTB = (PORTB | 0x03) & ~0x01
#define BICOLOR_GREEN PORTB = (PORTB | 0x03) & ~0x02

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
#define DISABLE_PWMA_INTERRUPT TIMSK &= ~(_BV(OCIE1A))
#define ENABLE_PWMA_INTERRUPT TIMSK |= _BV(OCIE1A)
#define WDT_VECT WDT_vect

#if (NumberOfOutputChannels == 2)
#define PWMTIMER_PWMB_INTERRUPT TIMER1_COMPB_vect
#define DISABLE_PWMB_INTERRUPT TIMSK &= ~(_BV(OCIE1B))
#define ENABLE_PWMB_INTERRUPT TIMSK |= _BV(OCIE1B)
#endif

#define A_PORT PORTB
#define A_C1 _BV(PINB0)
#define A_C2 _BV(PINB1)

#if (NumberOfOutputChannels == 2)
#define B_C1 _BV(PINB3)
#define B_C2 _BV(PINB4)
#else
#define IR_POWER _BV(PINB4)
#define ENABLE_IR_POWER A_PORT |= IR_POWER
#define DISABLE_IR_POWER A_PORT &= ~IR_POWER
#endif

#define BUTTON_PORT PORTB
#define BUTTON_PIN PINB
#define BUTTON_DDR DDRB
#define CHBUTTON _BV(PINB2)
#define CHBUTTON_PUSHED ((~BUTTON_PIN) & CHBUTTON)

#ifdef StartButtonEnabled
#if (NumberOfOutputChannels == 2)
#define STBUTTON _BV(PINB5)
#else
#define STBUTTON _BV(PINB3)
#endif
#define STBUTTON_PUSHED ((~BUTTON_PIN) & STBUTTON)
#define STBUTTON_INTERRUPT PCINT0_vect
#define DISABLE_STBUTTON_INTERRUPT GIMSK &= ~(_BV(PCIE))
#define ENABLE_STBUTTON_INTERRUPT GIMSK |= _BV(PCIE)
#endif

// ============================================================================
// Board 85 DuploTrain: ATtiny85 (DuploTrain)
// ============================================================================
#elif defined(ATTINY85_DUPLO_TRAIN)

#define NumberOfOutputChannels 1
// #define StartButtonEnabled
#define ChannelButtonEnabled
typedef uint8_t pwm_reg_t;

#define PWMCLK (_BV(CS12) | _BV(CS11))
#define START_PWM_TIMER TCCR1 |= PWMCLK
#define RESET_PWM_TIMER TCNT1 = 1;
#define STOP_PWM_TIMER TCCR1 &= 0xF0;
#define TIMER_105US TIMER0_COMPA_vect
#define EXTERNAL_INTERRUPT INT0_vect
#define PWMTIMER_PERIODSTART TIMER1_OVF_vect
#define PWMTIMER_PWMA_INTERRUPT TIMER1_COMPA_vect
#define DISABLE_PWMA_INTERRUPT TIMSK &= ~(_BV(OCIE1A))
#define ENABLE_PWMA_INTERRUPT TIMSK |= _BV(OCIE1A)
#define WDT_VECT WDT_vect

#if (NumberOfOutputChannels == 2)
#define PWMTIMER_PWMB_INTERRUPT TIMER1_COMPB_vect
#define DISABLE_PWMB_INTERRUPT TIMSK &= ~(_BV(OCIE1B))
#define ENABLE_PWMB_INTERRUPT TIMSK |= _BV(OCIE1B)
#endif

#define A_PORT PORTB
#define A_C1 _BV(PINB3)
#define A_C2 _BV(PINB4)

#if (NumberOfOutputChannels == 2)
#define B_C1 _BV(PINB0)
#define B_C2 _BV(PINB1)
#endif

#if defined(ChannelButtonEnabled) || defined(StartButtonEnabled)
#define BUTTON_PORT PORTB
#define BUTTON_PIN PINB
#define BUTTON_DDR DDRB
#endif

#ifdef ChannelButtonEnabled
#define CHBUTTON 0x01 // 0x20 with RSTDISBL=0
#define CHBUTTON_PUSHED ((~BUTTON_PIN) & CHBUTTON)
#endif

#ifdef StartButtonEnabled
#define STBUTTON 0x02
#define STBUTTON_PUSHED ((~BUTTON_PIN) & STBUTTON)
#endif

#else
#error "Define one of: ATTINY84, ATTINY85, ATTINY85_DUPLO_TRAIN"
#endif

// ============================================================================
// Function declarations
// ============================================================================
void SetupExternalInterrupt();
void Setup105usclock(); // 105us clock
void IoInit();
void SetupPWMTimer();
