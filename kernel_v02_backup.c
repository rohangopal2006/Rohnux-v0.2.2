#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static volatile uint16_t *const VGA_MEMORY =
    (volatile uint16_t *)0xB8000;

static size_t cursor_row = 0;
static size_t cursor_col = 0;

static void clear_screen(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (uint16_t)' ' | (uint16_t)(0x0F << 8);
    }

    cursor_row = 0;
    cursor_col = 0;
}

static void scroll_screen(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; row++) {
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] =
                VGA_MEMORY[row * VGA_WIDTH + col];
        }
    }

    for (size_t col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            (uint16_t)' ' | (uint16_t)(0x0F << 8);
    }

    cursor_row = VGA_HEIGHT - 1;
}

static void put_char(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
                (uint16_t)' ' | (uint16_t)(0x0F << 8);
        }
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)c | (uint16_t)(0x0F << 8);

        cursor_col++;

        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll_screen();
    }
}

static void print(const char *text)
{
    while (*text) {
        put_char(*text++);
    }
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static char scancode_to_ascii(uint8_t code)
{
    static const char keys[128] = {
        [0x02] = '1', [0x03] = '2', [0x04] = '3',
        [0x05] = '4', [0x06] = '5', [0x07] = '6',
        [0x08] = '7', [0x09] = '8', [0x0A] = '9',
        [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
        [0x0E] = '\b', [0x0F] = '\t',
        [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',
        [0x13] = 'r', [0x14] = 't', [0x15] = 'y',
        [0x16] = 'u', [0x17] = 'i', [0x18] = 'o',
        [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
        [0x1C] = '\n',
        [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
        [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
        [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
        [0x27] = ';', [0x28] = '\'',
        [0x29] = '`', [0x2B] = '\\',
        [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
        [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
        [0x32] = 'm', [0x33] = ',', [0x34] = '.',
        [0x35] = '/', [0x39] = ' '
    };

    if (code >= 128) {
        return 0;
    }

    return keys[code];
}

static void keyboard_test(void)
{
    while (1) {
        /* Bit 0 means keyboard data is available. */
        if ((inb(0x64) & 1) == 0) {
            continue;
        }

        uint8_t code = inb(0x60);

        /* Ignore key-release events. */
        if (code & 0x80) {
            continue;
        }

        char c = scancode_to_ascii(code);

        if (c == '\t') {
            c = ' ';
        }

        if (c != 0) {
            put_char(c);
        }
    }
}

void kernel_main(void)
{
    clear_screen();

    print("================================\n");
    print("          ROHNUX v0.2\n");
    print("================================\n\n");

    print("Keyboard input test\n");
    print("Type something below:\n\n");
    print("rohnux> ");

    keyboard_test();
}
