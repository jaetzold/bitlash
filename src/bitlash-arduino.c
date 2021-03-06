/***
	bitlash-arduino.c: A minimal implementation of certain core Arduino functions	
	
	The author can be reached at: bill@bitlash.net

	Copyright (C) 2008-2012 Bill Roy

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:
	
	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

***/
#include "bitlash.h"

#ifndef ARDUINO_BUILD

#ifdef TINY85
#define PINMASK 7
#endif

// Lite versions of arduino digital io
//
void pinMode(byte pin, byte mode) {
#ifdef TINY85
	//byte mask = _BV(pin & PINMASK);
	byte mask = _BV(pin);
	if (mode) DDRB |= mask;
	else DDRB &= ~mask;
#else
	volatile uint8_t *reg;
	if (pin < 8) reg = &DDRD;
	else if (pin < 16) { reg = &DDRB; pin -= 8; }
	else if (pin < 24) { reg = &DDRC; pin -= 16; }

#if NUMPINS > 24
	// Enable port f and e for the atmega32u8 and friends
	else if (pin < 32) { reg = &DDRF; pin -= 24; }
	else if (pin < 40) { reg = &DDRE; pin -= 32; }
#endif

	else return;
	if (mode) *reg |= (1 << pin);
	else *reg &= ~(1 << pin);
#endif
}

int digitalRead(byte pin) {
#ifdef TINY85
	//return ((PINB & (1 << (pin & PINMASK))) != 0);
	return ((PINB & _BV(pin)) != 0);
#else
	volatile uint8_t *reg;
	if (pin < 8) reg = &PIND;
	else if (pin < 16) { reg = &PINB; pin -= 8; }
	else if (pin < 24) { reg = &PINC; pin -= 16; }
	else return -1;
	return (*reg & (1 << pin)) != 0;
#endif
}

void digitalWrite(byte pin, byte value) {
#ifdef TINY85
	//byte mask = _BV(pin & PINMASK);
	byte mask = _BV(pin);
	if (value) PORTB |= mask;
	else PORTB &= ~mask;
#else
	volatile uint8_t *reg;
	if (pin < 8) reg = &PORTD;
	else if (pin < 16) { reg = &PORTB; pin -= 8; }
	else if (pin < 24) { reg = &PORTC; pin -= 16; }
	else return;
	if (value) *reg |= (1 << pin);
	else *reg &= ~(1 << pin);
#endif
}



// From Arduino 0012::wiring.c
volatile unsigned long timer0_clock_cycles = 0;
volatile unsigned long timer0_millis = 0;

ISR(SIG_OVERFLOW0) {
	// timer 0 prescale factor is 64 and the timer overflows at 256
	timer0_clock_cycles += 64UL * 256UL;
	while (timer0_clock_cycles > clockCyclesPerMicrosecond() * 1000UL) {
		timer0_clock_cycles -= clockCyclesPerMicrosecond() * 1000UL;
		timer0_millis++;
	}
}


unsigned long millis(void) {
unsigned long m;

	// Stir some Timer1 randomness into the entropy pool on every call here
	stir(TCNT1);

	// The USB stack is very sensitive to interrupt latency
	// disable just the timer interrupt here so that the USB interrupt can still happen
	TIMSK &= ~(1 << TOIE0);	// disable overflow interrupt
	m = timer0_millis;		// read the timer
	TIMSK |= (1 << TOIE0);	// re-enable overflow interrupt
	return m;
}


void delay(unsigned long ms) {
	unsigned long start = millis();
	while (millis() - start < ms) { 
		//wdt_reset();
		usbPoll(); 
	}
}


/* Delay for the given number of microseconds.  Assumes a 8 or 16 MHz clock. 
 * Disables interrupts, which will disrupt the millis() function if used
 * too frequently. */
void delayMicroseconds(unsigned int us)
{
	uint8_t oldSREG;

	// calling avrlib's delay_us() function with low values (e.g. 1 or
	// 2 microseconds) gives delays longer than desired.
	//delay_us(us);

#if F_CPU >= 16000000L
	// for the 16 MHz clock on most Arduino boards

	// for a one-microsecond delay, simply return.  the overhead
	// of the function call yields a delay of approximately 1 1/8 us.
	if (--us == 0)
		return;

	// the following loop takes a quarter of a microsecond (4 cycles)
	// per iteration, so execute it four times for each microsecond of
	// delay requested.
	us <<= 2;

	// account for the time taken in the preceeding commands.
	us -= 2;
#else
	// for the 8 MHz internal clock on the ATmega168

	// for a one- or two-microsecond delay, simply return.  the overhead of
	// the function calls takes more than two microseconds.  can't just
	// subtract two, since us is unsigned; we'd overflow.
	if (--us == 0)
		return;
	if (--us == 0)
		return;

	// the following loop takes half of a microsecond (4 cycles)
	// per iteration, so execute it twice for each microsecond of
	// delay requested.
	us <<= 1;
    
	// partially compensate for the time taken by the preceeding commands.
	// we can't subtract any more than this or we'd overflow w/ small delays.
	us--;
#endif

	// disable interrupts, otherwise the timer 0 overflow interrupt that
	// tracks milliseconds will make us delay longer than we want.
	oldSREG = SREG;
	cli();

	// busy wait
	__asm__ __volatile__ (
		"1: sbiw %0,1" "\n\t" // 2 cycles
		"brne 1b" : "=w" (us) : "0" (us) // 2 cycles
	);

	// reenable interrupts.
	SREG = oldSREG;
}



int analogRead(uint8_t pin) {
	// place our order
	ADMUX = pin;

	// start the conversion
	ADCSRA |= _BV(ADSC);

	// ADSC is cleared when the conversion finishes
	// TODO: usbPoll() here?  supposedly 1600 cycles to complete
	while (ADCSRA & _BV(ADSC)) { ; }
	return ADC;
}

#endif
