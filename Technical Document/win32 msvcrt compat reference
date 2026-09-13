# Win32 / MSVCRT Compatibility Layer Reference

Headers: `kernel/compat/win32.h`, `kernel/compat/msvcrt.h`
Loader: `kernel/exec/pe_loader.c`

## Overview

This compatibility layer allows **PE32+ (.exe) executables built with actual Windows-targeting compilers such as mingw-w64** to run directly on JetOS.

**Important:** Microsoft `Windows.h`, the Windows SDK, and Windows DLLs are not used at all. Every type and function described below is implemented from scratch by JetOS based only on the function names and their observed behavior. This follows an approach similar to Wine/ReactOS.

Function names in the PE executable's Import Address Table (IAT) are resolved and connected to implementations provided by this compatibility layer.

### Calling Convention (Critical)

PE32+ executables use the Microsoft x64 calling convention, passing arguments in `RCX`, `RDX`, `R8`, and `R9`.

However, the JetOS kernel itself is compiled using the System V AMD64 calling convention, which passes arguments in `RDI`, `RSI`, `RDX`, and `RCX`.

Therefore, **every function directly called by an EXE through the IAT must be declared with `WINAPI` (`__attribute__((ms_abi))`)**.

Otherwise, the implementation will read arguments from the wrong registers, causing effectively random behavior.

### Supported Executable Conditions

| Condition                                                                                    | Status                                                     |
| -------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| PE32+ (x64), with ASLR disabled or with relocation tables                                    | Supported                                                  |
| Image base address within the first 4 GiB                                                    | Supported                                                  |
| Uses only KERNEL32.dll functions (no CRT, `-nostdlib`)                                       | **Verified in QEMU**                                       |
| Normal CRT (`mainCRTStartup`) + MSVCRT functions such as `printf`/`malloc`                   | **Verified in QEMU**                                       |
| Actual file I/O (`fopen`/`fread`/`fwrite`)                                                   | **Verified in QEMU**                                       |
| Command-line arguments (`argc`/`argv`, `GetCommandLineA`)                                    | **Verified in QEMU**                                       |
| `atexit`/`_onexit` cleanup handlers                                                          | **Verified in QEMU**                                       |
| `CreateThread` (actual multithreading)                                                       | Implemented but **not verified** — known issue (see below) |
| Floating-point `printf("%f", ...)`                                                           | **Not Supported**                                          |
| C++ (`new`/`delete`, exceptions, RTTI, `iostream`)                                           | **Not Supported**                                          |
| GUI APIs (User32/GDI, `CreateWindowA` is only a minimal stub)                                | **Not Supported** (stub level)                             |
| Distinguishing multiple DLL names (all imports are resolved from a single stub pool by name) | **Not Supported**                                          |

---

## KERNEL32 Compatibility (`win32.h`)

### Console I/O

```c
#define STD_INPUT_HANDLE  ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE  ((DWORD)-12)

HANDLE GetStdHandle(DWORD std_handle);
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes_to_write, DWORD *bytes_written, void *overlapped);
BOOL WriteConsoleA(HANDLE console, LPCVOID buffer, DWORD chars_to_write, DWORD *chars_written, void *reserved);
BOOL ReadConsoleA(HANDLE console, LPVOID buffer, DWORD chars_to_read, DWORD *chars_read, void *input_control);
```

`GetStdHandle` returns distinct handles for standard input, standard output, and standard error.

Writing to these handles through `WriteFile` or `WriteConsoleA` outputs the data to the **serial port**.

This is the current implementation. Direct connection to the jash window is planned for a future implementation.

`ReadConsoleA` reads one line from the PS/2 keyboard ring buffer and blocks until Enter is pressed.

### File Operations

```c
HANDLE CreateFileA(LPCSTR path, DWORD desired_access, DWORD share_mode,
                    void *security_attrs, DWORD creation_disposition,
                    DWORD flags, HANDLE template_file);

BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD bytes_to_read,
              DWORD *bytes_read, void *overlapped);

BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes_to_write,
               DWORD *bytes_written, void *overlapped);

BOOL CloseHandle(HANDLE h);
DWORD GetFileSize(HANDLE file, DWORD *high);
BOOL DeleteFileA(LPCSTR path);
```

Only JETFS paths are supported. Other filesystems such as FAT32 are outside the scope of this layer.

Internally, these functions delegate to JetAPI functions such as `CreateFileJ` and `WriteFileJ`.

> **Note:** Programs using standard C stream APIs such as `fopen`, `fread`, and `fwrite` do not use this `CreateFileA` implementation. They use the `fopen` family described in the MSVCRT section below. These are separate internal implementations, and both operate by loading the entire file into memory.

