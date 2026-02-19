#pragma once
#include <stdint.h>

typedef struct interrupt_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} interrupt_frame_t;

void isr_exception_handler_c(interrupt_frame_t* frame);
void irq_handler_c(interrupt_frame_t* frame);
void isr_default_handler_c(void);
void syscall_handler_c(interrupt_frame_t* frame);
