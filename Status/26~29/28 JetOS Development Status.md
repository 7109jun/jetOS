# JetOS Development Status

> Last updated: Milestone 28 completed
> Build environment: `x86_64-w64-mingw32-gcc` (bootloader) + `gcc` (kernel) + QEMU + OVMF (testing)
> Verification methodology: Full-kernel compilation and linking + actual host-side compilation and execution of the clipboard logic (`widget_textbox_key` + `clipboard.c`), with seven verification cases covering cursor-position insertion, cut operations, and cross-application pasting between independent textbox instances + actual QEMU boot with serial-log verification.
>
> This document, together with the latest tar archive (`JetOS_Milestone28.tar`), is intended to provide a complete picture of the current state of the project.

---

# Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Milestone 28 is the **second Windows 7-level feature**.

The first priority from the M27 roadmap was the clipboard.

Multiple applications — including `jash`, Notepad, and the browser address bar — already handled text independently, but there was no mechanism for transferring text between applications.

M28 fills this gap with the familiar **Ctrl+C / Ctrl+X / Ctrl+V** shortcuts.

It is a relatively small subsystem, but it provides a significant usability improvement because text can now move between otherwise independent applications.

---

# Build & Run

Same as the M27 documentation. No additional requirements.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

---

# Usage

### Clipboard Shortcuts

* **Ctrl+C** — Copies the entire contents of the focused input area to the clipboard.

  * `jash`: current input line
  * Notepad: entire text buffer
  * Browser address bar: entire textbox contents
  * Partial text selection is not yet supported.

* **Ctrl+X** — Copies the same contents to the clipboard and then clears the source.

* **Ctrl+V** — Inserts the clipboard contents at the current cursor position.

  * `jash`: actual cursor position
  * Browser address bar: actual cursor position
  * Notepad: appended to the end of the buffer

Only the **left Ctrl key** is currently recognized. Right Ctrl is not supported.

---

# Verification Methodology (M28 Update)

In addition to the fourteen verification methods used through M27:

### 15. Actual Host-Side Execution of Clipboard Logic

The clipboard implementation was compiled and executed with host GCC.

The following components were tested:

* `widget_textbox_key`
* `clipboard.c`

The code used by the kernel was retained as-is, except that console rendering functions were replaced with empty host-side stubs where necessary.

Seven verification cases were performed.

The tests confirmed:

1. Ctrl+C copies the complete textbox contents correctly.
2. Ctrl+V inserts clipboard contents correctly.
3. Pasting at the middle of a string produces the correct ordering:

   * prefix
   * pasted content
   * suffix
4. Ctrl+X clears the original contents.
5. Ctrl+X leaves the copied contents available in the clipboard.
6. Pasting works between two independent textbox instances.
7. The clipboard is genuinely shared global state rather than state local to an individual textbox.

All seven tests passed.

The cross-instance test is particularly important because the source and destination textboxes were separate structures, proving that clipboard contents are shared independently of the textbox that originally produced them.

---

# Directory Structure

## Changes Compared with M27

```text
JetOS/
├── kernel/drivers/
│   ├── keyboard.h
│   │       M28: Added virtual key codes:
│   │       - KEY_CTRL_C
│   │       - KEY_CTRL_X
│   │       - KEY_CTRL_V
│   │
│   ├── keyboard.c
│   │       M28: Added left-Ctrl key tracking.
│   │       Ctrl+C/X/V scan-code combinations are converted
│   │       into the corresponding virtual key codes.
│   │       Other Ctrl+character combinations remain outside
│   │       the current scope and pass through as normal
│   │       character input.
│
├── kernel/gui/
│   ├── clipboard.h/c
│   │       M28: New global text clipboard implementation.
│   │       Single clipboard slot containing text only.
│   │
│   │       Because partial text selection does not yet exist,
│   │       Ctrl+C/X always operate on the entire focused
│   │       input area.
│   │
│   ├── widget.c/h
│   │       M28: Added Ctrl+C/X/V handling to
│   │       widget_textbox_key().
│   │       Paste operations insert text at the exact cursor
│   │       position.
│   │
│   │       The browser address bar automatically gains
│   │       clipboard support because it uses
│   │       widget_textbox_t; no browser-specific code
│   │       changes were required.
│
├── kernel/apps/jash.c
│   │       M28: Added Ctrl+C/X/V handling to the input-line
│   │       editor in jash_run_once().
│   │       Supports copying/cutting the entire current input
│   │       line and pasting at the cursor position.
│
├── kernel/gui/desktop.c
│       M28: Added Ctrl+C/X/V handling to notepad_append().
│       Supports copying/cutting the entire text buffer and
│       pasting at the end of the buffer.
```

All other directories remain the same as M27.

---

# Milestone Summary

| #  | Title                                      | Core Changes                                                                                                                                                                                                                                                                                                                                                    |
| -- | ------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 28 | Clipboard — Second Windows 7-Level Feature | Added left-Ctrl tracking and new `Ctrl+C/X/V` virtual key codes. Added a global text clipboard module. Integrated clipboard support into `jash` input lines, Notepad buffers, and the browser address bar through `widget_textbox_t`. Clipboard operations currently operate on entire input areas because partial text selection has not yet been implemented. |

---

# Current Working Features

## M1–27 Features

Same as the M27 documentation. All previously implemented functionality remains available.

At the completion of M28, the following were reconfirmed:

