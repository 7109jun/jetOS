# JetOS Development Status

> Last updated: Milestone 7 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Total source: 106 files, pure C (assembly isolated to a single HAL shim file)

---

## Philosophy

> **"An OS for people who want to use Windows but cannot afford it."**

JetOS is not identical to Windows, but it is designed to feel familiar from the moment it boots.

It is a completely independent and free OS with no external Windows/Linux code, DLLs, or SDK dependencies. Everything is implemented directly by JetOS.

---

## Directory Structure

JetOS/
├── boot/                    UEFI bootloader (mingw-w64, PE32+)
│   ├── bootloader.c         UEFI efi_main → loads KERNEL.ELF → jumps to kernel
│   ├── elf_loader.c         ELF64 section loader
│   ├── elf64.h              ELF64 structure definitions
│   └── freestanding_libc.c  memcpy/memset
│
├── kernel/
│   ├── kernel_entry.c       kernel_entry() — M1–M7 initialization
│   ├── kernel.h             global console
│   ├── panic.c/h            kernel panic
│   ├── freestanding_libc.c  kernel memcpy/memset
│   │
│   ├── hal/
│   │   ├── x86_asm_shim.h   only inline assembly isolation file
│   │   └── context_switch.c thread context switching
│   │
│   ├── mm/
│   │   ├── memmap.c/h       UEFI memory map
│   │   ├── pmm.c/h          physical memory manager
│   │   ├── vmm.c/h          virtual memory manager
│   │   └── heap.c/h         kernel heap
│   │
│   ├── int/
│   │   ├── idt.c/h          IDT
│   │   ├── exceptions.c/h   exception handlers
│   │   ├── pic.c/h          PIC
│   │   └── pit.c/h          PIT timer
│   │
│   ├── drivers/
│   │   ├── serial.c/h       COM1
│   │   ├── console.c/h      GOP framebuffer console
│   │   ├── font5x7.h        bitmap font
│   │   ├── keyboard.c/h     PS/2 keyboard
│   │   ├── mouse.c/h        PS/2 mouse
│   │   ├── pci.c/h          PCI enumeration
│   │   └── ahci.c/h         AHCI/SATA
│   │
│   ├── proc/
│   │   ├── thread.c/h       threads
│   │   ├── scheduler.c/h    round-robin scheduler
│   │   ├── process.c/h      processes
│   │   └── ipc.c/h          IPC mailbox
│   │
│   ├── fs/
│   │   ├── jetfs.c/h        JETFS v1
│   │   └── vfs.c/h          VFS
│   │
│   ├── gui/
│   │   ├── theme.h          design system
│   │   ├── wm.c/h           window manager
│   │   └── desktop.c/h      desktop shell
│   │
│   ├── jetapi/
│   │   └── jetapi.c         JetAPI
│   │
│   ├── compat/
│   │   └── win32.c          Win32 compatibility layer
│   │
│   ├── apps/
│   │   ├── file_manager.c/h File Manager
│   │   ├── settings.c/h     Settings
│   │   ├── task_manager.c/h Task Manager
│   │   └── jash.c/h         jash shell
│   │
│   ├── exec/
│   │   └── pe_loader.c/h    PE32+ loader
│   │
│   └── shell/
│       └── terminal.c/h     JETFS demo
│
├── include/
│   ├── efi/efi.h            UEFI definitions
│   └── jetapi/jetapi.h      JetAPI public header
│
├── tools/
│   └── mkjetfs.c            JETFS formatter
│
├── scripts/
│   ├── setup_toolchain.sh
│   ├── make_esp.sh
│   └── run_qemu.sh
│
└── Makefile

---

# Milestone Status

## Milestone 1 — Boot / Kernel Entry

Status: COMPLETE

- UEFI → ExitBootServices
- GOP framebuffer
- COM1 serial logging
- 5x7 bitmap font
- IDT
- Exception handlers
- UEFI memory map parsing
- QEMU real-boot verification

