# JetOS Development Status

> Last updated: Milestone 29 completed
> Build environment: `x86_64-w64-mingw32-gcc` (bootloader) + `gcc` (kernel) + QEMU + OVMF (testing)
> Verification methodology: Full-kernel compilation and linking + actual host-side compilation of `DirexJ` (`kernel/gfx/direxj.c`) to verify `isqrt` correctness, diff-based `present` correctness (the final screen always matches exactly), and the actual reduction in framebuffer writes (only 4.2% of all pixels were written in a frame where only a small region changed) + re-rendering the star and sun icons using the new `dj_*` functions and comparing the pixel output against M26 to confirm identical visual quality + actual QEMU boot with serial-log verification.
>
> This document, together with the latest tar archive (`JetOS_Milestone29.tar`), is intended to provide a complete picture of the current state of the project.

---

# Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Milestone 29 introduces **DirexJ**, using the requested name, but there is an important technical fact that must be stated clearly from the beginning:

**JetOS does not currently have a GPU driver.**

Since M1, all rendering has been pure software rendering, with the CPU directly writing pixels into the framebuffer provided by UEFI GOP.

That remains unchanged in M29.

Implementing an actual GPU driver such as VirtIO-GPU would itself be a kernel-sized project and is therefore outside the scope of this milestone.

As a result, the "GPU optimization" part of the original idea was deliberately narrowed down. M29 instead focuses on the two areas that can actually be improved with the current architecture:

1. **CPU-side rendering optimization**
2. **Cleaner and more reusable rendering primitives**

The first optimization changes framebuffer presentation from unconditional full-frame writes to **writing only pixels that actually changed**, reducing CPU/MMIO write traffic.

The second consolidates the anti-aliasing mathematics previously implemented independently in M23/M26 into a single reusable library.

From the next milestones onward, new rendering code no longer needs to duplicate the same anti-aliasing mathematics.

---

# Build & Run

Same as the M28 documentation. No additional requirements.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

DirexJ is used internally by `wm.c` without any additional configuration.

Applications can theoretically include:

```c
#include "../gfx/direxj.h"
```

However, DirexJ is **not yet exposed through JetAPI**, so user-space applications cannot directly call the `dj_*` API at this stage.

---

# Verification Methodology (M29 Update)

In addition to the fifteen verification methods used through M28:

## 16. Diff-Present Correctness and Actual Write Reduction

`kernel/gfx/direxj.c` was compiled directly with host GCC and tested independently.

Two properties were verified.

### A. Output Correctness

After calling `dj_present_diff`, the front and back buffers were compared pixel-by-pixel.

The buffers always became completely identical.

This demonstrates that the optimized implementation produces the same final screen as the previous full-frame copy implementation.

### B. Actual Write Reduction

A test frame of:

```text
64 × 48 pixels
```

was created.

Only two rows were modified:

```text
2 × 64 = 128 pixels
```

The number of pixels actually written by `dj_present_diff` was measured.

Only **4.2% of the total pixels** were written.

The remaining **95.8%** were already identical and therefore were not written again.

This provides a measured result rather than merely assuming that diff-based presentation will reduce MMIO writes.

---

## 17. Visual Regression Test for Anti-Aliased Icons

The new DirexJ functions were used to render the star and sun icons:

* `dj_fill_triangle_union_aa`
* `dj_fill_circle_aa`

These functions contain the same anti-aliasing logic previously used by `wm.c`.

The icons were rendered to PPM images and compared against the M26 rendering results.

The results were pixel-identical.

The anti-aliased edges therefore remained visually unchanged after the refactoring.

This confirms that the M29 change moved the existing mathematics into a reusable library without changing its rendering behavior.

---

# Directory Structure

## Changes Compared with M28

```text
JetOS/
├── kernel/gfx/
│   ├── direxj.h
│   └── direxj.c
│       M29: New DirexJ rendering library.
│
│       Provides:
│       - dj_isqrt()
│         Shared integer square-root implementation.
│
│       - dj_fill_circle_aa()
│         Solid anti-aliased circle rendering.
│
│       - dj_point_in_triangle()
│         Point-in-triangle test.
│
│       - dj_fill_triangle_union_aa()
│         Supersampled anti-aliased polygon rendering.
│         The triangle-union logic previously used for the
│         M26 star and speaker-cone rendering was moved here
│         without changing its mathematics.
│
│       - dj_present_diff()
│         Optimized framebuffer presentation that writes
│         only pixels whose values have changed.
│
├── kernel/gui/wm.c
│       M29: Removed the locally implemented isqrt and
│       triangle-supersampling mathematics and replaced them
│       with calls to the corresponding dj_* functions.
│
│       draw_weather_icon() now uses dj_fill_circle_aa()
│       for the sun and cloud circles instead of maintaining
│       separate rendering loops.
│
│       wm_present() is reduced to a call to:
│
│           dj_present_diff(g_front, g_con)
```

