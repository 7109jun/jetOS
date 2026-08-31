/*

* JHW - Jet Hardware
* Single-file implementation
* External libraries: 0
*
* Build:
* Linux : gcc -std=c11 -O2 -Wall -Wextra jhw.c -o jhw
* macOS : clang -std=c11 -O2 -Wall -Wextra jhw.c -o jhw
* Win32 : gcc -std=c11 -O2 -Wall -Wextra jhw.c -o jhw.exe
*
* JHW commands:
*
* /goal all
* /goal hardware 70
* /goal cpu
* /goal gpu
* /goal storage
* /goal network
* /goal usb
* /goal input
* /goal sensor
*
* cpu.info();
* memory.info();
* pci.list();
* usb.list();
* storage.list();
* network.list();
* input.list();
* sensor.list();
* display.info();
* system.info();
* hardware.summary();
*
* jhw(
* ```
    cpu.info();
  ```
* ```
    memory.info();
  ```
* ```
    pci.list();
  ```
* );
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)

#define JHW_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/stat.h>

#elif defined(**linux**)

#define JHW_LINUX 1
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#elif defined(**APPLE**)

#define JHW_MACOS 1
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>

#else

#define JHW_GENERIC 1
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#endif

/* ============================================================

* Configuration
* ============================================================ */

#define JHW_VERSION_MAJOR 1
#define JHW_VERSION_MINOR 0

#define JHW_MAX_DEVICES 4096
#define JHW_MAX_NAME 256
#define JHW_MAX_PATH 1024
#define JHW_MAX_INPUT 65536

#define JHW_GOAL_CPU     0x00000001u
#define JHW_GOAL_MEMORY  0x00000002u
#define JHW_GOAL_PCI     0x00000004u
#define JHW_GOAL_USB     0x00000008u
#define JHW_GOAL_GPU     0x00000010u
#define JHW_GOAL_STORAGE 0x00000020u
#define JHW_GOAL_NETWORK 0x00000040u
#define JHW_GOAL_INPUT   0x00000080u
#define JHW_GOAL_AUDIO   0x00000100u
#define JHW_GOAL_SENSOR  0x00000200u
#define JHW_GOAL_POWER   0x00000400u
#define JHW_GOAL_ACPI    0x00000800u
#define JHW_GOAL_DISPLAY 0x00001000u
#define JHW_GOAL_ALL     0xffffffffu

/* ============================================================

* Result
* ============================================================ */

typedef enum
{
JHW_OK = 0,
JHW_ERROR = -1,
JHW_INVALID_ARGUMENT = -2,
JHW_NOT_SUPPORTED = -3,
JHW_NOT_FOUND = -4,
JHW_IO_ERROR = -5,
JHW_PERMISSION = -6,
JHW_SYNTAX = -7
} JHWResult;

/* ============================================================

* Device
* ============================================================ */

typedef enum
{
JHW_DEV_UNKNOWN = 0,
JHW_DEV_CPU,
JHW_DEV_MEMORY,
JHW_DEV_PCI,
JHW_DEV_USB,
JHW_DEV_GPU,
JHW_DEV_STORAGE,
JHW_DEV_NETWORK,
JHW_DEV_INPUT,
JHW_DEV_AUDIO,
JHW_DEV_SENSOR,
JHW_DEV_DISPLAY,
JHW_DEV_POWER,
JHW_DEV_FIRMWARE
} JHWDeviceType;

typedef struct
{
uint32_t id;
JHWDeviceType type;

```
uint64_t vendor;
uint64_t device;
uint64_t capacity;

uint32_t class_code;

char name[JHW_MAX_NAME];
char path[JHW_MAX_PATH];
```

} JHWDevice;

/* ============================================================

* Runtime
* ============================================================ */

typedef struct
{
JHWDevice devices[JHW_MAX_DEVICES];
size_t device_count;

```
uint32_t goals;
unsigned compatibility_goal;

unsigned cpu_count;
uint64_t memory_bytes;

int initialized;
```

} JHWRuntime;

static JHWRuntime J;

/* ============================================================

* Helpers
* ============================================================ */

static void jhw_copy(char *dst, size_t n, const char *src)
{
if (!dst || !n)
return;

```
if (!src)
{
    dst[0] = 0;
    return;
}

strncpy(dst, src, n - 1);
dst[n - 1] = 0;
```

}

