/*
 * JHW - Jet Hardware
 * Single-file, zero external libraries.
 *
 * Build:
 *   gcc -std=c11 -O2 -Wall -Wextra -pedantic jhw.c -o jhw
 *
 * Run:
 *   ./jhw
 *
 * Current design:
 *   jhw("...") receives C-like hardware intent code.
 *   This standalone implementation parses a small, explicit subset and
 *   dispatches it through JHW's internal hardware engine.
 *
 * IMPORTANT:
 *   This file is a self-contained JHW prototype, not a full C compiler.
 *   On JetOS, the same front-end can be connected to real kernel drivers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define JHW_MAX_CODE       65536
#define JHW_MAX_TOKENS     4096
#define JHW_MAX_TOKEN_LEN  256
#define JHW_MAX_DEVICES    64

typedef enum {
    JHW_OK = 0,
    JHW_ERR_ARGUMENT,
    JHW_ERR_SYNTAX,
    JHW_ERR_UNKNOWN,
    JHW_ERR_UNSUPPORTED,
    JHW_ERR_HARDWARE
} JHWResult;

typedef enum {
    JHW_DEV_CPU = 0,
    JHW_DEV_MEMORY,
    JHW_DEV_DISPLAY,
    JHW_DEV_STORAGE,
    JHW_DEV_NETWORK,
    JHW_DEV_INPUT
} JHWDeviceType;

typedef struct {
    JHWDeviceType type;
    int present;
    char name[64];
} JHWDevice;

typedef struct {
    JHWDevice devices[JHW_MAX_DEVICES];
    size_t device_count;

    uint8_t *memory;
    size_t memory_size;

    unsigned width;
    unsigned height;
    uint32_t *framebuffer;

    uint64_t instructions;
    int initialized;
} JHWContext;

static JHWContext g_jhw;

static const char *jhw_skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *jhw_skip_comment(const char *p) {
    p = jhw_skip_ws(p);

    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        while (*p && *p != '\n') p++;
        return p;
    }

    if (p[0] == '/' && p[1] == '*') {
        p += 2;
        while (*p && !(p[0] == '*' && p[1] == '/')) p++;
        if (*p) p += 2;
        return p;
    }

    return p;
}

static const char *jhw_clean(const char *p) {
    for (;;) {
        const char *q = jhw_skip_comment(p);
        if (q == p) return p;
        p = q;
    }
}

static int jhw_identifier(const char **pp, char *out, size_t out_sz) {
    const char *p;
    size_t n = 0;

    if (!pp || !*pp || !out || out_sz == 0) return 0;

    p = jhw_clean(*pp);
    if (!(isalpha((unsigned char)*p) || *p == '_')) return 0;

    while (isalnum((unsigned char)*p) || *p == '_') {
        if (n + 1 >= out_sz) return 0;
        out[n++] = *p++;
    }

    out[n] = '\0';
    *pp = p;
    return 1;
}

static int jhw_number(const char **pp, uint64_t *out) {
    char *end;
    const char *p;
    unsigned long long v;

    if (!pp || !*pp || !out) return 0;

    p = jhw_clean(*pp);
    if (!isdigit((unsigned char)*p)) return 0;

    v = strtoull(p, &end, 0);
    if (end == p) return 0;

    *out = (uint64_t)v;
    *pp = end;
    return 1;
}

static int jhw_string(const char **pp, char *out, size_t out_sz) {
    const char *p;
    size_t n = 0;

    if (!pp || !*pp || !out || out_sz == 0) return 0;

    p = jhw_clean(*pp);
    if (*p != '"') return 0;
    p++;

    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;

        if (c == '\\') {
            if (!*p) return 0;
            c = (unsigned char)*p++;
            switch (c) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                default: break;
            }
        }

        if (n + 1 >= out_sz) return 0;
        out[n++] = (char)c;
    }

    if (*p != '"') return 0;
    out[n] = '\0';
    *pp = p + 1;
    return 1;
}

static int jhw_expect(const char **pp, char ch) {
    const char *p = jhw_clean(*pp);
    if (*p != ch) return 0;
    *pp = p + 1;
    return 1;
}

static void jhw_register_device(JHWDeviceType type, const char *name) {
    if (g_jhw.device_count >= JHW_MAX_DEVICES) return;

    g_jhw.devices[g_jhw.device_count].type = type;
    g_jhw.devices[g_jhw.device_count].present = 1;

    snprintf(
        g_jhw.devices[g_jhw.device_count].name,
        sizeof(g_jhw.devices[g_jhw.device_count].name),
        "%s",
        name ? name : "unknown"
    );

    g_jhw.device_count++;
}

static void jhw_detect_hardware(void) {
    g_jhw.device_count = 0;

    /* Generic portable baseline. JetOS can replace these with real probes. */
    jhw_register_device(JHW_DEV_CPU, "generic-cpu");
    jhw_register_device(JHW_DEV_MEMORY, "system-memory");
    jhw_register_device(JHW_DEV_DISPLAY, "generic-display");
    jhw_register_device(JHW_DEV_STORAGE, "generic-storage");
    jhw_register_device(JHW_DEV_NETWORK, "generic-network");
    jhw_register_device(JHW_DEV_INPUT, "generic-input");
}

