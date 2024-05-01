#ifndef _WIFI_TCP_H
#define _WIFI_TCP_H

#ifdef __cplusplus
extern "C"{
#endif 
int wifi_connect(char* wifi_ssid, char* wifi_password);

void wifi_disconnect(void);

void send_tcp(char* tcp_server_ip, int tcp_server_port, char* data, int data_len);

void run_udp_beacon(char* udp_server_ip, int udp_server_port);

int send_udp(char* udp_server_ip, int udp_server_port, char* data);
#ifdef __cplusplus
}
#endif

#endif // _WIFI_TCP_H