static int jhw_file_read(
const char *path,
char *out,
size_t size
)
{
FILE *f;
size_t n;

```
if (!path || !out || size < 2)
    return 0;

f = fopen(path, "rb");

if (!f)
    return 0;

n = fread(out, 1, size - 1, f);
fclose(f);

out[n] = 0;

while (n &&
       isspace((unsigned char)out[n - 1]))
    out[--n] = 0;

return 1;
```

}

static uint64_t jhw_number(const char *s)
{
char *end = NULL;

```
if (!s)
    return 0;

return (uint64_t)strtoull(s, &end, 0);
```

}

static int jhw_join(
char *out,
size_t size,
const char *a,
const char *b
)
{
int n;

```
if (!out || !a || !b)
    return 0;
```

#if defined(_WIN32) || defined(_WIN64)

```
n = snprintf(out, size, "%s\\%s", a, b);
```

#else

```
n = snprintf(out, size, "%s/%s", a, b);
```

#endif

```
return n >= 0 && (size_t)n < size;
```

}

/* ============================================================

* Names
* ============================================================ */

static const char *jhw_type_name(JHWDeviceType t)
{
switch (t)
{
case JHW_DEV_CPU:     return "CPU";
case JHW_DEV_MEMORY:  return "MEMORY";
case JHW_DEV_PCI:     return "PCI";
case JHW_DEV_USB:     return "USB";
case JHW_DEV_GPU:     return "GPU";
case JHW_DEV_STORAGE: return "STORAGE";
case JHW_DEV_NETWORK: return "NETWORK";
case JHW_DEV_INPUT:   return "INPUT";
case JHW_DEV_AUDIO:   return "AUDIO";
case JHW_DEV_SENSOR:  return "SENSOR";
case JHW_DEV_DISPLAY: return "DISPLAY";
case JHW_DEV_POWER:   return "POWER";
case JHW_DEV_FIRMWARE:return "FIRMWARE";
default:              return "UNKNOWN";
}
}

/* ============================================================

* Device registry
* ============================================================ */

static uint32_t jhw_add_device(
JHWDeviceType type,
const char *name,
const char *path,
uint64_t vendor,
uint64_t device,
uint64_t capacity,
uint32_t class_code
)
{
JHWDevice *d;

```
if (J.device_count >= JHW_MAX_DEVICES)
    return 0;

d = &J.devices[J.device_count];

memset(d, 0, sizeof(*d));

d->id = (uint32_t)J.device_count + 1;
d->type = type;
d->vendor = vendor;
d->device = device;
d->capacity = capacity;
d->class_code = class_code;

jhw_copy(d->name, sizeof(d->name), name);
jhw_copy(d->path, sizeof(d->path), path);

J.device_count++;

return d->id;
```

}

/* ============================================================

* Linux CPU
* ============================================================ */

#if JHW_LINUX

static void jhw_linux_cpu(void)
{
FILE *f;
char line[512];
char model[JHW_MAX_NAME];

```
unsigned count = 0;

memset(model, 0, sizeof(model));

f = fopen("/proc/cpuinfo", "r");

if (!f)
    return;

while (fgets(line, sizeof(line), f))
{
    if (!strncmp(line, "processor", 9))
        count++;

    if (!strncmp(line, "model name", 10) &&
        !model[0])
    {
        char *p = strchr(line, ':');

        if (p)
        {
            p++;

            while (isspace((unsigned char)*p))
                p++;

            jhw_copy(model, sizeof(model), p);
        }
    }
}

fclose(f);

if (!count)
    count = 1;

J.cpu_count = count;

jhw_add_device(
    JHW_DEV_CPU,
    model[0] ? model : "CPU",
    "/proc/cpuinfo",
    0,
    0,
    0,
    0
);
```

}

static void jhw_linux_memory(void)
{
FILE *f;
char line[512];

```
f = fopen("/proc/meminfo", "r");

if (!f)
    return;

while (fgets(line, sizeof(line), f))
{
    unsigned long long kb;

    if (sscanf(
            line,
            "MemTotal: %llu kB",
            &kb
        ) == 1)
    {
        J.memory_bytes =
            (uint64_t)kb * 1024ULL;

        break;
    }
}

fclose(f);

jhw_add_device(
    JHW_DEV_MEMORY,
    "System RAM",
    "/proc/meminfo",
    0,
    0,
    J.memory_bytes,
    0
);
```

}

