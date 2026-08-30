# JetOS Development Status

> Last updated: Milestone 27 completed
> Build environment: `x86_64-w64-mingw32-gcc` (bootloader) + `gcc` (kernel) + QEMU + OVMF (testing)
> Verification methodology: Full-kernel compilation and linking + actual host-side PPM rendering of the snap-preview overlay for visual verification (semi-transparent "Aero glass" appearance + border) + manual arithmetic verification of left/right half-screen coordinates at a representative resolution (no overlap or negative dimensions) + actual QEMU boot with serial-log verification.
>
> This document, together with the latest tar archive (`JetOS_Milestone27.tar`), is intended to provide a complete picture of the current state of the project.

---

# Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Starting with this milestone, the project's direction has changed.

The new goal is to reach **Windows 7-level functionality** by identifying features that Windows 7 has but JetOS does not, and implementing them one by one.

Before starting Milestone 27, the JetOS window manager was comprehensively audited to determine what functionality it already provided.

The result was significantly more complete than expected:

* Window dragging
* Window resizing
* Minimize / maximize / close
* Scrollbar dragging
* Right-click context menus
* Start menu
* Taskbar window buttons
* Desktop icons
* Mouse wheel support

All of these were already implemented.

While searching for an actually missing category, **Aero Snap** was identified as a missing feature.

Aero Snap automatically arranges a window when its title bar is dragged to a screen edge, either snapping it to half of the screen or maximizing it.

This was selected as the first feature because Aero Snap was **introduced with Windows 7** and became one of the defining user-interface features of that version. It therefore fits the new "Windows 7-level" target particularly well.

---

# Build & Run

Same as the M26 documentation. No additional requirements.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

---

# Usage

### Aero Snap

Grab the title bar of any window and drag it to within **6 pixels of the left or right edge of the screen**.

A semi-transparent blue rectangle appears as a preview showing where the window will be placed.

Release the mouse button and the window snaps to exactly half of the screen.

Dragging the window to the **top edge of the screen** maximizes it.

A snapped window can be grabbed again by its title bar and dragged away. When this happens, the window immediately restores its original pre-snap size and continues being dragged.

This provides the same basic "detach" behavior as Windows 7.

---

# Verification Methodology (M27 Update)

In addition to the twelve verification methods used through M26:

### 13. Actual Host-Side Rendering of the Snap Preview

The snap-zone calculation in `wm.c` and the overlay rendering based on `console_blend_pixel` were copied directly into a small host-side rendering program.

A 200×150 PPM image was generated and visually inspected.

The test specifically verified the actual rendered appearance of:

* Semi-transparent overlay fill
* Bright border
* "Aero glass" visual effect

The rendered output was inspected directly rather than relying solely on numerical calculations.

This follows the same methodology used for the M26 icon anti-aliasing verification.

---

### 14. Manual Arithmetic Verification of Half-Screen Snap Coordinates

The `snap_zone_rect` calculations were manually verified at a representative resolution of **1024×768**.

The resulting snap zones were confirmed as:

* Left half: `x = 4..510`
* Right half: `x = 514..1020`

This produces an exact **4-pixel gap** between the two regions.

The calculations were checked to ensure:

* The left and right zones do not overlap.
* No negative dimensions are produced.
* The top/maximize snap follows the existing 4-pixel edge-margin convention used by `wm_toggle_maximize`.

---

# Directory Structure

## Changes Compared with M26

```text
JetOS/
├── kernel/gui/
│   ├── wm.h
│   │       M27: Added the `snapped` field to wm_window_t.
│   │       0 = not snapped
│   │       1 = left
│   │       2 = right
│   │
│   │       Added:
│   │       - wm_snap_window()
│   │       - wm_unsnap_window()
│   │       - wm_snap_zone_at()
│   │       - wm_set_snap_preview()
│   │       - wm_is_window_maximized()
│   │       - wm_is_window_snapped()
│   │
│   ├── wm.c
│   │       M27: Implemented the functions above.
│   │       Updated wm_toggle_maximize() so that it
│   │       understands snap state and preserves the
│   │       original pre-snap window geometry.
│   │
│   │       Added snap-preview overlay rendering
│   │       to wm_draw().
│   │
│   └── desktop.c
│           M27: Connected snap detection and application
│           to the drag event loop.
│
│           On mouse release:
│           - Detect the snap zone.
│           - Apply the appropriate snap state.
│
│           During dragging:
│           - Update the snap preview every frame.
│
│           When dragging a snapped window again:
│           - Immediately restore its original geometry.
│           - Continue the drag operation.
```

All other directories remain the same as M26.

---

# Milestone Summary

| $  | Title                                     | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| -- | ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 27 | Aero Snap — First Windows 7-Level Feature | Dragging a window title bar to the left or right screen edge snaps it to the corresponding half of the screen. Dragging to the top edge maximizes the window. A semi-transparent preview overlay is displayed while dragging. Re-dragging a snapped window immediately restores its original geometry. Snap state was integrated with the existing `wm_toggle_maximize()` state-management system so both maximize-button and snap-drag operations share the same `saved_x/y/w/h` mechanism. |

---

# Current Working Features

## M1–26 Features

Same as the M26 documentation. All previously implemented functionality remains available.

At the completion of M27, the following were reconfirmed:

* Full kernel compilation and linking succeed.
* Existing window dragging continues to work.
* Existing window resizing continues to work.
* Existing maximize behavior continues to work.
* No regressions were found in the existing window-management functionality.

---

## New in Milestone 27

### Aero Snap

Windows can now be:

* Snapped to the **left half** of the screen.
* Snapped to the **right half** of the screen.
* **Maximized** by dragging to the top edge.
* Previewed through a semi-transparent overlay while dragging.
* Detached from a snapped state by grabbing and dragging the title bar again.

