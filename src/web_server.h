#pragma once
#include <stdbool.h>
#include <stdint.h>

void web_server_init(void);
void web_server_task(void);          // call often from the main loop
bool web_client_connected(void);
bool web_reboot_requested(void);
