/*
 * potmeter_led_pwm.c
 *
 * Created: 2017-06-12 11:43:18
 * Author : Krisztian Stancz
 */ 

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <string.h>
#include "pwm_timer.h"
#include "ADC_driver.h"
#include "UART_driver.h"


#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <util/delay.h>




#include <avr/io.h>

void init_led(void)
{
	// Set LED as output
	DDR_LED |= 1 << DDR_LED_PIN;
	
	return;
}



int main(void)
{
	// Init functions
	init_led();
	init_fast_pwm();
	ADC_Init();
	UART_Init();
	PIN_PORT_LED |= 1 << PIN_LED;
	
	// Setting up STDIO input and output buffer
	// You don't have to understand this!
	//----- START OF STDIO IO BUFFER SETUP
	FILE UART_output = FDEV_SETUP_STREAM(UART_SendCharacter, NULL, _FDEV_SETUP_WRITE);
	stdout = &UART_output;
	FILE UART_input = FDEV_SETUP_STREAM(NULL, UART_GetCharacter, _FDEV_SETUP_READ);
	stdin = &UART_input;
	//----- END OF STDIO IO BUFFER SETUP

	// Try printf
	printf("Startup...\r\n");

	uint8_t read_data;
	
	while(1) {
		read_data = ADC_Read() >> 2;
		set_duty_cycle_a(read_data);
		printf("%d\r\n", read_data);

	}

	return 0;
}


