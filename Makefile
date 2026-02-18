ARCH=i386
CC=gcc
AS=nasm

CFLAGS=-m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS=-m elf_i386 -T linker.ld -nostdlib

BUILD=build
ISO=myos.iso

KERNEL_BIN=$(BUILD)/kernel.bin
OBJS=$(BUILD)/boot.o $(BUILD)/kernel.o

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.o: kernel/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) linker.ld
	ld $(LDFLAGS) -o $@ $(OBJS)

$(ISO): $(KERNEL_BIN)
	rm -rf $(BUILD)/isodir
	mkdir -p $(BUILD)/isodir/boot/grub
	cp $(KERNEL_BIN) $(BUILD)/isodir/boot/kernel.bin
	cp iso/boot/grub/grub.cfg $(BUILD)/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(BUILD)/isodir >/dev/null 2>&1

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 256M

clean:
	rm -rf $(BUILD) $(ISO)

.PHONY: all run clean