# JetOS Development Status

> Last updated: Milestone 18 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation/linking + **actual QEMU boot verification with serial logs/screenshots** (QEMU was installed in the sandbox starting from Milestone 10, making real-boot verification possible; every milestone since then has been verified through actual booting)
>
> This document + the latest tar file (`JetOS_Milestone18.tar`) should be sufficient to fully understand the current state of the project.

---

## Philosophy

> **"An OS for people who want to use Windows but cannot afford it."**

JetOS is an independent, free OS that is not identical to Windows, but should feel familiar from the moment it starts.

No dependency on code, DLLs, or SDKs from external operating systems such as Windows/Linux. Everything is implemented directly by JetOS.

Starting with Milestone 15, the goal shifted from being a **"good-looking OS"** to being a **real OS**. Linux's core design concepts (per-process address spaces, ELF execution, fault isolation) were referenced for implementation (open-source concepts were referenced; no code was copied, so there are no licensing issues).

---

## Build & Run

```bash
# Install environment (Debian/Ubuntu)
sudo apt install mingw-w64 qemu-system-x86 ovmf mtools dosfstools make gcc

# Build
make                                    # bootloader (mingw) + kernel (native gcc)
make mkjetfs && ./build/mkjetfs build/jetfs.img 16   # JETFS data disk
make esp                                # Create ESP (FAT32) image
./scripts/run_qemu.sh                   # Boot with QEMU+OVMF (includes -netdev user -device rtl8139 by default)

# Prepare test executables (optional)
./scripts/build_test_exe.sh             # PE32+ hello.exe (Win32 MessageBox test, Milestone 9)
./scripts/build_test_native_elf.sh      # Native ELF variants (normal/crash tests, Milestones 15/16)
```

Output:

- `build/BOOTX64.EFI`
- `build/KERNEL.ELF`
- `build/esp.img`
- `build/jetfs.img`

### Verification Methodology

1. **Host harness testing**: For hardware-independent or mostly hardware-independent logic such as `kernel/fs/jetfs.c`, `kernel/exec/pe_loader.c`, and `kernel/exec/elf_loader.c`, the lowest-level I/O functions such as `ahci_read/write_sectors` and `jetfs_read` are replaced with stubs. This allows the code to be compiled directly with normal Linux gcc and compared against actual file/network captures. This method caught two use-after-free bugs (PE loader and ELF loader — both involved referencing pointers after `kfree()`).

2. **Actual QEMU boot + serial logs**: Boot logs are collected using `-serial stdio` or `-serial file:...`. `STAGE:` and `=== ... SELF-TEST ===` markers are used to verify the success of each stage. The kernel contains built-in boot-time self-tests for each subsystem (`NET/TCP/DNS/RING3 self-test`), providing automatic regression testing every time a new image is built.

3. **QEMU monitor + screendump**: The GUI is launched using `-monitor unix:...,server,nowait -display none`, then a PPM screenshot is captured through the monitor socket using the `screendump` command, converted to PNG, and visually inspected.

**Important:** In this sandbox environment, a QEMU background process dies when the tool call finishes. Therefore, "boot → interact → capture → shutdown" must all be performed within a single bash invocation.

Also, the ratio between `mouse_move` deltas and actual pixel movement is not constant, making precise automated coordinate clicking unreliable. Static screen capture verification (such as the desktop immediately after boot) should therefore be preferred.

---

## Directory Structure (Major Components, Milestone 18)