static int jhw_has_device(JHWDeviceType type) {
    size_t i;
    for (i = 0; i < g_jhw.device_count; i++) {
        if (g_jhw.devices[i].type == type &&
            g_jhw.devices[i].present) {
            return 1;
        }
    }
    return 0;
}

static JHWResult jhw_backend_memory_write(uint64_t address, uint64_t value, unsigned width) {
    size_t i;

    if (!g_jhw.memory || g_jhw.memory_size == 0)
        return JHW_ERR_HARDWARE;

    if (address >= g_jhw.memory_size)
        return JHW_ERR_ARGUMENT;

    if ((size_t)width > g_jhw.memory_size - (size_t)address)
        return JHW_ERR_ARGUMENT;

    for (i = 0; i < width; i++)
        g_jhw.memory[address + i] = (uint8_t)(value >> (i * 8));

    printf("[JHW] memory.write addr=%llu width=%u value=%llu\n",
           (unsigned long long)address,
           width,
           (unsigned long long)value);

    return JHW_OK;
}

static JHWResult jhw_backend_memory_read(uint64_t address, unsigned width, uint64_t *value) {
    uint64_t v = 0;
    unsigned i;

    if (!value || !g_jhw.memory || g_jhw.memory_size == 0)
        return JHW_ERR_ARGUMENT;

    if (address >= g_jhw.memory_size)
        return JHW_ERR_ARGUMENT;

    if ((size_t)width > g_jhw.memory_size - (size_t)address)
        return JHW_ERR_ARGUMENT;

    for (i = 0; i < width; i++)
        v |= ((uint64_t)g_jhw.memory[address + i]) << (i * 8);

    *value = v;

    printf("[JHW] memory.read addr=%llu width=%u -> %llu\n",
           (unsigned long long)address,
           width,
           (unsigned long long)v);

    return JHW_OK;
}

static JHWResult jhw_backend_display_clear(uint32_t color) {
    size_t pixels, i;

    if (!g_jhw.framebuffer || g_jhw.width == 0 || g_jhw.height == 0)
        return JHW_ERR_HARDWARE;

    pixels = (size_t)g_jhw.width * (size_t)g_jhw.height;
    for (i = 0; i < pixels; i++)
        g_jhw.framebuffer[i] = color;

    printf("[JHW] display.clear color=0x%08X (%ux%u)\n",
           color, g_jhw.width, g_jhw.height);

    return JHW_OK;
}

static JHWResult jhw_backend_display_pixel(unsigned x, unsigned y, uint32_t color) {
    if (!g_jhw.framebuffer || x >= g_jhw.width || y >= g_jhw.height)
        return JHW_ERR_ARGUMENT;

    g_jhw.framebuffer[(size_t)y * g_jhw.width + x] = color;

    printf("[JHW] display.pixel x=%u y=%u color=0x%08X\n",
           x, y, color);

    return JHW_OK;
}