/* ============================================================

* Linux PCI
* ============================================================ */

static JHWDeviceType jhw_linux_pci_type(uint32_t c)
{
unsigned base = (c >> 16) & 0xff;

```
switch (base)
{
    case 0x01:
        return JHW_DEV_STORAGE;

    case 0x02:
        return JHW_DEV_NETWORK;

    case 0x03:
        return JHW_DEV_GPU;

    case 0x04:
        return JHW_DEV_AUDIO;

    case 0x0c:
    {
        unsigned sub = (c >> 8) & 0xff;

        if (sub == 0x03)
            return JHW_DEV_USB;

        break;
    }

    default:
        break;
}

return JHW_DEV_PCI;
```

}

static void jhw_linux_pci(void)
{
DIR *dir;
struct dirent *e;

```
dir = opendir("/sys/bus/pci/devices");

if (!dir)
    return;

while ((e = readdir(dir)))
{
    char root[JHW_MAX_PATH];
    char path[JHW_MAX_PATH];

    char vendor_s[64];
    char device_s[64];
    char class_s[64];

    uint64_t vendor;
    uint64_t device;
    uint64_t class_code;

    char name[JHW_MAX_NAME];

    vendor_s[0] = 0;
    device_s[0] = 0;
    class_s[0] = 0;

    if (e->d_name[0] == '.')
        continue;

    if (!jhw_join(
            root,
            sizeof(root),
            "/sys/bus/pci/devices",
            e->d_name))
        continue;

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            "vendor"))
        continue;

    jhw_file_read(
        path,
        vendor_s,
        sizeof(vendor_s)
    );

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            "device"))
        continue;

    jhw_file_read(
        path,
        device_s,
        sizeof(device_s)
    );

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            "class"))
        continue;

    jhw_file_read(
        path,
        class_s,
        sizeof(class_s)
    );

    vendor = jhw_number(vendor_s);
    device = jhw_number(device_s);
    class_code = jhw_number(class_s);

    snprintf(
        name,
        sizeof(name),
        "%s",
        e->d_name
    );

    jhw_add_device(
        jhw_linux_pci_type((uint32_t)class_code),
        name,
        root,
        vendor,
        device,
        0,
        (uint32_t)class_code
    );
}

closedir(dir);
```

}

/* ============================================================

* Linux USB
* ============================================================ */

static void jhw_linux_usb(void)
{
DIR *dir;
struct dirent *e;

```
dir = opendir("/sys/bus/usb/devices");

if (!dir)
    return;

while ((e = readdir(dir)))
{
    char root[JHW_MAX_PATH];
    char path[JHW_MAX_PATH];

    char vendor_s[64];
    char product[JHW_MAX_NAME];

    uint64_t vendor;

    vendor_s[0] = 0;
    product[0] = 0;

    if (e->d_name[0] == '.')
        continue;

    if (!jhw_join(
            root,
            sizeof(root),
            "/sys/bus/usb/devices",
            e->d_name))
        continue;

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            "idVendor"))
        continue;

    jhw_file_read(
        path,
        vendor_s,
        sizeof(vendor_s)
    );

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            "product"))
        continue;

    jhw_file_read(
        path,
        product,
        sizeof(product)
    );

    if (!vendor_s[0] && !product[0])
        continue;

    vendor = jhw_number(vendor_s);

    jhw_add_device(
        JHW_DEV_USB,
        product[0] ? product : e->d_name,
        root,
        vendor,
        0,
        0,
        0
    );
}

closedir(dir);
```

}

/* ============================================================

* Linux generic class scanner
* ============================================================ */

static void jhw_linux_class(
const char *root,
JHWDeviceType type
)
{
DIR *dir;
struct dirent *e;

```
dir = opendir(root);

if (!dir)
    return;

while ((e = readdir(dir)))
{
    char path[JHW_MAX_PATH];

    if (e->d_name[0] == '.')
        continue;

    if (!jhw_join(
            path,
            sizeof(path),
            root,
            e->d_name))
        continue;

    jhw_add_device(
        type,
        e->d_name,
        path,
        0,
        0,
        0,
        0
    );
}

closedir(dir);
```

}

#endif

/* ============================================================

* Windows discovery
* ============================================================ */

#if JHW_WINDOWS