```text
JetOS/
├── boot/                     UEFI bootloader (mingw-w64)
├── kernel/
│   ├── kernel_entry.c        Overall initialization sequence + boot self-test coordinator
│   ├── hal/
│   │   ├── x86_asm_shim.h   ★ Assembly isolation file 1 (port I/O, LGDT/LIDT,
│   │   │                        segment reloads, Ring3 entry (iretq),
│   │   │                        system-call trap (int $0x80), etc.)
│   │   ├── context_switch.c ★ Assembly isolation file 2
│   │   │                        (thread context switching, naked function)
│   │   ├── syscall_entry.c   ★ Assembly isolation file 3 (Milestone 14,
│   │   │                        system-call entry trampoline)
│   │   ├── gdt.c/h           GDT+TSS setup (Milestone 14, kernel/user segments for Ring3)
│   │   ├── syscall.c/h       System-call dispatcher (pure C)
│   │   └── ring3_test.c/h    Initial Ring3 test from M14 (currently unused —
│   │                            see "Known Issues" below)
│   ├── mm/
│   │   ├── pmm.c/h, heap.c/h Existing components
│   │   └── vmm.c/h            Milestone 16: kernel identity mapping (no U bit) +
│   │                            per-process PML4/PDPT/PD/PT
│   │                            (U bit only enabled for user space)
│   ├── int/                  IDT/PIC/PIT + exception handlers
│   │                         (Milestone 16: Ring3 faults terminate only the process,
│   │                          while Ring0 faults cause a full kernel panic)
│   ├── drivers/
│   │   ├── keyboard.c/h      Milestone 8: extended scancodes (arrow keys, etc.), F5/F6
│   │   ├── console.c/h       Milestone 13: UTF-8 decoding + Hangul routing
│   │   ├── utf8.c/h          Milestone 13: UTF-8 decoder
│   │   ├── hangul_font.c/h   Milestone 13: stroke-based Hangul syllable renderer
│   │   ├── rtc.c/h           Milestone 13: CMOS RTC (for clock app)
│   │   ├── pci.c/h, ahci.c/h Existing components
│   │   └── rtl8139.c/h       Milestone 10: NIC driver
│   │                            (polling-based, including empirically fixed bugs)
│   ├── net/                  Milestones 10–12
│   │   ├── net.c/h           Ethernet+ARP+IPv4+ICMP, net_ip_send() low-level hook
│   │   ├── tcp.c/h           Minimal TCP client (handshake/transmit/receive/close)
│   │   ├── udp.c/h           Minimal UDP
│   │   └── dns.c/h           Minimal DNS client (A records only)
│   ├── fs/jetfs.c/h, vfs.c/h Milestone 8: indirect blocks
│   │                         (up to ~4.2 MB), block reclamation
│   ├── gui/
│   │   ├── wm.c/h             Milestones 13/17/18: Win32-style windows,
│   │   │                        context menus, desktop icons, wallpaper,
│   │   │                        window resizing, minimize/maximize buttons
│   │   ├── theme.c/h          Milestone 18: runtime accent-color presets (4)
│   │   └── desktop.c/h        Event loop + icon/button click routing
│   ├── jetapi/, compat/win32.c Milestone 9: WINAPI(ms_abi) calling convention fix
│   ├── apps/
│   │   ├── jash.c/h           jash shell — cd/ls/mkdir/cat/write/rm,
│   │   │                        net/arp/ping/http/nslookup/browser/run
│   │   │                        (automatic ELF+PE detection), history, arrow keys
│   │   ├── file_manager.c/h  Milestone 13: click-based directory navigation
│   │   ├── settings.c/h      Milestone 18: actually clickable theme swatches
│   │   ├── clock.c/h         Milestone 13: real-time clock (RTC-based)
│   │   ├── browser.c/h       Milestone 18: GUI web browser
│   │   │                        (real TCP/DNS usage)
│   │   └── task_manager.c/h  Existing component
│   └── exec/
│       ├── pe_loader.c/h      Milestone 9: PE32+ loader
│       │                        (bugs fixed, execution verified)
│       └── elf_loader.c/h     Milestones 15/16: native ELF64 loader
│                                (per-process address spaces)
├── tools/
│   ├── mkjetfs.c              JETFS format tool
│   ├── jetfs_import.c         Milestone 9: host file → JETFS image importer
│   └── testdata/              Test hello.exe/ELF sources + linker scripts
└── scripts/
    ├── setup_toolchain.sh, make_esp.sh, run_qemu.sh
    ├── build_test_exe.sh          Milestone 9
    └── build_test_native_elf.sh   Milestones 15/16
```

---

## Milestone Summary

