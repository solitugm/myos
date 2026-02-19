#include <stdint.h>
#include "keyboard.h"
#include "console.h"

#define BUF_SIZE 128
#define KEYQ_SIZE 64

static char input_buf[BUF_SIZE];
static int input_len = 0;
static int line_ready = 0;

static char keyq[KEYQ_SIZE];
static int keyq_r = 0;
static int keyq_w = 0;

static const char keymap[128] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
    [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
    [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
    [0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x34]='.',
    [0x39]=' ',
    [0x1C]='\n',
    [0x0E]='\b'
};

static void keyq_push(char c) {
    int next = (keyq_w + 1) % KEYQ_SIZE;
    if (next == keyq_r) return;
    keyq[keyq_w] = c;
    keyq_w = next;
}

int keyboard_read_char(void) {
    if (keyq_r == keyq_w) return -1;
    char c = keyq[keyq_r];
    keyq_r = (keyq_r + 1) % KEYQ_SIZE;
    return (int)(unsigned char)c;
}

void keyboard_handler(uint8_t sc) {
    if (sc & 0x80) return;

    char ch = keymap[sc];
    if (!ch) return;

    keyq_push(ch);

    if (ch == '\n') {
        console_putc('\n');
        input_buf[input_len] = 0;
        line_ready = 1;
        return;
    }

    if (ch == '\b') {
        if (input_len > 0) {
            input_len--;
            console_backspace();
        }
        return;
    }

    if (input_len < BUF_SIZE - 1) {
        input_buf[input_len++] = ch;
        console_putc(ch);
    }
}

int keyboard_getline(char* buffer, int maxlen) {
    if (!line_ready) return 0;

    int n = input_len;
    if (n > maxlen - 1) n = maxlen - 1;

    for (int i = 0; i < n; i++) buffer[i] = input_buf[i];
    buffer[n] = 0;

    input_len = 0;
    line_ready = 0;
    return 1;
}
