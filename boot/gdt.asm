BITS 32
GLOBAL gdt_flush

gdt_flush:
    mov eax, [esp + 4]     ; pointer to gdt descriptor
    lgdt [eax]

    mov ax, 0x10           ; data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush_cs     ; far jump to reload CS
.flush_cs:
    ret