#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"

#include "wifi.h"


int main() {
    stdio_init_all();

    wifi_connect(WIFI_SSID, WIFI_PASSWORD);

    run_tcp_client_test(SERVER_IP, atoi(SERVER_PORT));
    //sleep_ms(1000);

    run_udp_beacon(SERVER_IP, atoi(SERVER_PORT));

    wifi_disconnect();
    return 0;
}