static void jhw_windows_discover(void)
{
SYSTEM_INFO si;
MEMORYSTATUSEX ms;

```
memset(&si, 0, sizeof(si));

GetSystemInfo(&si);

J.cpu_count =
    (unsigned)si.dwNumberOfProcessors;

memset(&ms, 0, sizeof(ms));

ms.dwLength = sizeof(ms);

if (GlobalMemoryStatusEx(&ms))
    J.memory_bytes = ms.ullTotalPhys;

jhw_add_device(
    JHW_DEV_CPU,
    "Windows CPU",
    "windows",
    0,
    0,
    0,
    0
);

jhw_add_device(
    JHW_DEV_MEMORY,
    "System RAM",
    "windows",
    0,
    0,
    J.memory_bytes,
    0
);

jhw_add_device(
    JHW_DEV_FIRMWARE,
    "Windows firmware",
    "windows",
    0,
    0,
    0,
    0
);
```

}

#endif

/* ============================================================

* macOS discovery
* ============================================================ */

#if JHW_MACOS

static void jhw_macos_discover(void)
{
unsigned cpu = 1;
size_t size = sizeof(cpu);

```
if (sysctlbyname(
        "hw.ncpu",
        &cpu,
        &size,
        NULL,
        0) == 0)
{
    J.cpu_count = cpu;
}
else
{
    J.cpu_count = 1;
}

jhw_add_device(
    JHW_DEV_CPU,
    "macOS CPU",
    "macOS",
    0,
    0,
    0,
    0
);

jhw_add_device(
    JHW_DEV_MEMORY,
    "System RAM",
    "macOS",
    0,
    0,
    0,
    0
);
```

}

#endif

/* ============================================================

* Discovery
* ============================================================ */

static void jhw_discover(void)
{
J.device_count = 0;

#if JHW_LINUX

```
jhw_linux_cpu();
jhw_linux_memory();

jhw_linux_pci();
jhw_linux_usb();

jhw_linux_class(
    "/sys/class/block",
    JHW_DEV_STORAGE
);

jhw_linux_class(
    "/sys/class/net",
    JHW_DEV_NETWORK
);

jhw_linux_class(
    "/sys/class/input",
    JHW_DEV_INPUT
);

jhw_linux_class(
    "/sys/class/drm",
    JHW_DEV_DISPLAY
);

jhw_linux_class(
    "/sys/class/hwmon",
    JHW_DEV_SENSOR
);
```

#elif JHW_WINDOWS

```
jhw_windows_discover();
```

#elif JHW_MACOS

```
jhw_macos_discover();
```

#else

```
J.cpu_count = 1;

jhw_add_device(
    JHW_DEV_CPU,
    "Generic CPU",
    "generic",
    0,
    0,
    0,
    0
);
```

#endif
}

/* ============================================================

* Initialization
* ============================================================ */

static JHWResult jhw_init(void)
{
if (J.initialized)
return JHW_OK;

```
memset(&J, 0, sizeof(J));

J.compatibility_goal = 70;
J.goals = JHW_GOAL_ALL;

jhw_discover();

J.initialized = 1;

return JHW_OK;
```

}

/* ============================================================

* API
* ============================================================ */

static JHWResult jhw_system_info(void)
{
puts("JHW");
printf(
"version=%d.%d\n",
JHW_VERSION_MAJOR,
JHW_VERSION_MINOR
);

#if JHW_LINUX
puts("os=Linux");
#elif JHW_WINDOWS
puts("os=Windows");
#elif JHW_MACOS
puts("os=macOS");
#else
puts("os=Generic");
#endif

```
printf(
    "devices=%zu\n",
    J.device_count
);

printf(
    "compatibility_goal=%u%%\n",
    J.compatibility_goal
);

return JHW_OK;
```

}