---

## Milestone 2 — Memory / Bootloader-Kernel Separation

Status: COMPLETE

- ELF64 kernel loader
- Bootloader/kernel ABI boundary
- Physical memory manager
- Virtual memory manager
- Kernel heap
- PIC remapping
- PIT 100 Hz timer
- QEMU real-boot verification

Fixed bug:

- Missing "memory" clobber in LIDT caused optimization to remove the IDTR assignment and resulted in triple faults.

---

## Milestone 3 — Process / Thread / Scheduler / IPC

Status: COMPLETE

- Thread creation/termination
- Independent 16 KiB stacks
- Round-robin scheduler
- Preemptive scheduling
- Cooperative yield
- Process containers
- IPC mailbox
- Context switching

Known limitation:

- Terminated thread stacks are not reclaimed.
- No zombie reaper.

---

## Milestone 4 — Storage / Filesystem / Terminal

Status: COMPLETE

- PCI enumeration
- AHCI/SATA driver
- JETFS v1
- Superblock
- Inode table
- Data bitmap
- Directory system
- File create/read/write/delete/rename
- Logical namespaces: :> :] :) :< :/
- mkjetfs host tool
- Boot-time filesystem demo

Known limitations:

- Maximum file size is 48 KiB.
- No indirect blocks.
- Deleted file data blocks are not reclaimed.

---

## Milestone 5 — GUI / Keyboard / Mouse / Window Manager

Status: COMPLETE

- PS/2 keyboard
- PS/2 mouse
- Scancode → ASCII
- Shift support
- Keyboard ring buffer
- Mouse packet handling
- Window manager
- Z-order
- Title-bar dragging
- Double buffering
- Taskbar
- Start menu
- Notepad
- Shutdown
- Custom arrow cursor

Fixed bugs:

- PIC2 cascade masking
- Mouse initialization race condition
- Screen tearing

---

## Milestone 6 — Applications / JetAPI / Win32 Compatibility

Status: COMPLETE

JetAPI:

- CreateWindowJ
- ReadFileJ
- VirtualAllocJ
- CreateThreadJ
- GetSystemInfoJ
- ListThreadsJ

Win32 compatibility:

- CreateWindowA
- CreateFileA
- ReadFile
- WriteFile
- VirtualAlloc
- ExitProcess

Applications:

- File Manager
- Settings
- Task Manager
- Notepad

Known limitations:

- VirtualAllocJ is currently a kmalloc wrapper.
- No per-process address spaces.
- Win32 compatibility is currently a simplified KERNEL32-style stub layer.

---

## Milestone 7 — Design Refresh / jash / PE32+ Loader

Status: COMPLETE

### GUI

- Dark blue-gray theme
- Blue accents
- Gradient title bars
- 2px window borders
- Red X button
- Improved taskbar
- Active-window highlighting
- Start menu separator
- Pixel-art arrow cursor

### jash

JetOS-specific shell based on the original Jash.cs design and reimplemented in C.

Implemented:

- help
- mem info
- mem list
- base64 encode
- base64 decode
- browse
- run

### PE32+ Loader

Implemented:

- MZ header parsing
- PE header parsing
- Section loading
- Relocation processing
- IAT linking
- PE32+ execution path
- jash run

### Keyboard Routing

IRQ1
→ keyboard_irq_handler()
→ g_ring
→ desktop_thread_fn()
→ keyboard_poll_char()
→ jash_inject_char()
→ g_inject_buf
→ jash_thread_fn()
→ jash_run_once()

Only one thread consumes keyboard_poll_char(), preventing keyboard-input race conditions.

---

# Current Working Features

## Hardware / Boot

- UEFI boot
- QEMU + OVMF
- Real PC USB boot
- GOP framebuffer
- COM1 serial debugging
- PS/2 keyboard
- PS/2 mouse
- SATA/AHCI disk I/O

## Kernel

