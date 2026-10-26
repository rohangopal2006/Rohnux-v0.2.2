#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* x86 Port I/O for Keyboard */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* VGA text mode definitions */
static volatile uint16_t *const VGA_MEMORY = (volatile uint16_t *)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = 0x0F; // White on black

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = ((uint16_t)terminal_color << 8) | ' ';
        }
    }
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    VGA_MEMORY[index] = ((uint16_t)color << 8) | c;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = VGA_HEIGHT - 1;
        }
        return;
    }
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = VGA_HEIGHT - 1;
        }
    }
}

void terminal_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    }
}

void print_string(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void clear_screen(void) {
    terminal_initialize();
}

/* Keyboard Scancode to ASCII Mapping (Set 1) */
char get_ascii_char(uint8_t scancode) {
    char map[58] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    if (scancode < 58) {
        return map[scancode];
    }
    return 0;
}

char keyboard_read_char(void) {
    while (1) {
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (!(scancode & 0x80)) { // Ignore key releases
                char c = get_ascii_char(scancode);
                if (c != 0) {
                    return c;
                }
            }
        }
    }
}

/* String Utilities */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/* In-Memory File System (RAM FS) */
#define MAX_ITEMS 32
#define NAME_LEN 32

typedef struct {
    char name[NAME_LEN];
    bool is_dir;
} fs_item_t;

static fs_item_t fs[MAX_ITEMS];
static int fs_count = 0;

void init_fs(void) {
    fs_count = 0;
}

void cmd_ls(void) {
    print_string("\n");
    if (fs_count == 0) {
        print_string("(empty directory)\n");
        return;
    }
    for (int i = 0; i < fs_count; i++) {
        if (fs[i].is_dir) {
            print_string("[DIR]  ");
        } else {
            print_string("[FILE] ");
        }
        print_string(fs[i].name);
        print_string("\n");
    }
}

void cmd_mkdir(const char *name) {
    if (fs_count >= MAX_ITEMS) {
        print_string("\nError: File system full\n");
        return;
    }
    for (int i = 0; name[i] != '\0' && i < NAME_LEN - 1; i++) {
        fs[fs_count].name[i] = name[i];
        fs[fs_count].name[i + 1] = '\0';
    }
    fs[fs_count].is_dir = true;
    fs_count++;
    print_string("\nDirectory created successfully.\n");
}

void cmd_touch(const char *name) {
    if (fs_count >= MAX_ITEMS) {
        print_string("\nError: File system full\n");
        return;
    }
    for (int i = 0; name[i] != '\0' && i < NAME_LEN - 1; i++) {
        fs[fs_count].name[i] = name[i];
        fs[fs_count].name[i + 1] = '\0';
    }
    fs[fs_count].is_dir = false;
    fs_count++;
    print_string("\nFile created successfully.\n");
}

/* Command Parser */
void execute_command(char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print_string("\nRohnux v0.2 Commands:\n");
        print_string("  help          - Show available commands\n");
        print_string("  about         - Information about Rohnux\n");
        print_string("  clear         - Clear the screen\n");
        print_string("  ls            - List files and directories\n");
        print_string("  mkdir <name>  - Create a directory\n");
        print_string("  touch <name>  - Create a file\n");
    } else if (strcmp(cmd, "about") == 0) {
        print_string("\nRohnux Kernel v0.2 - Built from scratch\n");
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls();
    } else if (strncmp(cmd, "mkdir ", 6) == 0) {
        cmd_mkdir(cmd + 6);
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        cmd_touch(cmd + 6);
    } else if (cmd[0] != '\0') {
        print_string("\nUnknown command. Type 'help'.\n");
    }
}

/* Kernel Entry Point with Interactive Shell Loop */
void kernel_main(void) {
    terminal_initialize();
    init_fs();

    print_string("=========================\n");
    print_string("      ROHNUX v0.2        \n");
    print_string("=========================\n");
    print_string("Type 'help' for commands.\n\n");
    print_string("rohnux> ");

    char cmd_buffer[256];
    int buf_idx = 0;
    cmd_buffer[0] = '\0';

    // Interactive Shell Loop
    while (1) {
        char c = keyboard_read_char();

        if (c == '\n') { // Enter key executed
            cmd_buffer[buf_idx] = '\0';
            execute_command(cmd_buffer);
            buf_idx = 0;
            cmd_buffer[0] = '\0';
            print_string("\nrohnux> ");
        } 
        else if (c == '\b') { // Backspace key
            if (buf_idx > 0) {
                buf_idx--;
                cmd_buffer[buf_idx] = '\0';
                terminal_backspace();
            }
        } 
        else if (c >= 32 && c <= 126) { // Printable characters
            if (buf_idx < 255) {
                cmd_buffer[buf_idx++] = c;
                cmd_buffer[buf_idx] = '\0';
                char str[2] = {c, '\0'};
                print_string(str);
            }
        }
    }
}
