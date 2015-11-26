#define UART_BAUD_RATE 19200


#define PIN(x) (*(&x - 2)) // Address Of Data Direction Register Of Port x 
#define DDR(x) (*(&x - 1)) // Address Of Input Register Of Port x

/*
 *
 * PIN MAPPING:
 *  13	PB5			Display STROBE
 *  12	PB4			Display DATA
 *  11	PB3			Display CLOCK
 *  
 *   9  PB1 (OC1A)  Heater
 *
 *  A0  PC0 (ADC0)  Thermocouple
 * 
 */

#define DPY_PORT          PORTB
#define DPY_STROBE        PB5
#define DPY_DATA          PB4
#define DPY_CLOCK         PB3

#define HEATER_PORT       PORTB
#define HEATER            PB1

#define SEG_A             0x80
#define SEG_B             0x40
#define SEG_C             0x20
#define SEG_D             0x10
#define SEG_E             0x08
#define SEG_F             0x04
#define SEG_G             0x02

#define NUMBER_OF_DIGITS 3