static JHWResult jhw_hardware_summary(void)
{
size_t i;
unsigned counts[16];

```
memset(counts, 0, sizeof(counts));

for (i = 0; i < J.device_count; i++)
{
    unsigned t =
        (unsigned)J.devices[i].type;

    if (t < 16)
        counts[t]++;
}

printf("CPU      : %u\n", counts[JHW_DEV_CPU]);
printf("MEMORY   : %u\n", counts[JHW_DEV_MEMORY]);
printf("PCI      : %u\n", counts[JHW_DEV_PCI]);
printf("USB      : %u\n", counts[JHW_DEV_USB]);
printf("GPU      : %u\n", counts[JHW_DEV_GPU]);
printf("STORAGE  : %u\n", counts[JHW_DEV_STORAGE]);
printf("NETWORK  : %u\n", counts[JHW_DEV_NETWORK]);
printf("INPUT    : %u\n", counts[JHW_DEV_INPUT]);
printf("AUDIO    : %u\n", counts[JHW_DEV_AUDIO]);
printf("SENSOR   : %u\n", counts[JHW_DEV_SENSOR]);
printf("DISPLAY  : %u\n", counts[JHW_DEV_DISPLAY]);

return JHW_OK;
```

}

static JHWResult jhw_device_list(void)
{
size_t i;

```
printf(
    "%-5s %-10s %-32s %-18s %s\n",
    "ID",
    "TYPE",
    "NAME",
    "VENDOR/DEVICE",
    "PATH"
);

for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    printf(
        "%-5u %-10s %-32s %04llx:%04llx %s\n",
        d->id,
        jhw_type_name(d->type),
        d->name,
        (unsigned long long)d->vendor,
        (unsigned long long)d->device,
        d->path
    );
}

return JHW_OK;
```

}

static JHWResult jhw_cpu_info(void)
{
printf(
"logical_cpu_count=%u\n",
J.cpu_count
);

#if JHW_LINUX

```
{
    char text[8192];
    char *p;

    if (jhw_file_read(
            "/proc/cpuinfo",
            text,
            sizeof(text)))
    {
        p = strstr(text, "model name");

        if (p)
        {
            p = strchr(p, ':');

            if (p)
            {
                p++;

                while (isspace((unsigned char)*p))
                    p++;

                printf("model=%s\n", p);
            }
        }
    }
}
```

#endif

```
return JHW_OK;
```

}

static JHWResult jhw_memory_info(void)
{
printf(
"total_bytes=%llu\n",
(unsigned long long)J.memory_bytes
);

```
printf(
    "total_mib=%llu\n",
    (unsigned long long)(
        J.memory_bytes / 1024ULL / 1024ULL
    )
);

return JHW_OK;
```

}

static JHWResult jhw_storage_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_STORAGE)
        continue;

    printf(
        "%u | %s | %s | %llu bytes\n",
        d->id,
        d->name,
        d->path,
        (unsigned long long)d->capacity
    );
}

return JHW_OK;
```

}

static JHWResult jhw_network_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_NETWORK)
        continue;

    printf(
        "%u | %s | %s\n",
        d->id,
        d->name,
        d->path
    );
}

return JHW_OK;
```

}

static JHWResult jhw_usb_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_USB)
        continue;

    printf(
        "%u | USB | %s | vendor=%04llx | %s\n",
        d->id,
        d->name,
        (unsigned long long)d->vendor,
        d->path
    );
}

return JHW_OK;
```

}

static JHWResult jhw_pci_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_PCI &&
        d->type != JHW_DEV_GPU &&
        d->type != JHW_DEV_AUDIO &&
        d->type != JHW_DEV_NETWORK &&
        d->type != JHW_DEV_STORAGE)
        continue;

    printf(
        "%u | %-8s | %04llx:%04llx | class=%06x | %s\n",
        d->id,
        jhw_type_name(d->type),
        (unsigned long long)d->vendor,
        (unsigned long long)d->device,
        d->class_code,
        d->name
    );
}

return JHW_OK;
```

}

static JHWResult jhw_input_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_INPUT)
        continue;

    printf(
        "%u | %s | %s\n",
        d->id,
        d->name,
        d->path
    );
}

return JHW_OK;
```

}

static JHWResult jhw_sensor_list(void)
{
size_t i;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_SENSOR)
        continue;

    printf(
        "%u | %s | %s\n",
        d->id,
        d->name,
        d->path
    );
}

return JHW_OK;
```

}

static JHWResult jhw_display_info(void)
{
size_t i;
unsigned n = 0;

```
for (i = 0; i < J.device_count; i++)
{
    JHWDevice *d = &J.devices[i];

    if (d->type != JHW_DEV_DISPLAY)
        continue;

    printf(
        "%u | %s | %s\n",
        d->id,
        d->name,
        d->path
    );

    n++;
}

if (!n)
    puts("display=not-detected");

return JHW_OK;
```

}

