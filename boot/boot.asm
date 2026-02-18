; Multiboot2 header + entry point
BITS 32
GLOBAL _start
EXTERN kmain

SECTION .multiboot
align 8
mb2_header_start:
    dd 0xE85250D6          ; multiboot2 magic
    dd 0                   ; architecture (0 = i386)
    dd mb2_header_end - mb2_header_start
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))

    ; end tag
    dw 0
    dw 0
    dd 8
mb2_header_end:

SECTION .text
_start:
    ; basic stack
    mov esp, stack_top

    call kmain

.hang:
    cli
    hlt
    jmp .hang

SECTION .bss
align 16
stack_bottom:
    resb 16384
stack_top: