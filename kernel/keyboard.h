#pragma once
#include <stdint.h>

void keyboard_handler(uint8_t scancode);
int keyboard_getline(char* buffer, int maxlen);