static JHWResult jhw_backend_console_write(const char *text) {
    if (!text) return JHW_ERR_ARGUMENT;
    fputs("[JHW] console: ", stdout);
    fputs(text, stdout);
    fputc('\n', stdout);
    return JHW_OK;
}

static JHWResult jhw_backend_cpu_halt(void) {
    printf("[JHW] cpu.halt requested\n");
    return JHW_OK;
}

static JHWResult jhw_backend_storage_read(uint64_t lba, uint64_t count) {
    if (!jhw_has_device(JHW_DEV_STORAGE))
        return JHW_ERR_HARDWARE;

    /*
     * Portable prototype:
     * real JetOS backend replaces this with AHCI/NVMe/USB/VirtIO.
     */
    printf("[JHW] storage.read lba=%llu count=%llu\n",
           (unsigned long long)lba,
           (unsigned long long)count);

    return JHW_OK;
}

static JHWResult jhw_backend_network_send(uint64_t size) {
    if (!jhw_has_device(JHW_DEV_NETWORK))
        return JHW_ERR_HARDWARE;

    printf("[JHW] network.send bytes=%llu\n",
           (unsigned long long)size);

    return JHW_OK;
}

static JHWResult jhw_call(const char *name, const char **pp) {
    uint64_t a, b, c;
    char text[1024];

    if (strcmp(name, "memory_write") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_expect(pp, ',') ||
            !jhw_number(pp, &b) ||
            !jhw_expect(pp, ',') ||
            !jhw_number(pp, &c) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_memory_write(a, b, (unsigned)c);
    }

    if (strcmp(name, "memory_read") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_number(pp, &b) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_memory_read(a, (unsigned)b, &c);
    }

    if (strcmp(name, "display_clear") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_display_clear((uint32_t)a);
    }

    if (strcmp(name, "display_pixel") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_expect(pp, ',') ||
            !jhw_number(pp, &b) ||
            !jhw_expect(pp, ',') ||
            !jhw_number(pp, &c) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_display_pixel((unsigned)a, (unsigned)b, (uint32_t)c);
    }

    if (strcmp(name, "console") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_string(pp, text, sizeof(text)) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_console_write(text);
    }

    if (strcmp(name, "storage_read") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_expect(pp, ',') ||
            !jhw_number(pp, &b) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_storage_read(a, b);
    }

    if (strcmp(name, "network_send") == 0) {
        if (!jhw_expect(pp, '(') ||
            !jhw_number(pp, &a) ||
            !jhw_expect(pp, ')'))
            return JHW_ERR_SYNTAX;

        return jhw_backend_network_send(a);
    }

    if (strcmp(name, "cpu_halt") == 0) {
        if (!jhw_expect(pp, ')')) {
            const char *q = jhw_clean(*pp);
            if (*q == '(') {
                *pp = q + 1;
                if (!jhw_expect(pp, ')'))
                    return JHW_ERR_SYNTAX;
            } else {
                return JHW_ERR_SYNTAX;
            }
        }

        return jhw_backend_cpu_halt();
    }

    return JHW_ERR_UNKNOWN;
}