* Full kernel compilation and linking succeed.
* Existing textbox input continues to work.
* Existing `jash` input editing continues to work.
* Existing Notepad input continues to work.
* Browser address-bar input continues to work.
* No regressions were found in the existing text-input functionality.

---

## New in Milestone 28

### System-Wide Text Clipboard

Text can now be transferred between:

* `jash`
* Notepad
* Browser address bar

using:

```text
Ctrl+C
Ctrl+X
Ctrl+V
```

The clipboard is implemented as global kernel state.

A major benefit of placing clipboard handling inside `widget_textbox_t` is that applications using the widget automatically inherit clipboard support.

For example, the browser address bar required **no browser-specific code changes** after clipboard support was added to `widget_textbox_key()`.

---

# Bugs / Issues Found in M28

No bugs were found in the newly implemented clipboard functionality.

The seven host-side clipboard tests all passed, including cross-instance clipboard sharing.

---

# Known Limitations / Incomplete Features

All limitations documented in M27 remain valid, including:

* Only three Aero Snap zones.
* No snap keyboard shortcuts.
* No cursor-relative snap detachment.
* FAT32 UI integration still pending.
* FAT32 deletion still pending.
* Remaining certificate-validation work.
* No ECDHE.
* No AC97/HDA digital audio.

M28 adds the following limitations:

| Item                               | Current Status                                                                                                                                                                                                                             |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| No partial text selection          | There is currently no UI for selecting part of a string using mouse dragging or Shift+Arrow. Ctrl+C/X therefore always operate on the entire focused input area. This differs from Windows, where only the selected portion can be copied. |
| Right Ctrl not supported           | Only the left Ctrl key is tracked. This is asymmetric with Shift, which supports both left and right variants. The practical impact is currently small, but the limitation is documented.                                                  |
| No Ctrl+A/Z/Y                      | Other common Ctrl shortcuts such as Select All, Undo, and Redo are not implemented. Only Ctrl+C/X/V are currently supported.                                                                                                               |
| Text-only clipboard                | The clipboard currently stores text only. Images and files are not supported. Copying JETFS/FAT32 files to the clipboard is unrelated and remains a separate file-manager feature.                                                         |
| Clipboard state is lost on restart | Clipboard contents exist only as a global variable in kernel memory. They disappear after reboot, and there is currently no clipboard manager or clipboard history.                                                                        |

---

# Next Tasks

The clipboard item from the M27 roadmap is now complete.

The updated priority order is:

## 1. Alt+Tab Window Switching

Windows can currently be switched by clicking taskbar buttons, but there is no keyboard shortcut for cycling through windows.

Left-Ctrl tracking has already established the required pattern, so implementing Alt tracking can reuse a similar approach.

---

## 2. Recycle Bin

Determine the current deletion behavior in the file manager and implement a Recycle Bin-style system so deleted files can be restored instead of being immediately and permanently removed.

---

## 3. User-Configurable Desktop Wallpaper

Replace the current hard-coded desktop gradient with support for selecting and changing the desktop wallpaper.

---

## 4. Partial Text Selection

Add text selection through mechanisms such as:

* Mouse dragging
* Shift + Arrow

This is the major missing piece required to upgrade the current clipboard from **whole-buffer operations** to **selection-based operations** like a normal desktop OS.

---

## 5. Expose FAT32 Writing Through `jash` / File Manager

Carried over from M26.

Possible command:

```text
usbexport <jetfs-path> <8.3-name>
```

A corresponding **Export to USB** action should also be added to the file manager.

---

## 6. FAT32 Deletion

Implement:

```text
fat32_delete()
```

as the next major FAT32 filesystem extension.

---

## 7. Remaining Certificate Validation

Continue work on:

* CRL
* OCSP
* Proper certificate Name normalization/comparison

---

## 8. ECDHE Support

Add Elliptic Curve Diffie-Hellman Ephemeral key exchange to the TLS implementation.

---

## 9. AC97 / HDA Digital Audio

Move beyond PC-speaker tone generation and implement an actual digital audio driver.

---

# Code Style / Design Principles

All principles established through M27 remain valid:

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
* Audit the existing system before implementing a large roadmap feature.
* Reuse existing state-management mechanisms whenever practical.

---

# New M28 Principles

## 1. Put Shared Functionality at the Toolkit Level When Possible

If functionality can be implemented inside a shared toolkit component, doing so allows every application using that component to benefit automatically.

In M28, clipboard support was implemented in `widget_textbox_key()`.

As a result, the browser address bar gained Ctrl+C/X/V support without requiring any browser-specific changes.

The same principle applies to future applications:

> If a reusable component can provide a feature for multiple applications, implement the feature in the shared component first rather than duplicating the implementation in every application.

This reduces duplicated code and makes future applications inherit established functionality automatically.

---

## 2. Reuse Proven Input-Handling Patterns for New Hardware Keys

Left-Ctrl tracking was implemented by reusing the existing Shift-key tracking pattern in `keyboard.c`.

The existing make/break scan-code handling was adapted rather than inventing a completely new mechanism.

This approach has two advantages:

* Existing, already-tested input-handling behavior is reused.
* The amount of new keyboard-state logic is minimized.

General rule:

> When adding support for a new hardware input with behavior similar to an existing input, reuse the established implementation pattern whenever possible instead of creating a new mechanism from scratch.

This reduces both implementation complexity and the number of untested paths introduced into the kernel.
