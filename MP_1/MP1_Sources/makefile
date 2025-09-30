AS=nasm
PLATFORM=$(shell uname)
$(info $(PLATFORM))

ifeq ($(PLATFORM), Linux)
GCC=x86_64-linux-gnu-gcc
LD=x86_64-linux-gnu-ld
endif

ifeq ($(PLATFORM), Darwin)
GCC=x86_64-elf-gcc
LD=x86_64-elf-ld
endif

GCC_OPTIONS = -m32 -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -fno-exceptions -fno-rtti -fno-stack-protector -fleading-underscore -fno-asynchronous-unwind-tables -fno-pie

all: kernel.bin

clean:
	rm -f *.o *.bin

run:
	qemu-system-x86_64 -kernel kernel.bin -serial stdio
	
debug:
	qemu-system-x86_64 -s -S -kernel kernel.bin
	
# ==== KERNEL ENTRY POINT ====

start.o: start.asm
	$(AS) -f elf -o start.o start.asm

# ==== UTILITIES ====

utils.o: utils.H utils.C
	$(GCC) $(GCC_OPTIONS) -c -o utils.o utils.C

# ==== DEVICES ====

simple_console.o: simple_console.H simple_console.C
	$(GCC) $(GCC_OPTIONS) -c -o simple_console.o simple_console.C

# ==== KERNEL MAIN FILE ====

kernel.o: kernel.C
	$(GCC) $(GCC_OPTIONS) -c -o kernel.o kernel.C

kernel.bin: start.o kernel.o simple_console.o utils.o  linker.ld
	$(LD) -melf_i386 -T linker.ld -o kernel.bin start.o kernel.o simple_console.o utils.o

