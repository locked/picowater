/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "wifi.h"

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "lwip/udp.h"

#define BEACON_MSG_LEN_MAX 127
#define BEACON_INTERVAL_MS 1000


#define DEBUG_printf printf
#define BUF_SIZE 2048

#define TEST_ITERATIONS 10
#define POLL_TIME_S 5

#if 0
static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    printf("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        }
        printf("%02x ", bptr[i++]);
    }
    printf("\n");
}
#define DUMP_BYTES dump_bytes
#else
#define DUMP_BYTES(A,B)
#endif

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;

static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("[wifi] close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

// Called with results of operation
static err_t tcp_result(void *arg, int status) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    state->complete = true;
    return tcp_client_close(arg);
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("[wifi] tcp_client_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE) {
        state->run_count++;

        tcp_result(arg, 0);
        return ERR_OK;

        // We should receive a new buffer from the server
        //state->buffer_len = 0;
        //state->sent_len = 0;
        //DEBUG_printf("Waiting for buffer from server\n");
    }

    //tcp_result(arg, 0);
    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("[wifi] connect failed %d\n", err);
        return tcp_result(arg, err);
    }
    state->connected = true;
    DEBUG_printf("[wifi] waiting for buffer from server\n");
    return ERR_OK;
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    DEBUG_printf("[wifi] tcp_client_poll\n");
    return tcp_result(arg, -1); // no response is an error?
}

static void tcp_client_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        DEBUG_printf("[wifi] tcp_client_err %d\n", err);
        tcp_result(arg, err);
    }
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (!p) {
        return tcp_result(arg, -1);
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("[wifi] recv %d err %d\n", p->tot_len, err);
        DEBUG_printf("[wifi] buffer %s\n", p->payload, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            DUMP_BYTES(q->payload, q->len);
        }
        // Receive the buffer
        const uint16_t buffer_left = BUF_SIZE - state->buffer_len;
        state->buffer_len += pbuf_copy_partial(p, state->buffer + state->buffer_len,
                                               p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

	//tcp_result(state, 0);

    return ERR_OK;
}

static bool tcp_client_open(void *arg, int tcp_server_port) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("[wifi] Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), tcp_server_port);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb) {
        DEBUG_printf("[wifi] failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, tcp_client_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    state->buffer_len = 0;

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, tcp_server_port, tcp_client_connected);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

// Perform initialisation
static TCP_CLIENT_T* tcp_client_init(char* tcp_server_ip) {
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        DEBUG_printf("[wifi] failed to allocate state\n");
        return NULL;
    }
    ip4addr_aton(tcp_server_ip, &state->remote_addr);
    return state;
}

int send_tcp(char* tcp_server_ip, int tcp_server_port, char* data, int data_len, char* response) {
    TCP_CLIENT_T *state = tcp_client_init(tcp_server_ip);
    if (!state) {
        return -1;
    }
    if (!tcp_client_open(state, tcp_server_port)) {
        tcp_result(state, -1);
        return -1;
    }

	DEBUG_printf("[wifi] sending [%s] to server\n", data);
	err_t err = tcp_write(state->tcp_pcb, data, data_len, TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		DEBUG_printf("[wifi] failed to write data %d\n", err);
		tcp_result(state, -1);
		return -1;
	}

    while (!state->complete) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }

    strcpy(response, (unsigned char *)&(state->buffer));

    free(state);
    
    return 0;
}


int wifi_connect(char* wifi_ssid, char* wifi_password) {
    if (cyw43_arch_init()) {
        printf("[wifi] failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    printf("[wifi] connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_password, CYW43_AUTH_WPA2_AES_PSK, 6000)) {
        printf("[wifi] failed to connect.\n");
        return 1;
    }

    printf("[wifi] Connected.\n");
    return 0;
}

void wifi_disconnect(void) {
    cyw43_arch_deinit();
}
