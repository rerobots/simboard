/*
To build,

	avr-gcc -mmcu=atmega328p -o hola.elf hola.c
	avr-objcopy -O ihex hola.elf hola.hex

To learn more about stdio support for AVR, read
https://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html
*/

#include <stdio.h>
#include <avr/io.h>
#include <avr/sleep.h>

#define F_CPU (10000000UL)
#include <util/delay.h>


static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n') {
		uart_putchar('\r', stream);
	}
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE ustdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);


int main()
{
	uint8_t k;

	stdout = &ustdout;

	printf("¡Hola, mundo!\n");

	DDRB = 0xFF;

	for (k = 0; k < 20; k++) {
		PORTB = k;
		_delay_ms(500);
	}

	sleep_cpu();
	return 0;
}
