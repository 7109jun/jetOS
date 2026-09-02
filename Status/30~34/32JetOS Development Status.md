# JetOS Development Status

> Last updated: Milestone 32 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + **QEMU 8.2 + OVMF — this environment was actually installed and used for real boot testing for the first time in this milestone.**
> Verification method: Full build (bootloader + kernel + all host tools) → ESP/JETFS image creation → **repeated real QEMU boots** in headless mode with serial log inspection + **actual screen capture** (QEMU monitor `screendump`) + direct in-kernel self-tests for the Recycle Bin and power management, with results verified through the serial console.
>
> This document is written so that the entire current state can be understood using only this document and the latest tar file (`JetOS_Milestone32.tar`).

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

This milestone had three main parts: ① real boot verification in the QEMU environment as promised in M31, ② the Recycle Bin, and ③ selecting the **10 things I consider essential for a computer OS** and implementing the most fundamental one among them: power-off/reboot.

However, as soon as ① was performed, **the VirtIO-GPU driver that had honestly been marked as "unverified" in M31 actually crashed the system during boot**, exactly as feared. The worst-case scenario I had anticipated became reality, and the cause was diagnosed and fixed on the spot.

This properly demonstrated the value of honestly documenting something as "unverified": if M31 had confidently and falsely claimed that the **GPU driver was complete**, this discovery would have been much more surprising and difficult to diagnose.

---

## Build & Run

**Starting with this milestone, QEMU/OVMF/mingw-w64/mtools are actually installed in the development environment itself** — the setup that `scripts/setup_toolchain.sh` was intended to perform was finally completed for real.

```bash
make                          # Bootloader + kernel
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
make jetfs_import             # (Optional) Import a program compiled with JCC
make esp
./scripts/run_qemu.sh         # Real boot! (It now actually works in this environment)
```

### New jash commands available after boot

```text
rm <path>              Move a file to the Recycle Bin (not immediate deletion)
recyclebin / trash     View Recycle Bin contents
restore <name>         Restore an item from the Recycle Bin to its original location
purge <name|all>       Permanently delete an item from the Recycle Bin
shutdown / poweroff    Shut down the system (verified working in real QEMU)
reboot / restart       Reboot the system (verified working in real QEMU)
```

---

## Verification Methodology (M32 — A Fundamental Step Up in This Project's Verification Process)

30. **The M31 VirtIO-GPU crash was discovered immediately through real QEMU boot testing**: When booting with `-device virtio-gpu-pci`, the system crashed immediately after `"STAGE: VIRTIO-GPU PROBE..."` with `[JETOS PANIC] PAGE FAULT code=0x2 addr=0x000000C000000014`. Analysis of the address showed that it was near `0xC000000000` (approximately 768 GiB). After adding debug logging and reproducing the issue, it was determined that the BAR pointed to by the `virtio-gpu-pci` "modern" capability was actually placed by QEMU far above 4 GiB, in a 64-bit-BAR-only MMIO window, while this kernel's VMM (`kernel/mm/vmm.c`) only identity-maps the 0–4 GiB range. The kernel was casting the unmapped address directly to a pointer and accessing it, causing the crash. After confirming the cause in code, a guard was added to **safely give up when a BAR is located above 4 GiB rather than attempting to use it**. After rebooting, the crash disappeared, and the system correctly fell back to the CPU rendering path with a log stating that the `COMMON_CFG BAR` was above 4 GiB and had therefore been abandoned.

31. **Successful boot was confirmed with both configurations: ****`virtio-gpu-pci`**** present and absent**: Without the device, the system reports that the PCI device cannot be found → normal fallback. With the device, the system reports that it is above 4 GiB and therefore abandoned → normal fallback. Both configurations were repeatedly confirmed to reach the desktop shell without crashing, with **more than five individual boot tests** performed in total.

32. **The actual screen was captured as a screenshot**: The QEMU monitor's `screendump` command was used to capture the booted desktop as an actual PNG. The screenshot visually confirmed that the gradient wallpaper (M21), anti-aliased sun (M23/M29 DirexJ), two windows (including title bars and buttons), taskbar (Start button/weather/clock/signal bars), and all six desktop icons were rendered correctly. This was the first time in the project that the GUI was directly verified as an actual rendered image.