| # | Title | Core Contents |
|---|---|---|
| 1 | Boot/Kernel Entry | UEFI → ExitBootServices, GOP, serial, IDT, 5x7 font |
| 2 | Memory | ELF64 kernel loader, PMM/VMM/heap, PIC+PIT |
| 3 | Process/Thread/Scheduler/IPC | Round-robin scheduler, threads, mailboxes |
| 4 | Storage/FS/Terminal | PCI+AHCI, JETFS v1, VFS namespace |
| 5 | GUI/Keyboard/Mouse/WM | PS/2 drivers, window manager, double buffering |
| 6 | Apps/JetAPI/Win32 seed | JetAPI, Win32 compatibility layer seed, initial 3 apps |
| 7 | Design Renewal/jash/PE Loader | Theme system, initial jash shell, PE32+ parsing |
| 7.5 | jash Enhancement | cd/ls/mkdir/cat/write/rm, history, arrow keys, functional close/minimize buttons, JETFS block reclamation |
| 8 | FS Expansion + GUI Polish | JETFS indirect blocks (48 KB → 4.2 MB), right-click context menu, Notepad F5/F6 save |
| 9 | Real `.exe` Execution | **Bugs discovered**: PE loader use-after-free and Win32 ABI mismatch (SysV ↔ MS x64). Both fixed and actual EXE execution verified. `jetfs_import` tool introduced |
| 10 | Basic Networking | RTL8139 driver, ARP/IPv4/ICMP, `jash net/arp/ping`. Actual ARP+ping round trip verified through QEMU SLIRP gateway |
| 11 | TCP + Real HTTP | **3 bugs discovered/fixed**: scheduler context loss when `scheduler_yield()` was called before `scheduler_start()`, incorrect packet offset due to missing RTL8139 Ethernet minimum-frame padding handling, and TCP receive data loss because the receive buffer was enabled only when called |
| 12 | DNS | Minimal DNS client, `jash nslookup`, hostname support for `http`. Parser pre-verified using actual DNS server response captures |
| 13 | 3 Built-in Apps + Unicode | Clock (CMOS RTC), clickable file manager, persistent Notepad. UTF-8 decoder + stroke-based Hangul renderer using initial consonants (19) + medial vowels (21) instead of all 11,172 precomposed syllables |
| 14 | Ring3 + System Calls | Custom GDT/TSS, Ring3 entry through `iretq`, `int $0x80` system-call gate. Actual CPL=3 code execution + system-call round trip verified |
| 15 | Real Process Isolation | **Fixed M14's security hole**: removed U bits from full identity mapping and introduced per-process PML4/PDPT/PD/PT, isolating the 2 MiB user region. Native ELF64 loader added and execution verified in an isolated address space. **Bug discovered**: the ELF loader had the same use-after-free pattern |
| 16 | Multiprocessing + Fault Isolation | Added per-thread CR3 field, scheduler switches address spaces on every context switch. **Core feature**: Ring3 exceptions terminate only that process (`thread_exit()`), while Ring0 exceptions cause a full panic. Demonstrated with a crash-test program intentionally writing to a NULL pointer |
| 17 | GUI UX Overhaul | Gradient wallpaper, 6 desktop icons (double-click execution), real-time taskbar clock, window resizing (corner dragging), Notepad "unsaved (*)" indicator |
| 18 | Final Polish + Color + Internet | Functional minimize/maximize buttons, 4 runtime theme presets switchable by clicking in Settings, **new GUI web browser app** (real TCP/DNS HTTP GET, basic HTML tag stripping) |

---

## Currently Working Features

### Hardware / Boot

- UEFI boot (QEMU+OVMF, structured for real USB boot)
- GOP framebuffer rendering, COM1 serial debugging
- PS/2 keyboard (including extended scancodes) + mouse
- SATA/AHCI
- RTL8139 NIC
- CMOS RTC

### Kernel Core

- Physical/virtual memory management and kernel heap
- 100 Hz preemptive scheduler + IPC mailboxes
- **Ring3 user mode + system calls (`int $0x80`)**
- **Per-process isolated address spaces (PML4) + CR3 switching integrated with the scheduler**
- **User-process fault isolation** (page faults and similar exceptions terminate only the affected process while the kernel continues running)

### File System

