# JetOS Development Status

> Last updated: At the completion of Milestone 38
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU 8.2 + OVMF
> Current directive: "Upgrade hardware compatibility; Linux references are allowed."
> In this milestone, the debugging process itself was as important as the result. The details are honestly documented below under "Lessons Learned from This Debugging Session."

---

## What Was Actually Fixed This Time — The Longest-Standing Hardware Limitation Since M31

### 1. VMM: Generic Support for Mapping Physical Addresses Above 4 GiB (`vmm_map_mmio`)

This item had been carried over as "next work" throughout the M31/M32/M33/M34/M35/M36/M37 documentation.

`kernel/mm/vmm.c` now reserves PDPT slot 5 (virtual addresses starting at 5 GiB, up to a maximum 1 GiB window) as a new **MMIO window**. `vmm_map_mmio(phys_addr, size)` maps arbitrary physical addresses into this window using 2 MiB pages and returns the corresponding kernel virtual address.

`vmm_create_process_address_space()` was also extended so that this slot is shared by every new process in the same way as the kernel identity-mapped region — without the U-bit, making it Ring 0-only.

### 2. VirtIO-GPU Now Actually Works with BARs Above 4 GiB

The COMMON_CFG/NOTIFY_CFG BAR mapping logic in `kernel/drivers/virtio_gpu.c` was changed from:

> "Give up if the BAR is above 4 GiB"

to:

> "If the BAR is above 4 GiB, actually map it with `vmm_map_mmio` and use it."

**Verified with actual QEMU (`-device virtio-gpu-pci`):**

Although QEMU had been installed in this verification sandbox during M35–M37, an actual GPU device had never been attached and tested before. This time, the real GPU device was finally attached.

As expected, COMMON_CFG was located at `0xC000000000`, far above the 4 GiB boundary.

Result:

```text
[virtio-gpu] COMMON_CFG BAR is above 4 GiB — attempting mapping with vmm_map_mmio
[virtio-gpu] COMMON_CFG mapped to vaddr=0x140000000
STAGE: VIRTIO-GPU INIT OK — DirexJ now updates the screen through the actual GPU device
```

**This is the first time in the history of this project that virtio-gpu successfully initialized.**

From M31 through M37, it had never successfully initialized even once; it always gave up and fell back to the CPU rendering path.

The login screen, account creation, desktop (wallpaper + 9 icons + 3 windows + taskbar + incorrect/correct password handling) were all verified by screenshots to appear correctly through the **actual GPU rendering path**.

The question of whether "the GPU actually draws to the screen" had previously been documented in M32/M33 as "not verified." This is the first time it has been confirmed.

### 3. New VirtIO-Input Absolute-Coordinate Tablet Driver — Partially Verified

A new driver, `kernel/drivers/virtio_input.c`, was added.

It implements PCI capability discovery and virtqueue handling in the same style as virtio-gpu. `desktop.c` was also modified to prefer this device over PS/2 when it is available.

The goal is to eliminate the verification sandbox's longest-standing problem:

> "PS/2 only provides relative coordinates, so precise GUI click automation with HMP `mouse_move` is not possible."

This limitation had been repeatedly documented from M32 through M37.

**Result: "Partial success."**

Device initialization, feature negotiation, and event queue setup all succeeded.

After sending a single **HMP `mouse_button`** event first, the cursor successfully moved from its default center position to another actual coordinate.

The new `vinput` diagnostic command in jash also directly confirmed that the event queue's `used_idx` changed from `0` to `4`.

However, additional `mouse_move` events sent afterward did not move the cursor any further.

This milestone could not conclusively determine whether this was:

* a bug in the driver itself, or
* a limitation of QEMU's event generation after the initial connection in this `-display none` headless configuration.

This is therefore honestly recorded as **"partially verified"** below under Known Limitations.

---

## Lessons Learned from This Debugging Session

Two mistakes in the testing methodology — rather than bugs in the new functionality — caused several hours of extremely confusing debugging during this milestone.

### 1. Forgetting `make esp`

`build/KERNEL.ELF` was rebuilt several times, but `make esp`, which places the newly built kernel into the `build/esp.img` image actually booted by QEMU, was not run.