33. **The Recycle Bin was verified first against a real ****`jetfs.c`**** implementation on the host and then once again inside the kernel**: First, `jetfs.c` and `recyclebin.c` were compiled with host GCC and tested against an actual disk image file. Twenty cases were verified, including deletion, restoration, collision prevention, refusal to restore when the original location was occupied, permanent deletion, and clearing the entire Recycle Bin. During this process, **two bugs in the test code itself** were discovered and fixed (see "Bugs Discovered" below) — `jetfs.c` and `recyclebin.c` were correct from the beginning. Then, **the actual kernel code was executed inside real QEMU against JETFS mounted on an actual AHCI disk**, running the complete delete → restore → delete again → purge sequence as an internal self-test. `"RECYCLEBIN SELF-TEST OK"` was directly confirmed through the serial console.

34. **Power management (shutdown/reboot) was verified by actually shutting down QEMU**: `power_shutdown()` was temporarily called at the end of the boot sequence. When booted without `-no-shutdown`, the **QEMU process itself exited after approximately 3 seconds with exit code 0**, much faster than the 20-second timeout. This is equivalent to actually turning off the machine from QEMU's perspective. `power_reboot()` was verified in the same manner, using `-no-reboot` and confirming that QEMU terminated when a reset was requested; it exited after approximately 2 seconds. After both tests were confirmed, the temporary calls were removed, leaving the functionality accessible only through jash commands.

---

## Directory Structure (M32 — Changes from M31 Only)

```text
JetOS/
├── kernel/drivers/virtio_gpu.c
│                    M32: Added a guard that safely gives up when a BAR
│                    exceeds the 4 GiB identity-mapping limit.
│                    This is the key fix for the actual crash.
│                    Diagnostic debug logging was also kept permanently
│                    rather than being treated as temporary logging,
│                    so that similar problems can be diagnosed immediately
│                    in the future.
│
├── kernel/fs/
│   ├── recyclebin.h/c
│                    M32 new: Recycle Bin — moves files under
│                    /.recyclebin/ using jetfs_rename and records their
│                    original paths in a text index.
│                    Supports restoration, permanent deletion,
│                    and clearing the entire Recycle Bin.
│
├── kernel/hal/
│   ├── power.h/c
│                    M32 new: Shutdown (QEMU-conventional ACPI port,
│                    not guaranteed on real hardware — documented)
│                    / reboot (8042 controller reset, a standard method
│                    that works broadly even on real hardware).
│                    Both were verified in real QEMU.
│
├── kernel/apps/jash.c
│                    M32: rm now calls recyclebin_delete instead of
│                    immediately deleting files.
│                    Added recyclebin/restore/purge/shutdown/reboot
│                    commands.
│
├── kernel/kernel_entry.c
│                    M32: Strengthened VirtIO-GPU diagnostic logging
│                    and added the Recycle Bin boot self-test
│                    (RECYCLEBIN SELF-TEST).
```

(All other directories remain identical to M31 — see the previous document.)

---

## Milestone Summary (M32 Addition Only)

| #  | Title                                                        | Core Contents                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| -- | ------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 32 | Real QEMU Boot Verification + Recycle Bin + Power Management | ① Installed QEMU/OVMF in the development environment and performed the first real boot — immediately discovered the M31 VirtIO-GPU crash, diagnosed the cause (4 GiB identity-mapping limit), and fixed it with a safety guard. ② Verified the actual desktop screen through a screenshot. ③ Added the Recycle Bin (delete → restore → permanent deletion), verified both on the host and inside the actual kernel. ④ Added power management (shutdown/reboot) and verified it by actually terminating QEMU. This implements item #1 from the 10 essential OS features list. |

---

## 10 Things I Consider Truly Essential for a Computer OS

The following were selected in priority order after excluding features that already exist, such as **multithreading, virtual memory + Ring3 isolation, TCP/DNS/TLS, GUI + window management, clipboard, Aero Snap, Alt+Tab, the built-in C compiler, and two filesystem types**.

1. **Power management (shutdown/reboot)** — O Implemented and fully verified in this milestone. This is the most basic item on the list, yet it was implemented surprisingly late — "How do you turn off the computer?" is arguably the first qualification of an OS, but JetOS did not have it from M1 through M31.

