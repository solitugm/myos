BITS 32
GLOBAL idt_load
GLOBAL isr_default_stub
GLOBAL irq1_keyboard_stub
GLOBAL irq0_timer_stub

EXTERN isr_default_handler_c
EXTERN irq1_keyboard_handler_c
EXTERN irq0_timer_handler_c

; lidt wrapper
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; 기본 ISR
isr_default_stub:
    pusha
    call isr_default_handler_c
    popa
    iretd

; IRQ1 키보드
irq1_keyboard_stub:
    pusha
    call irq1_keyboard_handler_c
    popa
    iretd

irq0_timer_stub:
    pusha
    call irq0_timer_handler_c
    popa
    iretd