### Memory

```c
LPVOID VirtualAlloc(LPVOID addr, uint32_t size, DWORD alloc_type, DWORD protect);
BOOL VirtualFree(LPVOID addr, uint32_t size, DWORD free_type);
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE heap, DWORD flags, uint32_t size);   /* HEAP_ZERO_MEMORY supported */
BOOL HeapFree(HANDLE heap, DWORD flags, LPVOID mem);
```

All of these are thin wrappers around the kernel heap (`kmalloc`/`kfree`).

Multiple heap objects are not managed. The handle returned by `GetProcessHeap()` is simply a unique dummy value representing the single available process heap.

### Process / Module / Command Line

```c
LPCSTR GetCommandLineA(void);
HANDLE GetModuleHandleA(LPCSTR module_name);
void *GetProcAddress(HANDLE module, LPCSTR proc_name);
__attribute__((noreturn)) void ExitProcess(DWORD exit_code);
DWORD GetTickCount(void);
```

`GetCommandLineA` and `__getmainargs` on the MSVCRT side return the **actual command-line arguments**.

If a program is launched using:

```text
run <path> <args...>
```

from jash, those arguments are passed directly into `argv`.

Arguments are split by spaces only. Quoted arguments are not currently supported.

`GetProcAddress` searches only the internal small function table. It cannot resolve every possible imported function.

### Threads

**Not verified in QEMU — see "Known Issue" below.**

```c
HANDLE CreateThread(void *sec_attrs, uint32_t stack_size, void *start_addr,
                     LPVOID param, DWORD creation_flags, DWORD *thread_id);

__attribute__((noreturn)) void ExitThread(DWORD exit_code);

DWORD WaitForSingleObject(HANDLE h, DWORD timeout_ms);  /* INFINITE supported */

DWORD GetCurrentThreadId(void);
```

These functions are implemented as thin wrappers around JetOS kernel threads.

The `HANDLE` returned by `CreateThread` is actually the corresponding internal `thread_t*` cast to a handle value.

> ⚠️ **Known Issue:** During boot self-testing, a reproducible issue occurs when a PE program using `CreateThread` is executed as the sixth PE program after five other PE programs have been executed consecutively. The program produces no output at all.
>
> The scheduler's thread queue was directly confirmed to be functioning correctly through diagnostic tracing. The root cause has not yet been identified.
>
> Programs using this API must therefore be independently verified before being relied upon.

### Miscellaneous

```c
char *lstrcpyA(char *dst, LPCSTR src);
char *lstrcatA(char *dst, LPCSTR src);
int lstrlenA(LPCSTR s);

void Sleep(DWORD milliseconds);

void SetLastError(DWORD err);
DWORD GetLastError(void);

void OutputDebugStringA(LPCSTR msg);   /* Outputs to serial */
BOOL IsDebuggerPresent(void);          /* Always FALSE */

int MessageBoxA(HWND owner, LPCSTR text, LPCSTR title, DWORD type);

HWND CreateWindowA(LPCSTR title, int x, int y, int w, int h,
                   DWORD style_placeholder);
```

`CreateWindowA` and `MessageBoxA` are currently implemented only at a minimal stub level.

This is the seed-stage implementation for Milestone 6. Actual User32/GDI rendering is not supported.

---

## MSVCRT Compatibility (`msvcrt.h`) — C Runtime

Normally compiled MinGW programs, without `-nostdlib`, are supported through the complete flow in which `mainCRTStartup` initializes the runtime and eventually calls `main()`.

### CRT Initialization (Internal — Not Normally Called Directly)

```c
int __getmainargs(int *argc, char ***argv, char ***envp,
                  int do_wildcard, void *startinfo);

extern char **g_win32_initenv;   /* Target data symbol for __initenv */

void _initterm(void **begin, void **end);

int __set_app_type(int type);

int *_commode(void);
int *_fmode(void);
int *_errno(void);
```

### Standard I/O

```c
FILE *__iob_func(int index);   /* 0=stdin, 1=stdout, 2=stderr */

int fputc(int c, FILE *stream);

uint32_t fwrite(const void *ptr, uint32_t size,
                uint32_t count, FILE *stream);

int fprintf(FILE *stream, const char *fmt, ...);

int vfprintf(FILE *stream, const char *fmt, va_list args);
```

**Supported `printf` format specifiers:**

`%d`, `%i`, `%u`, `%x`, `%X`, `%o`, `%c`, `%s`, `%p`, `%%`

Length modifiers:

* `l`
* `ll`

The Windows LLP64 convention is used:

* `%ld` → 4 bytes
* `%lld` → 8 bytes

