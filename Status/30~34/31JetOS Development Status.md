# JetOS Development Status

> Last updated: Milestone 31 completed
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU + OVMF (testing)
> Verification method: Alt+Tab was verified for logical correctness through host-side simulation (which also uncovered and fixed a real bug). VirtIO-GPU is **the least thoroughly verified code in this project's history** — struct layouts were verified with `sizeof`/`offsetof`, PCI capability parsing was verified using a synthetic config space, and ring index arithmetic was verified separately, but actual communication with a real device was never tested because QEMU was unavailable in the development environment. The entire kernel successfully compiles and links.
>
> This document, together with the latest tar archive (`JetOS_Milestone31.tar`), is intended to provide a complete picture of the current state of the project.

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

This milestone had two tracks: **things that had not been completed** (the backlog) and **the extremely important GPU work**.

The backlog was addressed by fully implementing and honestly verifying Alt+Tab, during which a real bug was discovered and fixed. On the GPU side, only two milestones after M29's DirexJ explicitly revealed that "this kernel has no GPU support," an actual GPU implementation was added: **VirtIO-GPU (2D mode)**, implemented from scratch from PCI capability discovery through the virtqueue protocol and GPU command submission.

However, this milestone introduced a fundamental limitation that this project had never encountered before:

**PCI and hardware-protocol code can only be truly validated by communicating with an actual device (or QEMU), but QEMU was not available in this development environment.**

For that reason, this document places unusually strong emphasis on **what could not be verified**. This may be considered the milestone that tested the project's principle of honest scope documentation more than any previous milestone.

---

## Build & Run

The build and execution procedure is the same as in the M30 documentation.

VirtIO-GPU is automatically detected during boot without any additional configuration. If QEMU is started with:

```bash
-device virtio-gpu-pci
```

the device will be detected. If no VirtIO-GPU device is present (for example, a configuration using only the standard UEFI GOP framebuffer), the driver quietly skips initialization and JetOS behaves exactly as it did through M29.

```bash
./scripts/run_qemu.sh
```

---

## Verification Methodology

### Alt+Tab — Fully Verified

**22. A real z-order cycling bug was discovered during host-side simulation.**

The initial implementation moved focus to the window immediately below the current one and updated the z-order array after every switch.

When this was simulated with three windows on the host, repeatedly pressing Alt+Tab revealed that **window 0 could never be reached**. The system would endlessly alternate between windows 1 and 2.

The cause was that `bring_to_front()` moved the selected window to the end of the array every time. As a result, "the window immediately below the current focus" would always converge on the window that had just been selected.

The implementation was redesigned to use a fixed cycle based on window slot indices:

```text
0 → 1 → 2 → 0 → ...
```

The same simulation was then used to verify:

* All three windows are reached within one complete cycle.
* Minimized windows are skipped.
* Closed windows are skipped.
* The system safely stops when only one eligible window remains.

All tests passed.

### VirtIO-GPU — Partially Verified

**23. Wire-format structure layouts were verified using `sizeof` and `offsetof`.**

All 9 relevant VirtIO common configuration and VirtIO-GPU command structures were compiled and inspected on the host. Their byte sizes and field offsets were compared against the specification.

This specifically verifies that the compiler did not introduce unexpected padding, which is one of the most common failure modes for this class of hardware protocol.

All 9 structures matched the specification exactly.

**24. PCI capability list parsing was verified using a synthetic configuration space.**

Because no real PCI device was available, a capability chain was constructed manually in a byte array containing:

* One unrelated capability
* `COMMON_CFG`
* `NOTIFY_CFG`

The `find_virtio_cap()` logic was then tested to verify:

* BAR extraction
* Offset extraction
* Length extraction
* `notify_off_multiplier` extraction
* Correct failure when a requested capability type does not exist
* Immediate failure when the PCI capabilities-list bit is disabled

All tested cases behaved correctly.

**25. Ring index arithmetic was verified.**

Virtqueue `avail`/`used` indices are `uint16_t` values and therefore wrap around at 65536.

The arithmetic involving:

```text
idx % VQ_SIZE
```

was tested over 200,000 iterations, including wraparound, to verify that indexing remains consistent.

**26. `present()` clipping logic was verified.**

Six cases were tested:

* Negative coordinates
* Rectangle extending beyond the right edge
* Rectangle extending beyond the bottom edge
* Rectangle completely outside the screen
* Zero-sized rectangle
* Normal in-bounds rectangle

All cases were correctly clipped or ignored.

**27. A real bug was discovered during a type audit and fixed.**

