# How I Built a Custom x86 Operating System Kernel and Interactive Shell from Scratch

**Author:** Rohan  
**Project:** Rohnux OS (v0.2)  
**Category:** Systems Programming / OS Development  

---

## Introduction

Many software engineers spend their entire careers operating behind layers of abstraction—writing high-level code that relies on runtimes, garbage collectors, standard libraries, and pre-packaged operating system APIs. While modern toolchains provide speed and productivity, they obscure a fundamental question: **What actually happens when a computer turns on and loads an executable into memory?**

Driven by curiosity and inspired by the early days of personal computing, I set out to build **Rohnux**: a lightweight 32-bit x86 operating system built entirely from scratch in C and Assembly. 

In this article, I will explain how Rohnux works, how it transitions from bare metal to C code, and how I implemented an interactive shell with an in-memory file system.

---

## 1. The Boot Sequence: Crossing the Bare-Metal Bridge

When an x86 PC powers on, the system hardware initializes in **Real Mode**—a legacy 16-bit state. Modern kernels must transition to **32-bit Protected Mode** to access linear memory and hardware control registers.

Rather than writing a full bootloader from scratch, Rohnux relies on the **Multiboot Specification**, allowing it to be loaded by standard bootloaders such as **GRUB**.

### The Assembly Entry Point (`src/boot.s`)

Because C requires an initialized stack pointer (`ESP`) to handle function calls, local variables, and parameters, our kernel's entry point must be written in assembly:

```assembly
/* Declare constants for the Multiboot header */
.set ALIGN,    1<<0             /* Align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* Provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* Multiboot flag field */
.set MAGIC,    0x1BADB002       /* Magic number letting bootloader find header */
.set CHECKSUM, -(MAGIC + FLAGS) /* Checksum required by Multiboot specification */

/* Multiboot Header */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Reserve 16 KiB Stack for the C Kernel */
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

/* Kernel Entry Point */
.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %esp
	call kernel_main

	cli
1:	hlt
	jmp 1b
```

### What Happens Here?
1. **Multiboot Signature:** GRUB scans the compiled binary for the `0x1BADB002` magic number. If verified, it confirms the binary is a valid kernel.
2. **Stack Allocation:** We reserve 16 KiB in uninitialized memory (`.bss`) and point `%esp` to `stack_top`.
3. **Execution Handoff:** Assembly transfers control to `kernel_main()`, our main entry point written in C.

---

## 2. Memory Layout: The Linker Script (`src/linker.ld`)

To ensure GRUB puts code and data where the kernel expects them, we use a custom linker script:

```ld
ENTRY(_start)

SECTIONS
{
	/* Begin placing sections at 1 MB (standard for x86 kernels) */
	. = 1M;

	.text BLOCK(4K) : ALIGN(4K)
	{
		KEEP(*(.multiboot))
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
}
```

The linker script places our kernel at physical address `0x00100000` (1 MiB), leaving low memory intact for hardware and system structures. It guarantees that the `.multiboot` section sits at the beginning of the file.

---

## 3. Writing the Bare-Metal C Kernel (`src/kernel.c`)

In bare-metal C programming, standard C library functions (`printf`, `malloc`, `strcmp`) are unavailable. Everything must be implemented manually.

### Direct VGA Hardware Access
To print characters to the display, Rohnux writes 16-bit units (8-bit ASCII character + 8-bit color code) directly to the **VGA Text Buffer** located at address `0xB8000`:

```c
static volatile uint16_t *const VGA_MEMORY = (volatile uint16_t *)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    VGA_MEMORY[index] = ((uint16_t)color << 8) | c;
}
```

### PS/2 Keyboard Driver & Polling Loop
To capture keystrokes without an active Interrupt Descriptor Table (IDT), Rohnux uses hardware port polling. The kernel checks status port `0x64` to verify if data is available, then reads raw scan codes from data port `0x60`:

```c
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

char keyboard_read_char(void) {
    while (1) {
        if (inb(0x64) & 1) { // Check if data buffer is full
            uint8_t scancode = inb(0x60);
            if (!(scancode & 0x80)) { // Key-press event (ignore release)
                char c = get_ascii_char(scancode);
                if (c != 0) return c;
            }
        }
    }
}
```

---

## 4. The Rohnux Interactive Shell & RAM File System

Once raw character inputs and screen outputs were functional, I developed a lightweight command interpreter and an in-memory Virtual File System (RAM FS).

### Built-in System Commands
Rohnux currently supports the following standard commands:
* `help` — Prints the system help menu.
* `about` — Displays OS version metadata.
* `clear` — Wipes the VGA video buffer.
* `ls` — Lists active files and directories.
* `mkdir <name>` — Allocates a dynamic directory entry in memory.
* `touch <name>` — Creates a dynamic file entry in memory.

### Shell Processing Loop

```c
void kernel_main(void) {
    terminal_initialize();
    init_fs();

    print_string("=========================\n");
    print_string("      ROHNUX v0.2        \n");
    print_string("=========================\n");
    print_string("Type 'help' for commands.\n\nrohnux> ");

    char cmd_buffer[256];
    int buf_idx = 0;

    while (1) {
        char c = keyboard_read_char();

        if (c == '\n') {
            cmd_buffer[buf_idx] = '\0';
            execute_command(cmd_buffer);
            buf_idx = 0;
            print_string("\nrohnux> ");
        } else if (c == '\b') { // Handle Backspace
            if (buf_idx > 0) {
                buf_idx--;
                terminal_backspace();
            }
        } else if (c >= 32 && c <= 126) { // Printable Characters
            if (buf_idx < 255) {
                cmd_buffer[buf_idx++] = c;
                char str[2] = {c, '\0'};
                print_string(str);
            }
        }
    }
}
```

---

## 5. Building and Booting the ISO Image

To test Rohnux on bare metal or inside a hypervisor, the build pipeline compiles all object files, links them into a Multiboot binary, and generates a bootable ISO using **GRUB**:

```bash
# 1. Compile assembly entry point and kernel source
gcc -m32 -ffreestanding -c src/boot.s -o build/boot.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -c src/kernel.c -o build/kernel.o

# 2. Link objects into binary executable
ld -m elf_i386 -T src/linker.ld -o build/rohnux.bin build/boot.o build/kernel.o

# 3. Create ISO image with GRUB rescue tools
cp build/rohnux.bin iso/boot/rohnux.bin
grub-mkrescue -o build/rohnux.iso iso

# 4. Boot inside QEMU emulator
qemu-system-i386 -cdrom build/rohnux.iso
```

---

## Conclusion and Future Roadmap

Building **Rohnux v0.2** provided deep insights into hardware initialization, video buffers, port I/O, and low-level execution models. Moving from zero lines of code to a bootable, self-contained OS with an interactive shell highlights the power of systems programming.

### What's Next for Rohnux v0.3?
1. **Interrupt Descriptor Table (IDT):** Replacing hardware polling with real asynchronous IRQ hardware interrupts.
2. **Memory Management:** Implementing a physical page allocator and kernel heap (`kmalloc`/`kfree`).
3. **ATA Hard Disk Driver:** Reading and writing persistent data to storage devices rather than holding files in RAM.

---