2. **Persistent configuration** — Settings such as wallpaper, volume, and clock format currently reset to hard-coded defaults on every boot. Storing something such as `/system/config.txt` in JETFS and loading it during boot could solve a large portion of this problem.

3. **User accounts / login** — The system currently goes directly from boot to the desktop, with no concept of who is using it. A real computer should have at least a login screen.

4. **File permissions / access control** — Neither JETFS nor FAT32 currently has ownership or permission bits. Even on a single-user OS, basic protection such as marking a file read-only is generally expected.

5. **Real audio output** — `speaker.c` currently provides only square-wave PC speaker beeps. Without actual waveform playback such as WAV/PCM, it is difficult to call the system a proper multimedia OS.

6. **Notification system** — There is no unified mechanism for the OS to inform the user that a task has finished or that a problem has occurred. Currently, each application only displays messages inside its own window.

7. **Search (files/apps)** — There is currently no way to search for a filename or installed application. The only option is to manually browse directories through the file manager.

8. **Filesystem journaling / crash recovery** — JETFS can become corrupted if power is lost while data is being written because atomic writes are not guaranteed. A proper OS filesystem should have at least this level of protection.

9. **Software installation / removal system** — Applications are currently built directly into the kernel image. JCC (M30) makes it possible to create new programs, but there is no actual concept of "installation" — such as registering an app, creating a Start Menu icon, or cleanly removing an application.

10. **Undo / Redo** — Text input applications such as Notepad have no Ctrl+Z functionality. There is no way to undo accidentally deleted text, making this the natural counterpart to the newly implemented Recycle Bin's file-level undo mechanism.

This milestone implemented item **#1 (power management)**. The remaining nine items have been incorporated into the "Next Tasks" section below in priority order.

---

## Complete Current Feature List (M31 Features + M32 Changes)

### M1–31 Complete Feature Set

(Same as the M31 document — all features remain intact. **Everything was re-confirmed together for the first time through real QEMU boot testing in M32**: all self-tests, including TLS, double-indirect blocks, AHCI, and the Recycle Bin, passed during actual boot; the GUI desktop was visually verified through a screenshot; and the VirtIO-GPU crash was discovered and fixed, resulting in no regressions.)

### New in Milestone 32

* **VirtIO-GPU crash fix**: Safely gives up when a BAR is located above 4 GiB instead of crashing.
* **Recycle Bin**: `rm` now performs recoverable deletion. Added `recyclebin` / `restore` / `purge` commands.
* **Power management**: `shutdown` / `poweroff` / `reboot` / `restart` — verified by actual QEMU termination.

---

## Bugs Actually Discovered and Fixed (M32)