All other directories remain the same as M28.

---

# Milestone Summary

| #  | Title                                         | Core Changes                                                                                                                                                                                                                                                                                                                                |
| -- | --------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 29 | DirexJ — CPU Optimization + Rendering Library | Documented the absence of a GPU driver honestly. Added diff-based framebuffer presentation so only changed pixels are written, reducing CPU/MMIO write traffic. Consolidated anti-aliasing primitives such as integer square root, circle AA, point-in-triangle testing, and triangle supersampling into `kernel/gfx/direxj.h/c` for reuse. |

---

# Current Working Features

## M1–28 Features

Same as the M28 documentation.

All previously implemented functionality remains available.

At the completion of M29, the following were reconfirmed:

* Full kernel compilation and linking succeed.
* Existing icon rendering remains visually correct.
* Existing desktop/background rendering remains correct.
* Clipboard functionality remains operational.
* Aero Snap remains operational.
* Existing window-management behavior remains operational.
* No visual regression was found in the affected rendering paths.

---

## New in Milestone 29

### DirexJ Rendering Library

DirexJ provides reusable rendering primitives:

```text
dj_isqrt()
dj_fill_circle_aa()
dj_point_in_triangle()
dj_fill_triangle_union_aa()
```

These functions provide a common implementation for anti-aliased circles and polygons.

Future rendering code can use these functions directly instead of duplicating the underlying mathematics.

---

### Diff-Based Framebuffer Presentation

`dj_present_diff()` compares the new framebuffer contents against the currently displayed contents and writes only pixels whose values differ.

The resulting screen is identical to the previous full-frame presentation method, but unchanged pixels are not written again.

This reduces unnecessary framebuffer/MMIO writes when only a small portion of the screen changes.

In the M29 host-side benchmark, a small changed region resulted in only **4.2% of the total pixels being written**.

---

# Bugs / Issues Found in M29

No bugs were found during the M29 implementation.

The milestone consisted primarily of:

* Refactoring
* Rendering-library extraction
* CPU-side presentation optimization

The optimized presentation path was verified against the previous full-frame behavior, and the visual rendering primitives were compared against their M26 output.

---

# Known Limitations / Incomplete Features

All limitations documented in M28 remain valid, including:

* No partial text selection.
* Right Ctrl is unsupported.
* Ctrl+A/Z/Y are unsupported.
* Clipboard is text-only.
* Clipboard contents disappear after reboot.
* Only three Aero Snap zones.
* No snap keyboard shortcuts.
* No cursor-relative snap detachment.
* FAT32 write UI integration is still pending.
* FAT32 deletion is still pending.
* Remaining certificate-validation work is still pending.
* No ECDHE.
* No AC97/HDA digital audio.

M29 adds the following limitations:

| Item                                                   | Current Status                                                                                                                                                                                                                                                                     |
| ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| No GPU acceleration                                    | DirexJ is a pure CPU software-rendering optimization/library. There is no actual GPU driver such as VirtIO-GPU.                                                                                                                                                                    |
| `dj_present_diff()` still reads the entire framebuffer | Only writes are reduced. To determine which pixels changed, the implementation still reads every pixel once per frame. This is not true dirty-rectangle tracking.                                                                                                                  |
| `wm_draw()` still renders the entire scene every frame | The desktop background, icons, and all windows are still redrawn into the back buffer every frame. M29 only optimizes the final framebuffer presentation step.                                                                                                                     |
| No true dirty-rectangle rendering                      | Rendering itself is not skipped when nothing has changed. A true dirty-rectangle system would require each relevant subsystem to accurately report which regions changed. This was considered a substantially riskier refactoring and was therefore left for a separate milestone. |
| DirexJ is not exposed through JetAPI                   | DirexJ is currently an internal kernel library used by `wm.c`. PE/ELF user applications cannot directly call `dj_*` functions yet.                                                                                                                                                 |