/* ============================================================

* /goal
* ============================================================ */

static void jhw_goal_print(void)
{
printf(
"goal=hardware-%u%%\n",
J.compatibility_goal
);

```
printf(
    "features=0x%08x\n",
    J.goals
);
```

}

static JHWResult jhw_goal(
const char *args
)
{
char target[64];
unsigned value;

```
if (!args)
    return JHW_INVALID_ARGUMENT;

target[0] = 0;

if (sscanf(
        args,
        "%63s %u",
        target,
        &value
    ) < 1)
{
    return JHW_SYNTAX;
}

if (!strcmp(target, "hardware"))
{
    if (value > 100)
        return JHW_INVALID_ARGUMENT;

    J.compatibility_goal = value;

    printf(
        "hardware compatibility goal = %u%%\n",
        value
    );

    return JHW_OK;
}

if (!strcmp(target, "all"))
{
    J.goals = JHW_GOAL_ALL;

    if (strstr(args, " "))
    {
        if (sscanf(args, "%*s %u", &value) == 1)
            J.compatibility_goal = value;
    }

    jhw_goal_print();

    return JHW_OK;
}

if (!strcmp(target, "cpu"))
    J.goals |= JHW_GOAL_CPU;

else if (!strcmp(target, "memory"))
    J.goals |= JHW_GOAL_MEMORY;

else if (!strcmp(target, "pci"))
    J.goals |= JHW_GOAL_PCI;

else if (!strcmp(target, "usb"))
    J.goals |= JHW_GOAL_USB;

else if (!strcmp(target, "gpu"))
    J.goals |= JHW_GOAL_GPU;

else if (!strcmp(target, "storage"))
    J.goals |= JHW_GOAL_STORAGE;

else if (!strcmp(target, "network"))
    J.goals |= JHW_GOAL_NETWORK;

else if (!strcmp(target, "input"))
    J.goals |= JHW_GOAL_INPUT;

else if (!strcmp(target, "audio"))
    J.goals |= JHW_GOAL_AUDIO;

else if (!strcmp(target, "sensor"))
    J.goals |= JHW_GOAL_SENSOR;

else if (!strcmp(target, "power"))
    J.goals |= JHW_GOAL_POWER;

else if (!strcmp(target, "acpi"))
    J.goals |= JHW_GOAL_ACPI;

else if (!strcmp(target, "display"))
    J.goals |= JHW_GOAL_DISPLAY;

else
    return JHW_NOT_FOUND;

printf("goal enabled: %s\n", target);

return JHW_OK;
```

}

/* ============================================================

* DSL parser
* ============================================================ */

static void jhw_trim(char *s)
{
size_t n;

```
if (!s)
    return;

while (*s && isspace((unsigned char)*s))
    memmove(s, s + 1, strlen(s));

n = strlen(s);

while (n &&
       isspace((unsigned char)s[n - 1]))
    s[--n] = 0;
```

}

static JHWResult jhw_call(
const char *name
)
{
if (!strcmp(name, "system.info"))
return jhw_system_info();

```
if (!strcmp(name, "hardware.summary"))
    return jhw_hardware_summary();

if (!strcmp(name, "device.list"))
    return jhw_device_list();

if (!strcmp(name, "cpu.info"))
    return jhw_cpu_info();

if (!strcmp(name, "memory.info"))
    return jhw_memory_info();

if (!strcmp(name, "pci.list"))
    return jhw_pci_list();

if (!strcmp(name, "usb.list"))
    return jhw_usb_list();

if (!strcmp(name, "storage.list"))
    return jhw_storage_list();

if (!strcmp(name, "network.list"))
    return jhw_network_list();

if (!strcmp(name, "input.list"))
    return jhw_input_list();

if (!strcmp(name, "sensor.list"))
    return jhw_sensor_list();

if (!strcmp(name, "display.info"))
    return jhw_display_info();

if (!strcmp(name, "goal.info"))
{
    jhw_goal_print();
    return JHW_OK;
}

return JHW_NOT_FOUND;
```

}

/* ============================================================

* jhw(C code)
* ============================================================ */

