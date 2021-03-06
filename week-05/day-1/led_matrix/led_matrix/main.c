/*
 * led_matrix.c
 *
 * Created: 2017-05-27 20:56:49
 * Author : Krisztian Stancz
 */ 

// GTM2088RGB - 8x8 common cathode (-)

// SN74HC595N
// 14 SER - PC2 - DATA_PIN - yellow
// 12 RCLK - PC1 - LATCH_PIN - green
// 11 SRCLK - PC0 - CLOCK_PIN - orange

// LED matrix pin layout
// 1-8 BLUE
// 9-16 RED
// 28-21 GREEN
// 17-20 ROW 1-4
// 29-32 ROW 5-8
// LED (1,1): upper left
// bit order MSB-> rows-R-G-B <-LSB

// timer: pulse every 3.75 sec

// button on PB7
// led on pb5



#ifndef F_CPU
#define F_CPU 16000000
#endif // F_CPU

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define SHIFT_REG_PORT	PORTC
#define DDR_SHIFT_REG	DDRC
#define DATA_PIN	PORTC2
#define LATCH_PIN	PORTC1
#define CLOCK_PIN	PORTC0

volatile uint8_t state = 255;
volatile uint8_t enabled = 0;

ISR(PCINT0_vect)
{
	// Start/reset on PB7 button press
	if (PINB & (1 << PINB7)) {
		switch (state) {
		case 255:	// start timer
			enabled = 1;	
			state = 0;
			TCNT1 = 0; // initialize timer counter
			break;
		default:	// reset timer
			enabled = 0;
			state = 255;
			break;
		}	
	}
}

ISR(TIMER1_COMPA_vect)
{
	if (enabled) {
	
		// Increase state at every timer compare
		state++;
	
		// Turn off timer after 63
		if (state > 63)
			enabled = 0;
	}
}


uint8_t timer_init(void)
{
	// Set prescaler to 1024
	TCCR1B |= (1<<CS10) | (1<<CS12);
	
	// Set CTC mode
	TCCR1B |= 1 << WGM12;
	
	// Initialize counter
	TCNT1 = 0;
	
	// Set compare value for 3.75 sec at 16 MHz / 1024
	OCR1A = 58593;
	
	// Enable compare interrupt
	TIMSK1 |= 1 << OCIE1A;

	// Enable global interrupts
	sei();
	
	return 0;
}



uint8_t button_init(void)
{
	// Enable interrupts on PCINT7 (PB7)
	PCMSK0 |= 1 << PCINT7;

	// Enable interrupts on pin group 0 (PCINT0-7)
	PCICR |=  1 << PCIE0;

	// Enable global interrupts
	sei();
	
	return 0;
}

uint8_t shift_port_init(void)
{
	// Set shift register port to output
	DDR_SHIFT_REG |= (1 << DATA_PIN);
	DDR_SHIFT_REG |= (1 << LATCH_PIN);
	DDR_SHIFT_REG |= (1 << CLOCK_PIN);
	
	// Set shift register pins to low
	SHIFT_REG_PORT &= ~(1 << DATA_PIN);
	SHIFT_REG_PORT &= ~(1 << LATCH_PIN);
	SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);
	
	return 0;
}

uint8_t shift_out(uint8_t *bytes, uint8_t size)
{
	
	// Set latch low while serial shift goes in
	SHIFT_REG_PORT &= ~(1 << LATCH_PIN);
	
	// For start, set clock to low
	SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);

	// Send out one bit for each rising clock pulse
	// Set data bit
	for (uint8_t j = 0; j < size; j++) {
		for (uint8_t i = 0; i <= 7; i++) {
			if (bytes[j] & (1 << i)) {
				SHIFT_REG_PORT |= 1 << DATA_PIN;
			} else {
				SHIFT_REG_PORT &= ~(1 << DATA_PIN);
			}
		
			// Set clock pulse high
			SHIFT_REG_PORT |= 1 << CLOCK_PIN;
	
			// Set clock pulse low
			SHIFT_REG_PORT &= ~(1 << CLOCK_PIN);
		}
	}
	
	// Set latch high to parallel write
	SHIFT_REG_PORT |= 1 << LATCH_PIN;
	
	return 0;
}

uint8_t show_leds(uint8_t *bytes)
{
	// Negate RGB bits for common cathode configuration
	for (uint8_t i = 1; i <= 3; i++) {
		bytes[i] = ~bytes[i];
	}
	
	// Shift out all bytes
	shift_out(bytes, 4);
	
	return 0;
}


int main(void)
{
    shift_port_init();
	button_init();
	timer_init();
			
	uint8_t leds[4];
	uint8_t red_left[] = {0x1, 0x3, 0x7, 0xf, 0x8, 0xc, 0xe, 0xf};
	uint8_t red_right[] = {0x8, 0xc, 0xe, 0xf, 0x1, 0x3, 0x7, 0xf};
	uint8_t actual_row;
	uint8_t last_red;
	
	while (1) {
		
		// After reset, all blue
		if (state == 255) {
			leds[0] = 0xff;
			leds[1] = 0x00;
			leds[2] = 0x00;
			leds[3] = 0xff;
			show_leds(leds);

		// Overtime, flashing red
		} else if (state > 63) {
			leds[0] = 0xff;
			leds[1] = 0xff;
			leds[2] = 0x00;
			leds[3] = 0x00;
			show_leds(leds);
			_delay_ms(500);
			leds[0] = 0x00;
			leds[1] = 0x00;
			leds[2] = 0x00;
			leds[3] = 0x00;
			show_leds(leds);
			_delay_ms(500);

		// Timer in operation
		} else {

			// Calculate actual row
			actual_row = (int) state / 4;
			if (actual_row > 7) 
				actual_row -= 8;
			
			// Calculate last red led position in actual row
			last_red = state % 8;
			
			// Turn on leds row-by-row
			for (uint8_t j = 0; j < 8; j++) {

				// Row's position
				// Until state 31 go downwards then upwards
				if (state < 32)
					leds[0] = 1 << (7 - j);
				else
					leds[0] = 1 << j;
			
				// Reds in row
				// Until state 31 use the left side then the right
				if (state < 32)	{
					if (j == actual_row)
						leds[1] = red_left[last_red] << 4;
					else if (j < actual_row)
						leds[1] = red_left[3] << 4;
					else if (j > actual_row)
						leds[1] = 0x00;
				} else {
					if (j == actual_row)
						leds[1] = red_right[last_red] | 0xf0;
					else if (j < actual_row)
						leds[1] = red_right[3] | 0xf0;
					else if (j > actual_row)
						leds[1] = 0xf0;
				}
				
				// Greens in row
				leds[2] = ~leds[1];

				// No Blues
				leds[3] = 0x00;

				// Turn on leds
				show_leds(leds);		
			}
		}
	}
	return 0;
}