- JETFS v1 (12 direct blocks + 1 indirect block = maximum ~4.2 MB/file)
- Logical namespace
- Accurate block reclamation when deleting/rewriting files

### Networking

- Ethernet + ARP + IPv4 + ICMP (ping) + TCP client + UDP + DNS (A records)
- Entirely polling-based
- Verified through actual QEMU SLIRP and external DNS server round trips

### GUI

- Window manager: dragging, **resizing**, **functional minimize/maximize/close buttons**, Win32-style and JetOS-style window themes
- Start menu
- **Right-click context menu**
- **Real-time taskbar clock**
- **Runtime color themes (4 presets)**
- **6 desktop icons + double-click execution**
- Gradient wallpaper
- UTF-8/Hangul rendering

### Applications

- **jash shell**: filesystem operations (`cd/ls/mkdir/touch/cat/write/rm`), networking (`net/arp/ping/nslookup/http/browser`), execution (`run` — automatic PE/ELF detection), history + arrow keys + cursor editing
- **Notepad**: F5/F6 save/load, unsaved-change indicator
- **File Manager**: click-based directory navigation
- **Settings**: system information + **clickable theme swatches**
- **Task Manager**: thread list
- **Clock**: real-time CMOS RTC display
- **GUI Browser** (new in M18): real HTTP GET + basic HTML tag stripping

### API Layer

- JetAPI (application ↔ kernel)
- Win32 compatibility layer (`CreateWindowA` / `MessageBoxA` / file I/O, etc., with **ms_abi calling convention fixed**)
- 3 system calls (`SYS_WRITE` / `SYS_EXIT` / `SYS_GETTICK`) for user processes

---

## Bug History — Discovered and Fixed

1. **PE/ELF loader use-after-free** (M9, M15): After `kfree(filebuf)`, pointers referencing data inside the buffer (`opt->AddressOfEntryPoint`, `eh->e_entry`) were still accessed.

   → Required values are now copied into local variables before freeing the buffer, and this pattern is consistently followed in file parsers.

2. **Win32 ABI mismatch** (M9): MinGW-built EXEs use the Microsoft x64 calling convention (RCX/RDX/R8/R9) when calling the IAT, while `win32.c` was compiled using the default SysV ABI (RDI/RSI/RDX/RCX), causing arguments to be read from incorrect registers.

   → Applied `WINAPI` (`__attribute__((ms_abi))`) to all IAT entry functions.

3. **Scheduler NULL-context bug** (M11): Calling `scheduler_yield()` before `scheduler_start()` (`g_current == NULL`) caused `&old->rsp` to become a fake address near NULL, permanently losing the context.

   → `scheduler_yield()` now immediately returns when `g_current` is NULL.

4. **RTL8139 frame-padding bug** (M11): Ethernet frames shorter than 60 bytes, such as pure ACK packets, are physically padded by the NIC, but the `rx_len` field reports the length before padding. The receive offset therefore advanced too little, corrupting the parsing of all subsequent packets.

   → Offset advancement now uses `max(rx_len, 64)`.

5. **TCP receive data loss** (M11): The receive buffer was activated only when `tcp_recv_until_close()` was called. Data arriving before that point had its sequence numbers consumed but the actual bytes were discarded.

   → Redesigned the TCP implementation to maintain an always-active internal accumulation buffer immediately after entering the ESTABLISHED state.

6. **M14 Ring3 security hole** (M14 → M15): The entire 0–4 GiB identity-mapped region had the U bit enabled, meaning Ring3 code could effectively access arbitrary kernel memory.

   → In M15, the shared kernel region was made supervisor-only (U bit disabled), while only the dedicated 2 MiB per-process user region has the U bit enabled.

   As a side effect, M14's original self-test method became invalid and was discarded in favor of ELF-based testing. This is not a regression; it is evidence that the security hole was actually closed.

---

## Known Limitations / Incomplete Features (M18)

### Core Incomplete Features