`boot_info->framebuffer_base` is a `uint64_t`, but the parameter of `virtio_gpu_init()` had accidentally been declared as `uint32_t`.

On systems where the framebuffer physical address is above 4 GB, this would truncate the address and cause the GPU resource to use completely incorrect backing memory.

This is particularly dangerous because **GCC does not report this conversion with the project's default `-Wall -Wextra` settings**. The conversion requires stricter warnings such as `-Wconversion`.

A manual type-by-type audit of all physical addresses, pointers, and sizes was therefore performed, and the parameter was corrected from `uint32_t` to `uint64_t`.

---

## What Could Not Be Verified

VirtIO-GPU was **not** able to verify any of the following:

* Whether the PCI device is actually detected
* Whether the vendor/device IDs are correct on the target configuration
* Whether `0x1AF4 / 0x1050` actually matches the QEMU `virtio-gpu-pci` device in the intended configuration
* Whether MMIO register reads and writes actually reach the device
* Whether feature negotiation succeeds against a real device
* Whether GPU commands submitted through the virtqueue receive actual device responses
* Whether anything is actually rendered to the display
* Whether `GPU_TIMEOUT_TICKS=200` is an appropriate timeout value

The last point is especially important.

The value `200` ticks was chosen as a provisional value without measurements from a real device. It may be too short and cause a functioning device to time out, or too long and unnecessarily delay recovery from a genuinely stalled device.

---

## Why VirtIO-GPU Is Still Included

There was some consideration as to whether this milestone should be labeled "complete" given the lack of real-device verification.

The decision was to include it, but **not to claim that the GPU implementation is proven to work**.

The implementation has two important safety properties:

1. Every major operation has a timeout.
2. Any initialization or rendering failure automatically falls back to the existing CPU rendering path.

Therefore, the GPU implementation is designed so that even if it is completely incorrect, it should not prevent JetOS from booting or destroy the existing rendering path.

The appropriate description is therefore:

> **Experimental VirtIO-GPU support that has been extensively statically tested but has not yet been validated against an actual device or QEMU.**

This distinction is important.

---

## Directory Structure

Changes from M30 are shown below.

```text
JetOS/
├── kernel/drivers/
│   ├── keyboard.h/c
│   │   └── M31: Added left-Alt key tracking using the same pattern as Ctrl.
│   │       Added KEY_ALT_TAB as a virtual key code used exclusively for Alt+Tab.
│   │
│   ├── virtio_gpu.h/c
│   │   └── M31: New VirtIO-GPU 2D driver.
│   │       Includes PCI capability discovery, VirtIO feature negotiation,
│   │       control queue setup, 2D resource creation, backing attachment,
│   │       scanout configuration, and present (transfer + flush).
│   │       NOT VERIFIED against a real device.
│
├── kernel/gui/
│   ├── wm.h/c
│   │   └── M31: Added wm_cycle_focus() for Alt+Tab.
│   │       Uses fixed window-slot ordering and skips minimized/closed windows.
│   │
├── kernel/gui/desktop.c
│   └── M31: Intercepts KEY_ALT_TAB at the window-manager level instead of
│       delivering it to applications, following the same pattern as PgUp/PgDn.
│
├── kernel/gfx/direxj.c
│   └── M31: dj_present_diff calculates the bounding box of changed pixels.
│       If VirtIO-GPU is available, only that region is submitted through
│       virtio_gpu_present(). Otherwise, the M29 CPU rendering path is used.
│
└── kernel/kernel_entry.c
    └── M31: Added virtio_gpu_init() after hal_sti() and AHCI initialization.
        Interrupts must already be enabled because the timeout mechanism
        depends on g_timer_ticks.
```

All other directories remain unchanged from M30.

---

## Milestone Summary

| Milestone | Title                                   | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| --------- | --------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 31        | Alt+Tab + VirtIO-GPU (2D, Experimental) | Alt+Tab now cycles through all eligible windows using fixed window-slot ordering. A real infinite 2-cycle bug in the first implementation was discovered through host-side simulation and fixed. VirtIO-GPU was implemented from scratch, including PCI capability discovery, feature negotiation, virtqueue setup, 2D resource creation, backing attachment, and scanout configuration. Actual device/QEMU operation remains unverified. All major operations use timeouts and automatically fall back to CPU rendering on failure. |

---

## Current Feature Set

### Milestones 1–30

All features from the M30 documentation remain available.

At the end of M31:

* The entire kernel successfully compiles and links.
* VirtIO-GPU initialization failure does not prevent the existing CPU rendering path from operating.
* No known M30 functionality was intentionally removed.

### Milestone 31