30. **VirtIO-GPU crash caused by exceeding the 4 GiB identity-mapping range** (see Verification Methodology #30 above) — The most important fix in this milestone. Added a BAR address upper-bound check to `virtio_gpu_init`.

31. **Argument evaluation order bug in the host test code**: Debug code such as `printf("...%d...", jetfs_stat(path,&t,NULL), t, ...)` relied on the assumption that the function call would be evaluated before `t` was read. In C, **the evaluation order of function arguments is unspecified**, so `t` could be read before `jetfs_stat` was called, causing the output to consistently show the previous value (`0`). `jetfs.c` and `recyclebin.c` were correct from the beginning; the diagnostic code itself was incorrectly pointing toward the wrong cause. This was fixed by separating the function call and use of its result into separate statements.

32. **Host test code failed to satisfy the precondition of ****`jetfs_write`**: `jetfs_write` requires the file to have already been created with `jetfs_create`; it does not automatically create files. The test code overlooked this requirement, resulting in the misleading symptom that "file deletion fails." In reality, the file had never been created in the first place. The existing `cmd_write` implementation in `jash.c`, which already followed the correct pattern of checking with `stat` and calling `create` when necessary, was discovered later and the test code was updated to match it.

(Bugs 31/32 were **bugs in the newly written host test harness**, not defects in `recyclebin.c` or `jetfs.c`. The actual product code was correct from the beginning.)

---

## Known Limitations / Incomplete Features (M32)

Most of the limitations listed in the M31 document are still valid (VirtIO-GPU legacy mode unsupported, no 3D support, no multi-monitor support, etc.). However, "not verified on real hardware" has been partially addressed through QEMU verification; see the updated limitations below.

| Item                                                                        | Current Status                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **VirtIO-GPU: BARs above 4 GiB unsupported**                                | The real solution — general VMM support for mapping arbitrary physical addresses into the kernel virtual address space — has not been implemented. The driver only detects the condition and safely gives up. Since QEMU commonly places BARs above 4 GiB in this configuration, the driver may effectively "always give up" under most default QEMU configurations. It is still much better than crashing.                                                                                                         |
| **VirtIO-GPU: Actual screen rendering through the GPU is still unverified** | A QEMU configuration in which the BAR is located below 4 GiB could not be found. Therefore, actual screen updates after successful VirtIO-GPU initialization were not verified in this milestone either.                                                                                                                                                                                                                                                                                                            |
| **Recycle Bin: directories unsupported**                                    | Only files can currently be moved to the Recycle Bin (see `recyclebin.h`).                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Recycle Bin: text-based linear-search index**                             | Performance may degrade if hundreds or thousands of items accumulate.                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Power shutdown: not guaranteed on real hardware**                         | The implementation uses a QEMU-conventional port, so it is unlikely to work on actual hardware. Proper ACPI AML parsing is required for a real implementation and would be a separate large task. This limitation is documented in `power.h`.                                                                                                                                                                                                                                                                       |
| **No confirmation before shutdown/reboot**                                  | The system shuts down immediately even if there are unsaved changes.                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **GUI mouse automation is difficult in this verification environment**      | JetOS only supports PS/2 relative-coordinate mice. An attempt was made to click using QEMU monitor absolute coordinates (`usb-tablet`), but the kernel could not recognize the device, as expected because there is no USB HID driver. Therefore, automated verification of mouse interactions such as double-clicking desktop icons could not be performed. The desktop's visual rendering was verified through screenshots, while keyboard-driven functionality and internal self-tests were verified separately. |

---

## Next Tasks (Suggested Priority Order)

The remaining items from the "10 Essential Features" list should be addressed first:

1. **Persistent configuration** — Item #2 from the 10-feature list.
2. **Undo/Redo** — Item #10; a natural next step alongside the Recycle Bin.
3. **Search (files/apps)** — Item #7.
4. **Filesystem journaling** — Item #8; requires careful implementation because it affects filesystem reliability.
5. **Real audio output (WAV/PCM)** — Item #5.
6. **Notification system** — Item #6.
7. **User accounts / login** — Item #3.
8. **File permissions / access control** — Item #4.
9. **Software installation / removal system** — Item #9.
10. **VirtIO-GPU: General VMM support for arbitrary physical-address mapping** — Required to truly remove the 4 GiB limitation carried over from M31/M32.
11. **User-customizable desktop wallpaper** — Carried over from M27.
12. **JCC: Array initializer-list support** — Carried over from M30.

---

## Reference: Code Style / Design Principles

All principles established through M31 remain valid. M32 adds the following:

* **"If something could not be verified, prioritize verifying it as soon as the necessary verification tools become available"**: VirtIO-GPU was explicitly documented as "unverified" in M31. As soon as QEMU became available, that area was immediately investigated and the crash was actually discovered. Honest documentation of limitations serves as a map showing exactly where verification should begin later.

* **"Distinguish bugs in the test code from bugs in the product code during diagnosis"**: Both the `jetfs_stat` argument evaluation-order issue and the `jetfs_write` precondition issue were initially suspected to be problems with `recyclebin.c`. By calmly separating each layer (test harness → `recyclebin.c` → `jetfs.c`) and reproducing them independently, it became clear that the actual problems were in the test code. When a test fails, do not immediately blame the product code; first verify the assumptions and preconditions of the test itself.

* **"When real-device or emulator-based verification becomes possible, prioritize it over static verification"**: The sizeof/offsetof/synthetic config-space verification from M31 was still valuable, but the actual hardware test demonstrated that it could not catch certain classes of bugs, such as incorrect structure layouts or interactions involving address mapping, timing, and real device values. Static verification can never completely replace interaction with an actual device. Whenever possible, real-device/emulator verification should therefore be treated as the highest priority.
