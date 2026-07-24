#include <stdio.h>

#define AF_INET 2
void udp_server_task();
void udp_txvi(void *pvParameters);
int charger_build_status(char *buf, int len);