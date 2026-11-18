#pragma once
#include <stdbool.h>

typedef struct {
    char ssid[33];
    char pass[65];
} settings_t;

extern settings_t g_settings;

void settings_load(void);                 // loads saved or default settings
bool settings_save(const char *ssid, const char *pass);
bool settings_reset(void);                // erase -> defaults on next boot
void settings_default_ssid(char *out, int len);
#define SETTINGS_DEFAULT_PASS "picokeys"