| Item | Status |
|---|---|
| Process user-space size | Fixed at 2 MiB per process (larger programs cannot run) |
| Dynamic linking | Not implemented — only statically linked ELF/PE binaries |
| User-pointer validation | Not implemented — invalid pointers passed to system calls are accessed directly by the kernel (a security risk) |
| HTTPS/TLS | Not implemented — GUI browser/jash `http` cannot access most HTTPS-only websites; only plain HTTP servers are supported |
| DNS functionality | A records only; no CNAME chain resolution or caching |
| Concurrent Ring3 processes | CR3 switching works, but stress testing with many simultaneous processes has not yet been performed |
| fork/exec model | Not implemented — `elf_execute()` simply attaches a new address space to a new thread and immediately executes it; there is no process tree or parent-child model |

### GUI Incomplete Features

| Item | Status |
|---|---|
| Scrollbars | Not implemented — long content in jash/browser/etc. is limited to the visible portion of the window |
| Copy/paste | Not implemented |
| Reusable widget toolkit | Not implemented — applications draw their own controls directly; there is no shared button/checkbox component library |
| Generalized keyboard focus routing | Not implemented — currently uses special cases such as "if jash is running, send input to jash; otherwise send it to Notepad." There is no general mechanism that automatically gives keyboard focus and input to the clicked window. Because of this, the browser address bar cannot be typed into directly through the GUI; URLs must be entered through jash commands |
| Confirmation dialogs | Not implemented — for example, closing Notepad does not ask whether to save; only the `*` indicator is shown |
| True modal MessageBox | No focus lock — other windows can still be manipulated while a MessageBox is displayed |

### Other

| Item | Status |
|---|---|
| Audio | No driver |
| Win32 API coverage | Only a subset such as `MessageBox`, `CreateWindow`, and file I/O. Many APIs such as `GetModuleHandleA` and `LoadLibraryA` are unimplemented and currently behave as no-ops |
| PE loader | Cannot run CRT-dependent EXEs; only statically linked executables using pure Win32 APIs are supported |

---

## Next Tasks — Proposed Priority Order

1. **Generalize keyboard focus routing** — Automatically give keyboard focus to the window that was clicked. This is the biggest GUI usability bottleneck. Once implemented, direct typing into the browser address bar and simultaneous use of multiple text applications will become natural.

2. **Scrollbars** — Introduce a common scrolling mechanism for all windows that can contain long content, including jash, the browser, and the file manager.

3. **Reusable widget toolkit** — Standardize buttons, checkboxes, text boxes, etc., so that Settings, File Manager, Browser, and other applications can share components without duplicating GUI code.

4. **HTTPS/TLS** — The feature with the greatest impact on actual Internet usability, but also one of the most difficult to implement. TLS 1.2 client support, including the handshake and certificate verification, should be the minimum target.

5. **User-pointer validation** — Add checks in system calls to verify that user-provided pointers actually belong to the calling process's user-space region. This is a high-priority security requirement because there is currently no real validation boundary.

6. **Dynamically expandable process user space** — Replace the fixed 2 MiB user region with dynamically expandable memory, potentially through simple `brk`/`mmap`-style system calls.

7. **fork/exec-style process model** — The current system only supports immediate execution, making shell pipelines and background execution impossible.

8. **Expand Win32 API coverage** — At minimum, reach a level where common console programs that do not depend on the CRT can run.

---

## Reference: Code Style / Design Principles

- **Assembly is restricted to exactly three files:**
  - `kernel/hal/x86_asm_shim.h` — port I/O, segment operations, Ring3 entry, and other single-instruction-level operations
  - `kernel/hal/context_switch.c` — thread context switching
  - `kernel/hal/syscall_entry.c` — system-call trampoline

  Everything else is 100% C. C++/Rust are also allowed when necessary (with user approval), but neither is currently used in the actual project.

- **Every new subsystem must add a boot-time self-test to `kernel_entry.c`** so that every newly built image automatically performs regression testing through serial logs. NET/TCP/DNS self-tests are examples of this approach.

- **Be careful with use-after-free patterns**: Whenever working with file parsers such as the PE/ELF loaders, always verify that no pointer referencing data inside a buffer is used after `kfree(buf)`. This exact mistake has already occurred twice.

- **Document known simplifications in code comments**: Clearly explain why a subsystem was simplified and what needs to be implemented later to make it fully complete.