---

# Next Tasks

The roadmap remains focused on the "Windows 7-level" objective.

## 1. Alt+Tab Window Switching

Carried over from M28.

The existing keyboard-state handling pattern can be reused for Alt tracking.

---

## 2. Recycle Bin

Implement a Recycle Bin system so files can be restored instead of being immediately and permanently deleted.

---

## 3. User-Configurable Desktop Wallpaper

Allow users to select and change the desktop wallpaper instead of using only the current hard-coded gradient.

---

## 4. Partial Text Selection

Implement text selection through:

* Mouse dragging
* Shift + Arrow

This will allow the clipboard to operate on selected text rather than only entire input buffers.

---

## 5. True Dirty-Rectangle Rendering

Extend DirexJ/window-manager rendering so `wm_draw()` itself can avoid unnecessary work.

The intended direction is:

```text
No visual changes
        ↓
No affected regions
        ↓
Skip unnecessary rendering
        ↓
Skip unnecessary framebuffer writes
```

This is a larger and riskier change than M29 because every subsystem that can modify the screen must correctly identify its affected regions.

---

## 6. Expose FAT32 Writing Through `jash` / File Manager

Carried over from M26.

Possible command:

```text
usbexport <jetfs-path> <8.3-name>
```

A corresponding **Export to USB** operation can also be added to the file manager.

---

## 7. FAT32 Deletion

Implement FAT32 file deletion support.

---

## 8. Remaining Certificate Validation

Continue work on:

* CRL
* OCSP
* Name normalization/comparison

---

## 9. ECDHE Support

Add Elliptic Curve Diffie-Hellman Ephemeral key exchange to the TLS implementation.

---

## 10. AC97 / HDA Digital Audio

Move beyond PC-speaker tone generation and implement a real digital audio driver.

---

# Code Style / Design Principles

All principles established through M28 remain valid:

* Perform a full system audit before starting a large roadmap feature.
* Reuse existing state-management mechanisms whenever practical.
* Use host-side harnesses for logic that can be isolated from hardware.
* Verify visual changes through actual rendering.
* Avoid large static buffers.
* Document intentional simplifications.
* Default security-sensitive behavior to fail-closed.
* Verify cryptographic code using data generated by real third-party tools.
* Cross-validate interoperable formats using tools from other operating systems.
* Distinguish symptoms from root causes.
* Implement shared functionality at the toolkit level when multiple applications can benefit.
* Reuse proven input-handling patterns for new hardware inputs.
* Measure optimization results rather than assuming they are effective.

---

# New M29 Principles

## 1. Do Not Let the Name Overstate the Actual Capability

When asked to create "DirexJ" as a GPU optimization system, the first architectural constraint was identified:

**JetOS has no GPU driver.**

Rather than hiding this limitation or implying that CPU framebuffer rendering is equivalent to GPU acceleration, the limitation is explicitly documented at the top of the milestone.

The actual M29 responsibilities are:

```text
CPU software rendering
        +
framebuffer/MMIO write reduction
        +
reusable rendering mathematics
```

not GPU acceleration.

General rule:

> Define a component according to what it actually does, not what its name might imply.

This prevents future confusion such as expecting GPU acceleration from a component that only optimizes CPU-side framebuffer rendering.

---

## 2. Prove That Refactoring Did Not Change the Result

Moving code from one location to another should not automatically be treated as safe merely because the mathematics were supposedly unchanged.

In M29, the anti-aliasing implementation was moved from `wm.c` into DirexJ.

Instead of assuming the output would remain identical, the old and new rendering results were compared.

The star and sun icons produced the same pixel output.

General rule:

> After a rendering refactor, directly compare the output against the previous implementation whenever possible.

This turns "the code was only moved" from an assumption into a verified property.

---

## 3. Measure Optimizations Quantitatively

The diff-based presentation optimization was not justified by saying that it "should be faster."

The host harness measured:

* Total pixels
* Changed pixels
* Actual pixels written

A representative frame showed:

```text
Total pixels:       100%
Actually written:     4.2%
Unchanged:           95.8%
```

The optimization can therefore be described using an observed measurement rather than speculation.

General rule:

> If an optimization claims to reduce work, measure the work before and after it whenever practical.

This applies not only to framebuffer writes, but also to future rendering, filesystem, networking, and scheduling optimizations.
