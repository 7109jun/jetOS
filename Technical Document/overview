# JetOS API Documentation Overview

JetOS is a UEFI-based operating system written from scratch for x86-64.
It provides its own filesystem (JETFS), GUI window manager, networking stack, and multiple layers of APIs that allow applications to access system services.

This documentation set describes all programming interfaces exposed by JetOS.

## API Layer Architecture

Depending on the type of application, a different API layer is used.

```text
┌─────────────────────────────────────────────────────────┐
│  PE32+ (.exe) binaries built with actual Windows         │
│  compilers (MinGW, etc.)                                 │
│  → Win32 / MSVCRT compatibility layer                    │
├─────────────────────────────────────────────────────────┤
│  JetOS native applications                               │
│  (jash, notepad, Task Manager, etc.)                     │
│  → JetAPI (Native)                                       │
├─────────────────────────────────────────────────────────┤
│  JetOS native ELF64 programs                             │
│  (such as programs produced by jcc/jas)                  │
│  → Direct raw syscall (INT 0x80)                         │
├─────────────────────────────────────────────────────────┤
│              JetOS Kernel                                │
│      (JETFS, GUI, networking, scheduler)                 │
└─────────────────────────────────────────────────────────┘
```

## Documentation List

| Document                              | Description                                                                                                     |
| ------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `01_JetAPI_Reference.md`              | JetOS-specific native API — windows, files, memory, threads, and system information                             |
| `02_Win32_MSVCRT_Compat_Reference.md` | KERNEL32/MSVCRT compatibility layer for running PE32+ executables built with actual Windows-targeting compilers |
| `03_JETFS_Syscall_Reference.md`       | JETFS API + raw syscall ABI for native ELF64 programs                                                           |
| `04_jash_Shell_Reference.md`          | Complete command reference for the built-in shell (jash)                                                        |

## Design Philosophy

* **Everything is implemented from scratch**: The Win32/MSVCRT compatibility layer follows an approach similar to Wine or ReactOS. It does not use Microsoft's source code or DLLs. Instead, it reimplements the API surface expected by Windows applications from scratch based on observed behavior.

* **Honest documentation of limitations**: The project documents the verification status of each feature — whether it has been actually tested in QEMU or only confirmed to compile — and records known limitations in the source-code comments. These documents follow the same principle: anything that is not supported is explicitly marked as **"Not Supported."**

* **Real-system verification**: Most APIs have been verified by booting JetOS in the QEMU emulator and checking their execution results through the serial log. Items marked **"Verified in QEMU"** have been confirmed to work through this process. Items without this designation may compile successfully but have not been directly verified in QEMU.

## Quick Start Examples

### JetOS Native Application (C, using JetAPI)

```c
#include "jetapi/jetapi.h"

void my_app(void) {
    JetWindowHandle win = CreateWindowJ("My App", 100, 100, 400, 300, 0x00303040);
    WriteFileJ("/myapp_log.txt", "started\n", 8);
}
```

### Console Program Built with an Actual Windows Compiler

```c
#include <stdio.h>

int main(void) {
    printf("Hello from a real Win32 program on JetOS!\n");
    return 0;
}
```

Compile it normally with MinGW:

```bash
x86_64-w64-mingw32-gcc -O2 -o hello.exe hello.c
```

Then place the resulting executable into JETFS and run it from jash:

```text
run hello.exe
```

It runs directly on JetOS, including CRT initialization, `printf`, and normal process termination.

See `02_Win32_MSVCRT_Compat_Reference.md` for details.
