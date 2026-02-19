#pragma once
#include <stdint.h>

void console_clear(void);
void console_putc(char c);
void console_puts(const char* s);
void console_backspace(void);

void console_set_cursor(uint16_t row, uint16_t col);
void console_enable_cursor(uint8_t start, uint8_t end);
