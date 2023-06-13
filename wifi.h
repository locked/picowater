#ifndef _WIFI_TCP_H
#define _WIFI_TCP_H

#ifdef __cplusplus
extern "C"{
#endif 
int wifi_connect(char* wifi_ssid, char* wifi_password);

void wifi_disconnect(void);

void run_tcp_client_test(char* tcp_server_ip, int tcp_server_port);

void run_udp_beacon(char* udp_server_ip, int udp_server_port);
#ifdef __cplusplus
}
#endif

#endif // _WIFI_TCP_H