The snapped window's original geometry is preserved and restored when the window is detached.

---

# Bugs / Issues Found in M27

No bugs were found in the newly implemented functionality.

However, the pre-development audit produced an important finding:

The JetOS window manager already contained significantly more functionality than initially expected, including:

* Window dragging
* Window resizing
* Minimize / maximize / close
* Scrollbars
* Right-click context menus
* Start menu
* Taskbar window buttons
* Desktop icons
* Mouse wheel support

These had already been implemented between M18 and M21.

This confirmed that a complete feature audit is necessary before selecting new functionality for the "Windows 7-level" roadmap.

Without such an audit, future milestones could unnecessarily reimplement features that already exist.

---

# Known Limitations / Incomplete Features

All limitations documented in M26 remain valid, including FAT32 and certificate-validation limitations.

M27 adds the following limitations:

| Item                                     | Current Status                                                                                                                                                                                                                                                       |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Only three snap zones                    | Only left-half, right-half, and maximize snapping are implemented. Windows 7/10-style four-way corner snapping to quarter-screen regions is not implemented.                                                                                                         |
| No snap keyboard shortcuts               | Keyboard shortcuts such as `Windows + Arrow` are not implemented. Snapping currently requires mouse dragging.                                                                                                                                                        |
| Snap detection is based on screen edges  | Snapping to the edge of another window is not implemented. This is outside the current scope and was not part of Windows 7's original Aero Snap behavior.                                                                                                            |
| Cursor-relative detachment is simplified | Windows 7 preserves the relative position of the cursor within the title bar when a snapped window is detached. JetOS currently restores the original window geometry without preserving that exact cursor-relative position. This is an intentional simplification. |

---

# Next Tasks

The roadmap is now reorganized around the "Windows 7-level" target.

These are features confirmed during the pre-development audit to be present in Windows 7 but not yet implemented in JetOS.

Priority is based primarily on perceived user impact relative to implementation complexity.

## 1. Clipboard — Copy / Paste

Multiple applications, including `jash`, Notepad, and the browser, handle text, but there is currently no way to move text between applications.

A system-wide clipboard would provide a major usability improvement for a relatively small implementation cost.

---

## 2. Alt+Tab Window Switching

Windows can currently be switched by clicking their taskbar buttons, but there is no keyboard shortcut for cycling through windows.

Implementing `Alt+Tab` would significantly improve keyboard-based window management.

---

## 3. Recycle Bin

The file manager currently needs a proper deletion model.

Instead of deleting files immediately and permanently, implement a Recycle Bin-style mechanism that allows deleted files to be restored.

---

## 4. User-Configurable Desktop Wallpaper

The current desktop background is a hard-coded gradient.

Add support for selecting and changing the desktop wallpaper, similar to Windows 7's personalization interface.

---

## 5. Expose FAT32 Writing Through `jash` / File Manager

Carried over from M26.

Possible implementation:

```text
usbexport <jetfs-path> <8.3-name>
```

A corresponding "Export to USB" action should also be added to the file manager.

---

## 6. FAT32 Deletion

Implement `fat32_delete()` as the next major FAT32 filesystem extension after write support.

---

## 7. Remaining Certificate Validation

Continue work on:

* CRL
* OCSP
* Proper certificate Name normalization/comparison

These were deferred because of their relatively high implementation cost compared with their immediate practical benefit.

---

## 8. ECDHE Support

Add Elliptic Curve Diffie-Hellman Ephemeral key exchange to improve the TLS implementation.

---

## 9. AC97 / HDA Digital Audio

Move beyond the current PC-speaker tone support and implement an actual digital audio driver.

---

# Code Style / Design Principles

All principles established through M26 remain valid:

* Keep assembly limited to three files.
* Test actual boot behavior.
* Use host-side harnesses where appropriate.
* Perform a full rebuild whenever headers change.
* Verify visual changes using screenshots or rendered output.
* Avoid large static buffers.
* Be careful with use-after-free conditions.
* Document intentional simplifications.
* Use only concepts, rather than copying implementations, when studying other projects.
* Inspect all callers when widening API parameter types.
* Verify cryptographic code using data generated by real third-party tools.
* Default security-sensitive behavior to fail-closed.
* Split verification into multiple independent conditions and verify each condition empirically.
* Distinguish symptoms from root causes.
* Cross-validate interoperable formats using tools from the operating systems that consume those formats.

---

# New M27 Principles

## 1. Audit the Existing System Before Tackling a Large Goal

When given a broad objective such as **"reach Windows 7-level functionality,"** first perform a comprehensive audit of the existing implementation.

A vague goal such as "add Windows 7 features" can easily lead to duplicated work.

For M27, `wm.c` and `desktop.c` were audited first.

This revealed that JetOS already had significantly more window-management functionality than expected.

Only after identifying the genuinely missing functionality was Aero Snap selected.

The same process should be used for future milestones:

1. Identify candidate Windows 7 features.
2. Search the codebase.
3. Confirm whether each feature actually exists.
4. Determine its current implementation quality.
5. Only then select the next missing feature.

---

## 2. Reuse Existing State-Management Mechanisms

Aero Snap does not introduce a separate window-geometry restoration system.

Instead, it extends the existing `saved_x/y/w/h` restoration mechanism from the M18 maximize implementation.

The maximize button and snap-drag system therefore share the same saved original geometry.

Conditional state saving prevents one mechanism from accidentally overwriting the geometry saved by the other.

This approach reduces duplicated state and minimizes the possibility of inconsistent window behavior.

**General rule:**

> When implementing a feature similar to an existing feature, extend and reuse the existing state-management mechanism whenever possible instead of creating a second independent state system.
