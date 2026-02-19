#include <stdint.h>
#include "isr.h"
#include "port.h"
#include "pic.h"
#include "console.h"
#include "pit.h"
#include "keyboard.h"
#include "panic.h"
#include "syscall.h"

volatile uint32_t g_ticks = 0;

static void print_hex32(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) {
        console_putc(hex[(v >> (i * 4)) & 0xF]);
    }
}

static const char* exception_name(uint32_t vec) {
    static const char* names[32] = {
        "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow", "BOUND", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present", "Stack-Segment Fault",
        "General Protection", "Page Fault", "Reserved", "x87 Floating-Point", "Alignment Check", "Machine Check",
        "SIMD Floating-Point", "Virtualization", "Control Protection", "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
    };
    if (vec < 32) return names[vec];
    return "Unknown";
}

void isr_default_handler_c(void) {
    console_puts("[irq] unhandled interrupt\n");
}

void isr_exception_handler_c(interrupt_frame_t* frame) {
    console_puts("\n[exc] vector=");
    print_hex32(frame->vector);
    console_puts(" (");
    console_puts(exception_name(frame->vector));
    console_puts(") err=");
    print_hex32(frame->error);
    console_puts(" eip=");
    print_hex32(frame->eip);
    console_putc('\n');

    if (frame->vector == 3) {
        return;
    }

    panic("fatal exception");
}

void irq_handler_c(interrupt_frame_t* frame) {
    uint32_t vec = frame->vector;
    if (vec == 0x20) {
        pit_irq_tick();
        pic_send_eoi(0);
        return;
    }

    if (vec == 0x21) {
        uint8_t sc = inb(0x60);
        keyboard_handler(sc);
        pic_send_eoi(1);
        return;
    }

    if (vec >= 0x20 && vec <= 0x2F) {
        pic_send_eoi((uint8_t)(vec - 0x20));
    }
}

void syscall_handler_c(interrupt_frame_t* frame) {
    uint32_t ret = syscall_dispatch(frame->eax, frame->ebx, frame->ecx, frame->edx);
    frame->eax = ret;
}
