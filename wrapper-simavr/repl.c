/*
Copyright (C) 2024 rerobots, Inc.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_hex.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_uart.h>


void uart_output_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
	printf("{\"event\": \"UART\", \"value\": %d}\n", value);
}


int main(int argc, char **argv)
{
	avr_t *avr = NULL;
	uint32_t freq;
	uint8_t *boot = NULL;
	uint32_t boot_base, boot_size;
	uint32_t flags;
	avr_irq_t *uart_output_irq = NULL;

	if (argc != 4) {
		printf("Usage: %s MCU FREQ FILE\n", argv[0]);
		return 1;
	}
	freq = atoi(argv[2]);
	if (freq < 1) {
		printf("Frequency must be greater than 0\n");
		return 1;
	}

	avr = avr_make_mcu_by_name(argv[1]);
	if (!avr) {
		printf("Failed to make MCU\n");
		return 1;
	}

	boot = read_ihex_file(argv[3], &boot_size, &boot_base);

	avr_init(avr);
	avr->frequency = freq;
	memcpy(avr->flash + boot_base, boot, boot_size);
	free(boot);
	boot = NULL;
	avr->pc = boot_base;
	avr->codeend = avr->flashend;

	flags = 0;
	avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &flags);
	flags &= ~AVR_UART_FLAG_STDIO;
	avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &flags);

	uart_output_irq = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
	avr_irq_register_notify(uart_output_irq, uart_output_hook, NULL);

	while (1) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed) {
			break;
		}
	}

	return 0;
}