static JHWResult jhw_execute(
const char *source
)
{
char *copy;
char *p;

```
if (!source)
    return JHW_INVALID_ARGUMENT;

copy = (char *)malloc(strlen(source) + 1);

if (!copy)
    return JHW_ERROR;

strcpy(copy, source);

p = copy;

while (*p)
{
    char *start;
    char *end;
    char command[512];

    while (*p &&
           (isspace((unsigned char)*p) ||
            *p == ';' ||
            *p == '(' ||
            *p == ')'))
        p++;

    if (!*p)
        break;

    start = p;

    while (*p && *p != ';')
        p++;

    end = p;

    if (*p)
        p++;

    *end = 0;

    jhw_trim(start);

    if (!*start)
        continue;

    /*
     * /goal is deliberately a JHW language command.
     */

    if (!strncmp(start, "/goal", 5))
    {
        char *args = start + 5;

        while (*args &&
               isspace((unsigned char)*args))
            args++;

        if (jhw_goal(args) != JHW_OK)
        {
            free(copy);
            return JHW_SYNTAX;
        }

        continue;
    }

    /*
     * Allow:
     *
     *   cpu.info()
     *   cpu.info
     *
     */

    jhw_copy(
        command,
        sizeof(command),
        start
    );

    {
        char *paren = strchr(command, '(');

        if (paren)
            *paren = 0;
    }

    jhw_trim(command);

    if (jhw_call(command) != JHW_OK)
    {
        fprintf(
            stderr,
            "JHW: unknown command: %s\n",
            command
        );

        free(copy);
        return JHW_NOT_FOUND;
    }
}

free(copy);

return JHW_OK;
```

}

/* ============================================================

* Interactive JHW
* ============================================================ */

static void jhw_help(void)
{
puts("");
puts("JHW - Jet Hardware");
puts("");
puts("Commands:");
puts("  /goal all");
puts("  /goal hardware 70");
puts("  /goal cpu");
puts("  /goal gpu");
puts("  /goal storage");
puts("  /goal network");
puts("  /goal usb");
puts("  /goal input");
puts("  /goal sensor");
puts("");
puts("  system.info()");
puts("  hardware.summary()");
puts("  device.list()");
puts("  cpu.info()");
puts("  memory.info()");
puts("  pci.list()");
puts("  usb.list()");
puts("  storage.list()");
puts("  network.list()");
puts("  input.list()");
puts("  sensor.list()");
puts("  display.info()");
puts("  goal.info()");
puts("");
}

static void jhw_repl(void)
{
char line[JHW_MAX_INPUT];

```
puts("JHW interactive runtime");
puts("Type 'help' or 'exit'.");

for (;;)
{
    printf("jhw> ");
    fflush(stdout);

    if (!fgets(
            line,
            sizeof(line),
            stdin))
        break;

    jhw_trim(line);

    if (!strcmp(line, "exit") ||
        !strcmp(line, "quit"))
        break;

    if (!strcmp(line, "help"))
    {
        jhw_help();
        continue;
    }

    if (!*line)
        continue;

    (void)jhw_execute(line);
}
```

}

/* ============================================================

* Main
* ============================================================ */

int main(
int argc,
char **argv
)
{
JHWResult result;

```
if (jhw_init() != JHW_OK)
    return 1;

if (argc == 1)
{
    jhw_help();
    jhw_system_info();
    jhw_hardware_summary();

    return 0;
}

if (!strcmp(argv[1], "--help"))
{
    jhw_help();
    return 0;
}

if (!strcmp(argv[1], "--version"))
{
    printf(
        "JHW %d.%d\n",
        JHW_VERSION_MAJOR,
        JHW_VERSION_MINOR
    );

    return 0;
}

if (!strcmp(argv[1], "--scan"))
{
    jhw_system_info();
    jhw_hardware_summary();
    jhw_device_list();

    return 0;
}

if (!strcmp(argv[1], "--repl"))
{
    jhw_repl();
    return 0;
}

/*
 * Everything after the executable is treated as
 * one JHW program.
 */

{
    size_t total = 1;
    int i;
    char *program;

    for (i = 1; i < argc; i++)
        total += strlen(argv[i]) + 2;

    program = (char *)malloc(total);

    if (!program)
        return 1;

    program[0] = 0;

    for (i = 1; i < argc; i++)
    {
        if (i > 1)
            strcat(program, " ");

        strcat(program, argv[i]);
    }

    result = jhw_execute(program);

    free(program);
}

return result == JHW_OK ? 0 : 1;
```

}