#### Alt+Tab

* Switches between open windows.
* Uses fixed window-slot ordering.
* Skips minimized windows.
* Skips closed windows.
* Safely handles the case where only one eligible window remains.

#### VirtIO-GPU — Experimental

* Detects VirtIO-GPU through PCI.
* Performs VirtIO feature negotiation.
* Sets up a control virtqueue.
* Creates a 2D GPU resource.
* Attaches the existing GOP framebuffer as backing memory.
* Configures scanout 0.
* Transfers changed regions to the GPU.
* Flushes updated regions.
* Automatically falls back to CPU rendering if GPU initialization or presentation fails.

**Important: actual operation has not been verified against QEMU or real hardware.**

---

## Bugs Found and Fixed in M31

**28. Alt+Tab infinite 2-cycle bug**

The original z-order-based cycling implementation could become trapped between two windows.

Host-side simulation exposed the issue. The implementation was redesigned to use fixed window-slot indexing, eliminating the infinite 2-cycle.

**29. `framebuffer_base` 64-bit to 32-bit truncation**

The `virtio_gpu_init()` framebuffer parameter was incorrectly declared as `uint32_t` even though `framebuffer_base` is `uint64_t`.

The parameter was changed to `uint64_t`.

The issue was not detected by the project's default GCC warnings and was discovered through manual type auditing.

---

## Known Limitations / Incomplete Work

All limitations carried over from M30 remain valid, including the outstanding JCC-related items.

New limitations introduced in M31:

| Item                                        | Status                                                                                                          |
| ------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| VirtIO-GPU not tested against a real device | The most important M31 limitation. See the verification section above.                                          |
| VirtIO-GPU legacy/transitional devices      | Not supported. Only the modern `0x1050` device ID is recognized.                                                |
| VirtIO-GPU 3D / virgl                       | Not supported. Only 2D mode is implemented.                                                                     |
| Scanline padding                            | If `pixels_per_scanline != width`, initialization is safely aborted.                                            |
| Multiple monitors                           | Not supported. Only scanout 0 is used.                                                                          |
| GPU interrupts                              | Not implemented. Polling is used, similar to the existing AHCI approach. MSI-X is not configured.               |
| `GPU_TIMEOUT_TICKS`                         | The value `200` is currently an unverified provisional value and must be recalibrated using a real environment. |
| Alt+Tab preview UI                          | No Windows-style preview list. Each Tab press immediately switches to the next eligible window.                 |

---

## Next Tasks

Priority order:

1. **Run JetOS with VirtIO-GPU under QEMU and perform real end-to-end verification.**
   This is the most important task in M31. Verify device detection, feature negotiation, virtqueue operation, resource creation, transfer, flush, and actual screen output. If the GPU path causes boot or rendering failures, immediately disable `virtio_gpu_init()` and return to the known-good M30 CPU rendering path.

2. **Recalibrate `GPU_TIMEOUT_TICKS`** based on measurements from an actual environment.

3. **Recycle Bin**

4. **User-configurable desktop wallpaper**

5. **JCC: Array initializer-list support** carried over from M30.

6. **Partial text selection** using mouse dragging.

7. **Connect FAT32 write support to `jash` and File Manager.**

8. **Complete the remaining certificate-validation functionality**, including CRL/OCSP-related verification.

---

## Code Style / Design Principles

All principles from M30 remain valid.

M31 adds the following:

### 1. Unverifiable code must fail safely

When hardware cannot be tested, the correct response is not to pretend it works and not necessarily to abandon the implementation.

Instead, ask:

> **"If this code is completely wrong, can JetOS still boot safely?"**

For VirtIO-GPU, this means:

* Timeouts at every blocking stage
* Automatic fallback on failure
* No mandatory dependency on GPU initialization
* No destructive side effects when initialization fails

The inability to perform hardware verification cannot be removed, but the resulting risk can be reduced through defensive design.

### 2. Understand bug categories that compilers cannot automatically catch

The `framebuffer_base` truncation bug passed both `-Wall` and `-Wextra`.

It required stricter conversion warnings such as `-Wconversion` to be diagnosed automatically.

Therefore, physical addresses, pointers, sizes, and hardware-defined integer fields must receive manual type audits even when the compiler reports no warnings.

### 3. The more important the feature, the more important honest limitation reporting becomes

The more important a feature is, the greater the temptation to describe it as "done."

That is precisely when inaccurate confidence becomes most dangerous.

If a feature has not been tested against actual hardware, it must be explicitly labeled as unverified.

**For important features, honest verification status is more valuable than an impressive completion claim.**
