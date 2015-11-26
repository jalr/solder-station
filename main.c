#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include <util/delay.h>
#include <stdlib.h>

#include "main.h"
#include "uart.h"

#define AREF_VOLTAGE 1.1

#define THERMOCOUPLE_FACTOR 0.31

#define Ta 100
#define Kp 10000
#define Ki 10
#define Kd 100000

//#define DISABLE_HEATER

static const uint8_t PROGMEM sevensegment_table[] = {
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // 0
	SEG_B | SEG_C, // 1
	SEG_A | SEG_B | SEG_D | SEG_E | SEG_G, // 2
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_G, // 3
	SEG_B | SEG_C | SEG_F | SEG_G, // 4
	SEG_A | SEG_C | SEG_D | SEG_F | SEG_G, // 5
	SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, // 6
	SEG_A | SEG_B | SEG_C | SEG_F, // 7
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, // 8
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G // 9
};

volatile uint16_t adc0 = 0;

void displayNumber(uint16_t *number) {
	// calculate digits
	uint16_t multiplier;
	uint8_t digit[NUMBER_OF_DIGITS];
	for (uint8_t digitCounter=NUMBER_OF_DIGITS; digitCounter>0; digitCounter--) {
		switch (digitCounter-1) {
			case 3:
				multiplier = 1000;
				break;
			case 2:
				multiplier = 100;
				break;
			case 1:
				multiplier = 10;
				break;
			case 0:
				multiplier = 1;
				break;
		}
		digit[digitCounter-1] = 0;
		while (*number >= multiplier) {
			digit[digitCounter-1]++;
			*number -= multiplier;
		}
	}
	
	// send digits to display
	#if (DIGIT_ORDER == 1)
		for (uint8_t segmentPos=0; segmentPos<NUMBER_OF_DIGITS; segmentPos++) {
	#else
		for (uint8_t segmentPos=NUMBER_OF_DIGITS; segmentPos>0; segmentPos--) {
	#endif
		uint8_t segments = pgm_read_byte(&sevensegment_table[digit[segmentPos-1]]);
		for (uint8_t segmentBit=0; segmentBit<8; segmentBit++) {
			if (segments & (1<<segmentBit)) {
				DPY_PORT |= (1<<DPY_DATA);	
			} else {
				DPY_PORT &= ~(1<<DPY_DATA);
			}
			_delay_us(2);
			DPY_PORT |= (1<<DPY_CLOCK);
			_delay_us(2);
			DPY_PORT &= ~(1<<DPY_CLOCK);
		}
	}
	DPY_PORT &= ~(1<<DPY_STROBE);
	_delay_us(2);
	DPY_PORT |= (1<<DPY_STROBE);
	_delay_us(2);
}


ISR(ADC_vect) {
	adc0 = ADCW;
}

ISR(TIMER1_OVF_vect) {
	if (OCR1A > 50) HEATER_PORT |= (1<<HEATER);
}


ISR(TIMER1_COMPB_vect) {
}


ISR(TIMER1_COMPA_vect) {
	HEATER_PORT &= ~(1<<HEATER);	
}

int main(void) {
	uint16_t w = 300;			// Führungsgröße (Sollwert) w
	int16_t esum = 0;			// für I-Anteil
	int16_t ealt = 0;			// für D-Anteil
	uint8_t loop = 0;
	uart_init( UART_BAUD_SELECT_DOUBLE_SPEED(UART_BAUD_RATE,F_CPU) );
	DDR(DPY_PORT) |= (1<<DPY_STROBE) | (1<<DPY_DATA) | (1<<DPY_CLOCK);
	#ifndef DISABLE_HEATER
		DDR(HEATER_PORT) |= (1<<HEATER);
	#endif
	OCR1B = (2^16) - 25*64; // ADC needs 25 cycles for the first conversion; ADC prescaler 64
							// following conversions take 13 ADC clock cycles
	OCR1A = 0;

	DIDR0 = (1<<ADC0D);					// Disable ADC input buffer
	ADMUX = (1<<REFS1) | (1<<REFS0);	// Internal 1.1V Voltage Reference with external capacitor at AREF pin
	ADCSRA |= (1<<ADPS2) | (1<<ADPS1);	// Prescaler: 8 MHz / 64 = 125 kHz
	ADCSRA |= (1<<ADIE);				// ADC Interrupt Enable
	ADCSRB = (1<<ADTS2) | (1<<ADTS0);	// ADC Auto Trigger Source on Timer/Counter1 Compare Match B
	ADCSRA |= (1<<ADATE);				// ADC Auto Trigger Enable
	ADCSRA |= (1<<ADEN);				// ADC Enable
	ADCSRA |= (1<<ADSC);				// start conversion


	TCNT1 = 0;
	//TCCR1A = (1<<COM1A1);  //Clear OC1A on Compare Match
	TCCR1B = (1<<CS10); // Prescaler: 1
	TIMSK1 = (1<<TOIE1);
	TIMSK1 |= (1 << OCIE1A);
	TIMSK1 |= (1 << OCIE1B);
	uart_puts_P("SOLDER STATION\n");
	uart_puts_P("ready\n");
	_delay_ms(1000);

	sei();
	for(;;) {
		// http://rn-wissen.de/wiki/index.php/Regelungstechnik
		uint16_t adc0_local;

		float x;				// Regelgröße (Istwert) x
		int16_t e;				// Regelabweichung e
		int32_t y;				// Stellgröße y
		int32_t yP;				// Stellgröße y
		int32_t yI;				// Stellgröße y
		int32_t yD;				// Stellgröße y

		uint16_t x_int;
		char c[10];
		cli();
		adc0_local = adc0;
		sei();
		/*uart_puts_P("ADC: ");
		itoa(adc0_local,c,10);
		uart_puts_P("\n");
		uart_puts(c);*/
		x = adc0_local;
		x *= THERMOCOUPLE_FACTOR;
		x_int = x;

		e = w - x_int;
		esum = esum + e;
		yP = Kp;
		yP *= e;
		yI = Ki * Ta;
		yI *= esum;
		yD = (e - ealt);
		yD *= Kd/Ta;
		y = yP;
		y += yI;
		y += yD;
		ealt = e;

		//uart_puts_P("Temperature: ");
		itoa(x_int,c,10);
		uart_puts(c);
		uart_puts_P("\n");

		if (esum > 200)
			esum = 200;
		if (esum < -100)
			esum = -100;
	
		if (y > 63000)
			y = 63000;
		if (y < 0)
			y = 0;

		OCR1A = y;

		if (loop++ > 4) {
			displayNumber(&x_int);
			loop = 0;
		}
		_delay_ms(Ta);
	}
}
