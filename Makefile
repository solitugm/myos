ARCH=i386
CC=gcc
AS=nasm

CFLAGS=-m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS=-m elf_i386 -T linker.ld -nostdlib

BUILD=build
ISO=myos.iso
DISK_IMG=$(BUILD)/disk.img

KERNEL_BIN=$(BUILD)/kernel.bin
OBJS=$(BUILD)/boot.o $(BUILD)/isr.o $(BUILD)/gdt_asm.o \
	$(BUILD)/kernel.o $(BUILD)/idt.o $(BUILD)/pic.o $(BUILD)/gdt.o $(BUILD)/isr_c.o \
	$(BUILD)/console.o $(BUILD)/pit.o $(BUILD)/keyboard.o $(BUILD)/shell.o \
	$(BUILD)/pmm.o $(BUILD)/mb2.o $(BUILD)/kheap.o $(BUILD)/panic.o \
	$(BUILD)/ata.o $(BUILD)/fs.o $(BUILD)/exec.o $(BUILD)/syscall.o

all: $(ISO)

disk: $(DISK_IMG)

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

$(DISK_IMG): scripts/create_disk_image.sh disk/HELLO.TXT disk/HELLO.BIN | $(BUILD)
	./scripts/create_disk_image.sh $(DISK_IMG)

$(BUILD)/isr.o: boot/isr.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/idt.o: kernel/idt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pic.o: kernel/pic.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/isr_c.o: kernel/isr.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gdt_asm.o: boot/gdt.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/gdt.o: kernel/gdt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/console.o: kernel/console.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pit.o: kernel/pit.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: kernel/keyboard.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: kernel/shell.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: kernel/pmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mb2.o: kernel/mb2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kheap.o: kernel/kheap.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/panic.o: kernel/panic.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ata.o: kernel/ata.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fs.o: kernel/fs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exec.o: kernel/exec.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: kernel/syscall.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(ISO) $(DISK_IMG)
	qemu-system-i386 -boot order=d -drive file=$(DISK_IMG),format=raw,if=ide,index=0 -cdrom $(ISO) -m 256M -no-reboot -no-shutdown

run-headless: $(ISO) $(DISK_IMG)
	qemu-system-i386 -boot order=d -drive file=$(DISK_IMG),format=raw,if=ide,index=0 -cdrom $(ISO) -m 256M -no-reboot -no-shutdown -display none

clean:
	rm -rf $(BUILD) $(ISO)

.PHONY: all disk run run-headless clean
