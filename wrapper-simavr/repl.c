/*
Copyright (C) 2024 rerobots, Inc.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libwebsockets.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_hex.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_uart.h>


#define MAX_SIM_EVENT_LEN 128


typedef struct event_queue_t {
	char *event;
	struct event_queue_t *next;
} event_queue_t;


event_queue_t *eventq = NULL;
pthread_mutex_t eventq_mutex;


void event_queue_push(event_queue_t **eq, char *event)
{
	pthread_mutex_lock(&eventq_mutex);
	if (*eq == NULL) {
		*eq = malloc(sizeof(event_queue_t));
		(*eq)->next = NULL;
		(*eq)->event = event;
	} else {
		event_queue_t *target = *eq;
		while (target->next != NULL) {
			target = target->next;
		}
		target->next = malloc(sizeof(event_queue_t));
		target = target->next;
		target->next = NULL;
		target->event = event;
	}
	pthread_mutex_unlock(&eventq_mutex);
}


char *event_queue_pop(event_queue_t **eq)
{
	pthread_mutex_lock(&eventq_mutex);
	char *event;
	if (*eq == NULL) {
		pthread_mutex_unlock(&eventq_mutex);
		return NULL;
	}
	event = (*eq)->event;
	event_queue_t *old = *eq;
	old = *eq;
	*eq = (*eq)->next;
	free(old);
	pthread_mutex_unlock(&eventq_mutex);
	return event;
}


int event_queue_len(event_queue_t *eq)
{
	pthread_mutex_lock(&eventq_mutex);
	int len = 0;
	while (eq) {
		len++;
		eq = eq->next;
	}
	pthread_mutex_unlock(&eventq_mutex);
	return len;
}


void uart_output_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
	char *buf = malloc(sizeof(char)*MAX_SIM_EVENT_LEN);
	snprintf(buf, MAX_SIM_EVENT_LEN, "{\"event\": \"UART\", \"value\": %u}", value);
	event_queue_push(&eventq, buf);
}

void portB_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
	char *buf = malloc(sizeof(char)*MAX_SIM_EVENT_LEN);
	snprintf(buf, MAX_SIM_EVENT_LEN, "{\"event\": \"PORTB\", \"value\": %u}", value);
	event_queue_push(&eventq, buf);
}


static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	char *sim_event;
	int n;
	char buf[LWS_PRE + MAX_SIM_EVENT_LEN];

	switch (reason) {
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			fprintf(stderr, "LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			fprintf(stderr, "LWS_CALLBACK_CLIENT_ESTABLISHED\n");
			lws_callback_on_writable(wsi);
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
			fprintf(stderr, "LWS_CALLBACK_CLIENT_RECEIVE\n");
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			/* TODO: something better than busy-loop */
			sim_event = event_queue_pop(&eventq);
			if (sim_event) {
				fprintf(stderr, "sending...\n");
				snprintf(&buf[LWS_PRE], MAX_SIM_EVENT_LEN, "%s", sim_event);
				free(sim_event);
				sim_event = NULL;
				n = lws_write(wsi, &buf[LWS_PRE], strnlen(&buf[LWS_PRE], MAX_SIM_EVENT_LEN), LWS_WRITE_TEXT);
			}
			lws_callback_on_writable(wsi);
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			fprintf(stderr, "LWS_CALLBACK_CLIENT_CLOSED\n");
			break;

		default:
			fprintf(stderr, "warning: unhandled WebSocket callback: %d\n", reason);
			break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols ws_protocols[] = {
	{
		"simboard",
		ws_callback,
		0, 0, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};


void *sim_main(void *avr)
{
	int state = -1;
	while (1) {
		state = avr_run((avr_t *) avr);
		if (state == cpu_Done || state == cpu_Crashed) {
			break;
		}
	}
	char *buf = malloc(sizeof(char)*MAX_SIM_EVENT_LEN);
	if (state == cpu_Done) {
		snprintf(buf, MAX_SIM_EVENT_LEN, "{\"event\": \"SIM\", \"value\": \"DONE\"}");
	} else {  /* state == cpu_Crashed */
		snprintf(buf, MAX_SIM_EVENT_LEN, "{\"event\": \"SIM\", \"value\": \"CRASH\"}");
	}
	event_queue_push(&eventq, buf);
	return NULL;
}


void *lws_main(void *context)
{
	int n = 0;
	while (n >= 0) {
		n = lws_service((struct lws_context *) context, 0);
	}
	return NULL;
}


int main(int argc, char **argv)
{
	avr_t *avr = NULL;
	uint32_t freq;
	uint8_t *boot = NULL;
	uint32_t boot_base, boot_size;
	uint32_t flags;
	avr_irq_t *uart_output_irq = NULL;
	avr_irq_t *portB_irq = NULL;
	char *url = NULL;
	char *token_path = NULL;
	pthread_t sim_main_thread;
	pthread_t lws_thread;
	int errno;

	struct lws_context_creation_info ws_creation_info;
	struct lws_client_connect_info ws_connect_info;
	struct lws_context *ws_context;

	if (argc != 4 && argc != 6) {
		fprintf(stderr, "Usage: %s MCU FREQ FILE [URL TOKEN]\n", argv[0]);
		return 1;
	}
	freq = atoi(argv[2]);
	if (freq < 1) {
		fprintf(stderr, "Frequency must be greater than 0\n");
		return 1;
	}

	avr = avr_make_mcu_by_name(argv[1]);
	if (!avr) {
		fprintf(stderr, "Failed to make MCU\n");
		return 1;
	}

	boot = read_ihex_file(argv[3], &boot_size, &boot_base);

	if (argc > 4) {
		url = argv[4];
		token_path = argv[5];
	}

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

	portB_irq = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN_ALL);
	avr_irq_register_notify(portB_irq, portB_hook, NULL);

	if (pthread_mutex_init(&eventq_mutex, NULL)) {
		fprintf(stderr, "error creating mutex for event queue\n");
		return 1;
	}

	memset(&ws_creation_info, 0, sizeof(ws_creation_info));
	ws_creation_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	ws_creation_info.port = CONTEXT_PORT_NO_LISTEN;
	ws_creation_info.protocols = ws_protocols;
	ws_creation_info.timeout_secs = 10;
	ws_creation_info.connect_timeout_secs = 30;
	ws_creation_info.fd_limit_per_thread = 1 + 1 + 1;
	ws_context = lws_create_context(&ws_creation_info);
	if (!ws_context) {
		fprintf(stderr, "error: failed to create LWS context\n");
		return 1;
	}
	memset(&ws_connect_info, 0, sizeof(ws_connect_info));
	ws_connect_info.context = ws_context;
	ws_connect_info.host = ws_connect_info.address;
	ws_connect_info.origin = ws_connect_info.address;
	ws_connect_info.protocol = ws_protocols[0].name;

	lws_client_connect_via_info(&ws_connect_info);

	fprintf(stderr, "starting lws thread\n");
	errno = pthread_create(&lws_thread, NULL, lws_main, ws_context);
	if (errno) {
		fprintf(stderr, "error: failed to create WebSocket thread\n");
		return 1;
	}

	fprintf(stderr, "starting sim thread\n");
	errno = pthread_create(&sim_main_thread, NULL, sim_main, avr);
	if (errno) {
		fprintf(stderr, "error: failed to create main sim thread\n");
		return 1;
	}

	fprintf(stderr, "joining sim thread\n");
	if (pthread_join(sim_main_thread, NULL)) {
		fprintf(stderr, "error: failed to join main sim thread\n");
		return 1;
	}

	fprintf(stderr, "joining lws thread\n");
	if (pthread_join(lws_thread, NULL)) {
		fprintf(stderr, "error: failed to join main sim thread\n");
		return 1;
	}
	lws_context_destroy(ws_context);

	return 0;
}
