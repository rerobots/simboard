/*
Copyright (C) 2024 rerobots, Inc.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_hex.h>


int main(int argc, char **argv)
{
	avr_t *avr = NULL;
	uint32_t freq;
	uint8_t *boot = NULL;
	uint32_t boot_base, boot_size;

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

	while (1) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed) {
			break;
		}
	}

	return 0;
}