As a result, the same old kernel was repeatedly being booted.

This produced the extremely confusing signal:

> "I changed the code, but the behavior is exactly the same."

In reality, the updated kernel was simply never being booted.

**Rule:** Whenever kernel code changes, always verify both:

```text
make
make esp
```

### 2. Reusing `jetfs.img` Without Reformatting

An image that already contained an account was reused while automatically entering the three-step "first boot" input sequence:

```text
username
password
password confirmation
```

Because the system was actually showing the login screen, which requires only one password input, the extra automated input caused:

> "Incorrect password → retry"

This resulted in an apparent infinite loop.

It was initially mistaken for a kernel/desktop loop bug, leading to several hours of debugging a bug that did not actually exist.

Both problems looked exactly the same from the outside:

> "The screen isn't changing."

That made them particularly difficult to distinguish.

**Future rule:** Before investigating a mysterious runtime symptom, mechanically verify:

1. Is the newest binary actually being booted?
2. Is the filesystem image in a clean, expected state?
3. Is the test input appropriate for the current application state?

Only after those checks should the kernel code itself be treated as the primary suspect.

---

## Build & Run

The procedure is unchanged.

New configurations:

```text
./scripts/run_qemu.sh -device virtio-gpu-pci
```

Runs the actual GPU acceleration path.

```text
./scripts/run_qemu.sh -device virtio-tablet-pci
```

Runs the absolute-coordinate tablet path (partially functional).

The `vinput` diagnostic command has also been added to jash for inspecting the virtio-input event queue state.

---

## Verification Methodology (M38)

### 56. Real Hardware-Address Verification of the VMM Extension

Verified `vmm_map_mmio()` using the actual physical address `0xC000000000` assigned by QEMU.

This was not a simulated test.

The verification covered:

* mapping the real BAR address,
* obtaining the virtual mapping,
* dereferencing the mapped region,
* successfully writing to a reset register.

### 57. Full Real-Device Verification of the VirtIO-GPU Pipeline

Completed the entire pipeline for the first time:

```text
Initialization
    ↓
2D resource creation
    ↓
GPU rendering
    ↓
Actual screen presentation
```

The following were verified through screenshots using the actual GPU presentation path:

* Login screen
* Account creation failure
* Account creation success
* Desktop
* Wallpaper
* 9 desktop icons
* 3 windows
* Taskbar
* Correct password handling
* Incorrect password handling

### 58. Protocol-Level Verification of VirtIO-Input

Verified virtio-input at the protocol level on a real QEMU device.

The exact boundary of successful operation was also recorded:

> "The first batch of exactly 4 events is received successfully, but subsequent events are not."

The `used_idx` value observed through the jash `vinput` command provided concrete evidence.

This means the next developer does not need to start by assuming that the entire driver is broken. The investigation can begin directly around the event-buffer reuse/notification path.

### 59. Identification of Testing-Methodology Errors

The two debugging mistakes were precisely identified:

* missing `make esp`
* reusing a non-reformatted JETFS image

Both initially appeared to be mysterious kernel/runtime bugs, but were ultimately confirmed to be testing-pipeline errors.

Rather than leaving them as unexplained behavior, their actual causes were traced and documented.

---

## Milestone Summary

| #  | Title / Core Content                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| -- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 38 | **Hardware Compatibility Upgrade: 4 GiB+ MMIO Mapping, Actual GPU Acceleration, Absolute-Coordinate Mouse (Partial)** — Added `vmm_map_mmio()` to remove the M31-era limitation of giving up on BARs above 4 GiB. VirtIO-GPU successfully initialized and rendered through the actual QEMU GPU device for the first time in project history, with screenshots confirming the result. Added a virtio-input absolute-coordinate tablet driver; protocol-level operation was verified, but continuous event reception could not be confirmed in the headless environment. Also identified and documented two testing-methodology errors: missing `make esp` and failure to reformat the JETFS image. |

---

## Current Full Feature List (M1–M37 + M38)

### M1–M37

Same as the M37 documentation. All previous functionality remains intact.

No regression was observed during the real-device reboot verification performed for M38.

### Milestone 38 Additions