static JHWResult jhw_execute(const char *code) {
    const char *p;
    char name[JHW_MAX_TOKEN_LEN];

    if (!code) return JHW_ERR_ARGUMENT;

    p = code;

    while (*(p = jhw_clean(p)) != '\0') {
        if (!jhw_identifier(&p, name, sizeof(name)))
            return JHW_ERR_SYNTAX;

        if (!jhw_expect(&p, '('))
            return JHW_ERR_SYNTAX;

        /*
         * jhw(...) accepts a sequence of JHW operations.
         * Each operation is written as:
         *     operation(args)
         */
        if (strcmp(name, "memory_write") == 0) {
            uint64_t a, b, c;

            if (!jhw_number(&p, &a) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &b) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &c) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_memory_write(a, b, (unsigned)c) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "memory_read") == 0) {
            uint64_t a, b, value;

            if (!jhw_number(&p, &a) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &b) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_memory_read(a, (unsigned)b, &value) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "display_clear") == 0) {
            uint64_t color;

            if (!jhw_number(&p, &color) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_display_clear((uint32_t)color) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "display_pixel") == 0) {
            uint64_t x, y, color;

            if (!jhw_number(&p, &x) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &y) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &color) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_display_pixel(
                    (unsigned)x, (unsigned)y, (uint32_t)color) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "console") == 0) {
            char text[1024];

            if (!jhw_string(&p, text, sizeof(text)) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_console_write(text) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "storage_read") == 0) {
            uint64_t lba, count;

            if (!jhw_number(&p, &lba) ||
                !jhw_expect(&p, ',') ||
                !jhw_number(&p, &count) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_storage_read(lba, count) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "network_send") == 0) {
            uint64_t size;

            if (!jhw_number(&p, &size) ||
                !jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_network_send(size) != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else if (strcmp(name, "cpu_halt") == 0) {
            if (!jhw_expect(&p, ')'))
                return JHW_ERR_SYNTAX;

            if (jhw_backend_cpu_halt() != JHW_OK)
                return JHW_ERR_HARDWARE;
        } else {
            /* Generic helper for future backends. */
            const char *q = p - 1;
            (void)q;
            return JHW_ERR_UNKNOWN;
        }

        p = jhw_clean(p);

        if (*p == ';') p++;
        else if (*p != '\0') return JHW_ERR_SYNTAX;

        g_jhw.instructions++;
    }

    return JHW_OK;
}

/*
 * The public JHW function.
 *
 * User-facing form:
 *
 *     jhw(
 *         console("Hello JetOS");
 *         display_clear(0x00000000);
 *         display_pixel(10, 10, 0xFFFFFFFF);
 *         storage_read(0, 1);
 *     );
 *
 * In this standalone file, the argument is represented as a C string.
 * The JetOS/JCC integration stage can lower the actual jhw(C-code) syntax
 * directly to this engine instead of requiring a runtime string.
 */
JHWResult jhw(const char *c_code) {
    if (!g_jhw.initialized) {
        jhw_detect_hardware();

        g_jhw.width = 640;
        g_jhw.height = 480;

        g_jhw.memory_size = 1024 * 1024;
        g_jhw.memory = (uint8_t *)calloc(g_jhw.memory_size, 1);

        g_jhw.framebuffer =
            (uint32_t *)calloc((size_t)g_jhw.width * g_jhw.height,
                               sizeof(uint32_t));

        if (!g_jhw.memory || !g_jhw.framebuffer) {
            free(g_jhw.memory);
            free(g_jhw.framebuffer);
            g_jhw.memory = NULL;
            g_jhw.framebuffer = NULL;
            return JHW_ERR_HARDWARE;
        }

        g_jhw.initialized = 1;
    }

    return jhw_execute(c_code);
}

static const char *jhw_result_name(JHWResult r) {
    switch (r) {
        case JHW_OK: return "JHW_OK";
        case JHW_ERR_ARGUMENT: return "JHW_ERR_ARGUMENT";
        case JHW_ERR_SYNTAX: return "JHW_ERR_SYNTAX";
        case JHW_ERR_UNKNOWN: return "JHW_ERR_UNKNOWN";
        case JHW_ERR_UNSUPPORTED: return "JHW_ERR_UNSUPPORTED";
        case JHW_ERR_HARDWARE: return "JHW_ERR_HARDWARE";
        default: return "JHW_ERR_UNKNOWN";
    }
}

static void jhw_shutdown(void) {
    free(g_jhw.memory);
    free(g_jhw.framebuffer);
    memset(&g_jhw, 0, sizeof(g_jhw));
}

int main(void) {
    const char *program =
        "console(\"Hello from JHW\");"
        "display_clear(0x00102030);"
        "display_pixel(10,20,0x00FFFFFF);"
        "memory_write(16,123456,4);"
        "memory_read(16,4);"
        "storage_read(0,1);"
        "network_send(64);";

    JHWResult r = jhw(program);

    printf("result=%s\n", jhw_result_name(r));
    printf("instructions=%llu\n",
           (unsigned long long)g_jhw.instructions);

    jhw_shutdown();
    return r == JHW_OK ? 0 : 1;
}
