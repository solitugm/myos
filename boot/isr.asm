BITS 32
GLOBAL idt_load
GLOBAL isr_default_stub
GLOBAL irq1_keyboard_stub
GLOBAL irq0_timer_stub
GLOBAL isr_stub_table
GLOBAL syscall_stub

EXTERN isr_default_handler_c
EXTERN isr_exception_handler_c
EXTERN irq_handler_c
EXTERN syscall_handler_c

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

%macro ISR_NOERR 1
isr%1_stub:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1_stub:
    push dword %1
    jmp isr_common
%endmacro

isr_common:
    pusha
    push esp
    call isr_exception_handler_c
    add esp, 4
    popa
    add esp, 8
    iretd

irq_common:
    pusha
    push esp
    call irq_handler_c
    add esp, 4
    popa
    add esp, 8
    iretd

syscall_common:
    pusha
    push esp
    call syscall_handler_c
    add esp, 4
    popa
    add esp, 8
    iretd

isr_default_stub:
    pusha
    call isr_default_handler_c
    popa
    iretd

irq0_timer_stub:
    push dword 0
    push dword 32
    jmp irq_common

irq1_keyboard_stub:
    push dword 0
    push dword 33
    jmp irq_common

syscall_stub:
    push dword 0
    push dword 128
    jmp syscall_common

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

SECTION .rodata
isr_stub_table:
    dd isr0_stub, isr1_stub, isr2_stub, isr3_stub
    dd isr4_stub, isr5_stub, isr6_stub, isr7_stub
    dd isr8_stub, isr9_stub, isr10_stub, isr11_stub
    dd isr12_stub, isr13_stub, isr14_stub, isr15_stub
    dd isr16_stub, isr17_stub, isr18_stub, isr19_stub
    dd isr20_stub, isr21_stub, isr22_stub, isr23_stub
    dd isr24_stub, isr25_stub, isr26_stub, isr27_stub
    dd isr28_stub, isr29_stub, isr30_stub, isr31_stub