- Physical memory management
- Virtual memory
- Kernel heap
- 100 Hz timer
- Preemptive multithreading
- IPC mailbox
- Thread scheduler

## Filesystem

- JETFS v1
- Files
- Directories
- CRUD
- Logical namespaces
- mkjetfs

## GUI

- Window manager
- Double buffering
- Window dragging
- Z-order
- Taskbar
- Start menu
- Notepad
- File Manager
- Settings
- Task Manager
- Custom cursor

## Shell

- jash
- help
- mem
- base64
- browse
- run

## API

- JetAPI
- Win32 compatibility layer
- PE32+ loader

---

# Known Limitations

## Core

- No Ring3 user mode
- No process isolation
- No per-process virtual address spaces
- No networking
- No audio
- No Korean/Unicode support
- JETFS maximum file size: 48 KiB
- Deleted JETFS blocks are not reclaimed
- Zombie thread stacks are not reclaimed
- PE loader has no CRT support
- EXEs currently execute with kernel-level privileges

## GUI

- X button is currently visual only
- No minimize/maximize
- No scrollbars
- ASCII-only font
- No Korean/Unicode
- jash has limited scrolling
- Keyboard focus routing is simplified

---

# Build Environment

Install:

sudo apt install mingw-w64 gcc qemu-system-x86 ovmf mtools dosfstools make

Build:

make

Create JETFS:

make mkjetfs
./build/mkjetfs build/jetfs.img 16

Create ESP and run:

make esp
./scripts/run_qemu.sh

Build outputs:

- build/BOOTX64.EFI
- build/KERNEL.ELF
- build/esp.img
- build/jetfs.img

---

# Next Development Priorities

## 1. jash

- cd
- ls
- mkdir
- cat
- write
- Arrow-key cursor movement
- Command history
- PgUp/PgDn scrolling
- Proper keyboard focus routing

## 2. GUI

- Functional X button
- Minimize/restore
- Context menus
- Desktop icons
- Notepad save/load using JETFS

## 3. PE32+

- More Win32 API stubs
- MessageBox
- GetFileSize
- CRT-free Hello World EXE
- Better missing-import error messages

## 4. JETFS

- Indirect blocks
- Remove 48 KiB file limit
- Reclaim deleted data blocks
- Zombie thread reaper

## 5. Networking

- RTL8139 or virtio-net
- ARP
- IP
- TCP
- HTTP GET
- jash browse <URL>

## 6. User Mode

- Ring3
- User-mode stack
- System calls
- Per-process page tables
- True process isolation
- Ring3 PE execution

---

# Key Design Decisions

## Assembly Policy

Inline assembly is isolated to only two files:

1. kernel/hal/x86_asm_shim.h
   - Port I/O
   - LIDT
   - HLT
   - CLI/STI
   - CR2/CR3/CS access

2. kernel/hal/context_switch.c
   - Thread context switching
   - RSP replacement

Everything else is written in pure C.

## Bootloader

x86_64-w64-mingw32-gcc is used because UEFI x86_64 applications require:

- PE32+
- MS x64 calling convention

This allows the bootloader to be built directly without a separate linker script or objcopy step.

## ABI Boundary

UEFI Bootloader
→ MS x64 ABI
→ sysv_abi function pointer
→ JetOS Kernel

There is only one ABI boundary between the bootloader and kernel.

## JETFS Layout

Block 0
→ Superblock

Block 1..N
→ Inode table

Block N+1..M
→ Data block bitmap

Block M+1..end
→ Data blocks

Block size: 4096 bytes.

The host-side mkjetfs tool directly reuses kernel/fs/jetfs.h, ensuring that the host formatter and kernel use the same on-disk structures.

---

# JetOS Development Philosophy

JetOS should remain:

- Lightweight
- Independent
- Free
- Familiar to Windows users
- Mostly pure C
- Simple enough to understand
- Extensible
- Practical

Do not turn JetOS into a Windows clone.

The goal is to create an independent OS that feels familiar, not to reproduce Windows internally.