* **`vmm_map_mmio()`**: Generic support for mapping arbitrary physical addresses above 4 GiB into the kernel virtual address space using a dedicated MMIO window (PDPT slot 5, maximum 1 GiB).
* **VirtIO-GPU full operation**: Successfully initializes with BARs above 4 GiB and renders to the actual screen.
* **VirtIO-Input driver**: Absolute-coordinate tablet support, partially verified as described above.
* **jash `vinput` diagnostic command**: Displays virtio-input event queue status.

---

## Known Limitations / Incomplete Work (M38)

| Item                                                                                      | Status                                                                                                                                                                                                                                                                                                                                             |
| ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **VirtIO-Input: No additional events after the initial event batch**                      | Driver/protocol initialization, queue setup, and parsing of the first batch were all verified. The first 4 events are received successfully, but subsequent `mouse_move` events are not reflected. The cause remains unresolved — it may be a driver bug or a limitation of QEMU's headless mode. Verification with an actual display is required. |
| **VirtIO-GPU: 2D only, no 3D**                                                            | Existing limitation from M31 remains.                                                                                                                                                                                                                                                                                                              |
| **VirtIO-GPU: Still gives up when GOP `pixels_per_scanline` does not match expectations** | Existing M31 limitation; outside the scope of this milestone.                                                                                                                                                                                                                                                                                      |
| **USB absolute-coordinate mouse (`usb-tablet`)**                                          | Still not implemented. VirtIO-input was attempted first because its implementation is simpler. The USB stack itself was not modified in this milestone. If virtio-input cannot be completed, USB tablet support is the next candidate.                                                                                                             |
| **Real audio output (AC97/HDA)**                                                          | Still not started. Carried over since M34; virtio-input was prioritized for this milestone.                                                                                                                                                                                                                                                        |

Most limitations documented through M37 remain valid, including commercial software compatibility, filesystem journaling, the File Manager's 16 KiB copy limit, and other previously documented limitations.

---

## Recommended Next Tasks (Priority Order)

### 1. Highest Priority — Diagnose Continuous VirtIO-Input Event Reception

Test with a QEMU configuration using an actual display and move a real mouse.

Use the jash `vinput` command to check whether `used_idx` continues increasing.

If it increases:

> Confirm that the issue is specific to the headless sandbox.

If it does not increase:

> Confirm that the problem is in the driver, most likely around event-buffer recycling or device re-notification.

### 2. Real Audio Output

Implement actual audio output using:

* AC97
* HDA

This has remained deferred since M34.

### 3. VirtIO-GPU 3D / VirGL

Investigate VirtIO-GPU 3D and VirGL support.

Priority is low because the project's current requirements can be satisfied with 2D rendering.

### 4. Software Journaling / Commercial Software Compatibility

Continue the previously deferred work from M34/M36.

---

## Code Style / Design Principles

All principles from M37 remain valid.

M38 adds the following:

### 1. When Something "Mysteriously Doesn't Work," Question the Build/Test Pipeline First

Do not immediately assume that the most recently modified code is responsible.

Both major debugging issues in this milestone:

* missing `make esp`
* reusing an already-initialized filesystem image

made the system appear to behave as though the kernel itself were broken.

When a mysterious symptom occurs, mechanically verify:

> "Am I really running the newest build in a genuinely clean and expected test state?"

Only then investigate the code.

### 2. Record Partial Verification with Concrete Evidence

Do not describe partially working functionality simply as "doesn't work."

For virtio-input, the exact boundary was recorded:

> "The first 4 events work; subsequent events do not."

The observed queue index (`used_idx`) provides concrete evidence.

This allows future development to begin near the likely event-buffer recycling/re-notification logic instead of restarting the investigation from the entire driver.

### 3. Fix Structural Limitations at Their Root Cause

When an old limitation finally gets an opportunity to be removed, do not work around it.

The M31-era 4 GiB physical-address limitation was fundamentally caused by the lack of a generic mechanism for mapping physical addresses above 4 GiB.

`vmm_map_mmio()` fixes that structural limitation itself.

It is therefore not merely a VirtIO-GPU-specific workaround. It provides a reusable mechanism for **any future device whose BARs or MMIO regions reside above 4 GiB**, including VirtIO-Input and other PCI devices.
