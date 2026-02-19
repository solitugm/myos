#include "idt.h"

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idtp;

extern void idt_load(uint32_t idt_ptr_addr);
extern void isr_default_stub(void);
extern void irq0_timer_stub(void);
extern void irq1_keyboard_stub(void);
extern void syscall_stub(void);
extern uint32_t isr_stub_table[];

static void idt_set_gate(uint8_t vec, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[vec].base_low = handler & 0xFFFF;
    idt[vec].sel = sel;
    idt[vec].always0 = 0;
    idt[vec].flags = flags;
    idt[vec].base_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    idtp.limit = sizeof(idt_entry_t) * 256 - 1;
    idtp.base = (uint32_t)&idt[0];

    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)isr_default_stub, 0x08, 0x8E);
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E);
    }

    idt_set_gate(0x20, (uint32_t)irq0_timer_stub, 0x08, 0x8E);
    idt_set_gate(0x21, (uint32_t)irq1_keyboard_stub, 0x08, 0x8E);

    // Ring3 callable syscall gate
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);

    idt_load((uint32_t)&idtp);
}