Width and zero-padding are supported:

* `%5d`
* `%02d`

**Not supported:**

* Precision (`.2f`, etc.)
* Left alignment (`-`)
* Floating-point formats:

  * `%f`
  * `%e`
  * `%g`

Writing to console streams (`stdout`/`stderr`) outputs to the serial port.

Writing to an actual file stream is routed through the file-stream implementation described below.

### Actual File Streams — **Verified in QEMU**

```c
FILE *fopen(LPCSTR path, LPCSTR mode);   /* "r", "w", "a" supported */

int fclose(FILE *stream);

uint32_t fread(void *ptr, uint32_t size,
                uint32_t count, FILE *stream);

int feof(FILE *stream);

char *fgets(char *buf, int n, FILE *stream);

int fputs(const char *s, FILE *stream);

int fflush(FILE *stream);
```

Paths use the JETFS format, such as `/foo.txt`.

When a file is opened, the **entire file is loaded into memory**, and when the stream is closed or flushed, the **entire file is written back**.

This is necessary because JETFS currently does not provide a streaming API.

As a result, this implementation is not suitable for very large files.

Update modes such as `"r+"` and `"w+"` are not supported.

Verified example from an actual QEMU execution:

```c
FILE *f = fopen("/out.txt", "w");
fputs("line one\n", f);
fprintf(f, "the answer is %d\n", 42);
fclose(f);

/* Reading the file again produces exactly two lines. */
```

### Memory / String Operations

```c
void *malloc(uint32_t size);
void *calloc(uint32_t count, uint32_t size);
void free(void *p);

void *msvcrt_memcpy(void *dst, const void *src, uint32_t n);
/* Connected to "memcpy" in the IAT */

void *msvcrt_memset(void *dst, int val, uint32_t n);
/* Connected to "memset" in the IAT */

uint32_t strlen(const char *s);
int strncmp(const char *a, const char *b, uint32_t n);
uint32_t wcslen(const uint16_t *s);
```

### Program Termination / `atexit` — **Verified in QEMU**

```c
void exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

void *_onexit(void *fn);   /* atexit() internally calls this */

void _cexit(void);
```

`exit()` executes all registered `atexit`/`_onexit` handlers in **reverse registration order (LIFO)** before terminating.

The execution order was also verified directly in QEMU.

### Thread Synchronization (Single-Thread Assumption — No-Op)

```c
void InitializeCriticalSection(void *cs);
void EnterCriticalSection(void *cs);
void LeaveCriticalSection(void *cs);
void DeleteCriticalSection(void *cs);

void *TlsGetValue(DWORD index);   /* Always NULL — TLS slots not supported */
```

Critical-section functions are currently no-ops under the single-thread assumption.

Thread-local storage (TLS) slots are not currently implemented.

### Unicode Conversion (ASCII Range Only)

```c
int MultiByteToWideChar(
    int codepage,
    DWORD flags,
    LPCSTR src,
    int src_len,
    uint16_t *dst,
    int dst_len
);

int WideCharToMultiByte(
    int codepage,
    DWORD flags,
    const uint16_t *src,
    int src_len,
    char *dst,
    int dst_len,
    LPCSTR default_char,
    BOOL *used_default
);
```

These functions do not perform real code-page conversion.

Instead, bytes are simply zero-extended or truncated.

Characters outside the range `0–127` (ASCII) are therefore not converted correctly.

### Exception Handling (SEH) — **Not Implemented**

```c
void *__C_specific_handler(void);
```

This implementation does not actually unwind or handle Structured Exception Handling (SEH).

Programs following a straight-line execution path without exceptions often never call this function, so they may run normally.

However, code that actually triggers an exception, such as division by zero, is outside the supported execution model.

---

## Running PE32+ Programs

```text
jash /> run myprogram.exe arg1 arg2
```

Internally, `pe_load()` parses the PE headers and connects IAT entries to functions provided by this compatibility layer.

`pe_execute()` then executes the program in a new kernel thread.

After the `run` command is executed, jash prints the names of imported functions that could not be resolved normally and were therefore replaced with stubs.

This means that if the program actually calls one of those unresolved functions during execution, it may do nothing or otherwise fail to provide the expected behavior.

---

## Build Examples (For Developers)

### Minimal Program Without CRT

```bash
x86_64-w64-mingw32-gcc -nostdlib -Wl,--entry=_start \
    -Wl,--subsystem,console -o hello.exe hello.c -lkernel32
```

### Normal CRT-Based Program

Programs using `printf`, `fopen`, and other supported CRT functionality can be built normally:

```bash
x86_64-w64-mingw32-gcc -O2 -o hello.exe hello.c
```
