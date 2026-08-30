# JetOS Development Status

> Last updated: Milestone 26 completed
> Build environment: `x86_64-w64-mingw32-gcc` (bootloader) + `gcc` (kernel) + QEMU + OVMF (testing)
> Verification methodology: Full-kernel compilation and linking + OpenSSL-based certificate-chain harness (expanded from 31 tests in M25 to 40 in M26) + **independent verification using a real FAT32 image created with `mkfs.vfat`, with 74 files written using the same `fat32.c` used by the kernel and then verified with Linux `fsck.vfat`/mtools** + actual host-side PPM rendering of the icon anti-aliasing algorithm for visual inspection + actual QEMU boot with serial-log verification.
>
> This document, together with the latest tar archive (`JetOS_Milestone26.tar`), is intended to provide a complete picture of the current state of the project.

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Milestone 26 handled four independent tasks in parallel:

1. **Taskbar icon anti-aliasing** — postponed three times since M23 and finally implemented.
2. **FAT32 write support** — USB storage could previously only be "imported from"; it can now also be written to.
3. **`extendedKeyUsage` validation** — the final major missing piece of the certificate-chain validation work started in M24/M25.
4. **A real widget positioning bug fix** — a bug causing widgets to be drawn at incorrect screen positions.

Since these tasks were independent, their implementation order did not matter.

---

## Build & Run

Same as the M25 documentation. No additional build requirements.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

### Writing Files to FAT32 (New)

Applications can call `fat32_write_file(short_name, data, len)` to create or overwrite files on a FAT32-formatted USB drive.

Currently, this is available only as an internal API. Exposing it through an actual `jash` command or the file manager is planned for a future milestone, as described in **Next Tasks** below.

The current implementation supports:

* 8.3 short filenames only (for example, `"REPORT.TXT"`)
* Root-directory files only
* Creating new files
* Overwriting existing files

---

## Verification Methodology (M26 Update)

In addition to the nine verification methods used through M25:

### 10. FAT32 Write Verification Using a Real `mkfs.vfat` Image + `fsck.vfat`/mtools

A 64 MB FAT32 image with 2 KB clusters was created using `dosfstools`.

The kernel's `kernel/fs/fat32.c` was compiled with host GCC after replacing only `ahci_read/write_sectors` with host-side file-I/O stubs. The remaining FAT32 implementation was kept identical to the code used by the kernel.

The following 17 cases were verified successfully:

* Creating a new file
* Overwriting an existing file
* Writing content spanning multiple clusters
* Verifying that the previous cluster chain is actually released during overwrite
* Overwriting with smaller content
* Creating a zero-byte file
* Rejecting invalid 8.3 filenames
* Normalizing lowercase filenames
* Creating 70 root-directory entries
* Verifying that the root-directory cluster chain actually expands
* And other FAT32 allocation, overwrite, and directory-expansion cases

Afterward, the resulting image was opened using **completely independent Linux tools**, rather than our own FAT32 implementation:

* `fsck.vfat -v`
* `mdir`
* `mtype`

All 74 files were correctly recognized, and their contents matched the expected data.

The only issue reported by `fsck.vfat` was a mismatch in the FSInfo sector's **free-cluster count hint**. This field is only a performance-oriented cache and does not affect the correctness of the FAT cluster chains or directory structure. The current implementation intentionally does not update it, and this limitation is documented below.

---

### 11. Host-Side Visual Verification of the Anti-Aliasing Algorithm

The anti-aliasing functions from `wm.c` were copied directly into a small host-side rendering program and used to generate a 64×64 PPM image.

The following functions were rendered:

* `fill_triangle_union_aa`
* `point_in_triangle`
* `draw_star`
* `draw_speaker_icon`
* `draw_weather_icon`

The resulting image was visually inspected to confirm that:

* diagonal edges of the star were smooth,
* the diagonal edges of the speaker horn were smooth,
* circular edges of the sun/cloud shapes were smooth.

This was verified through actual rendered output rather than by relying only on numerical reasoning.

---

### 12. Widget Overlap Bug: Root Cause Identified and Fixed

The reported symptom was simply that "widgets overlap," which was not sufficient to identify the actual cause.

The coordinate conventions in `wm.c` were therefore compared across the relevant applications:

* `content_cb` uses absolute coordinates.
* `click_cb` uses local coordinates.
* The widget toolkit expects absolute/back-buffer coordinates.

A full application-by-application comparison revealed that the browser's `widget_textbox_t` was the only affected widget.

`browser.c` initialized the address bar only once:

```c
widget_textbox_init(&g_addr_box, 4, 3, ...);
```

After initialization, its coordinates were never updated when the browser window moved.

As a result, the address bar was always rendered near screen coordinates `(4, 3)`, regardless of where the browser window was actually located.

For example, even when the browser window was positioned at `(200, 200)`, its address bar remained near the top-left corner of the desktop and taskbar area.

The click handler still used local coordinates, so address-bar click detection continued to work correctly. This likely explains why the bug remained unnoticed for some time.

The fix was to recalculate the address-bar position every frame inside `browser_content_cb`:

```c
g_addr_box.x = x + 4;
g_addr_box.y = y + 3;
```

The address bar now correctly follows the actual position of the browser window.

---

## Directory Structure

### M26 Changes Compared with M25

```text
JetOS/
├── kernel/fs/
│   ├── fat32.h/c
│   │       M26: Read-only → write support.
│   │       Added fat32_write_file().
│   │       FAT mirror updates implemented through
│   │       fat_set_next_cluster().
│   │       Free-cluster search, chain allocation,
│   │       chain release, and root-directory expansion
│   │       are all implemented.
│
├── kernel/gui/
│   ├── wm.c
│   │       M26: Added anti-aliasing to
│   │       draw_star(), draw_speaker_icon(),
│   │       and draw_weather_icon().
│   │       Added the reusable polygon helpers
│   │       fill_triangle_union_aa() and
│   │       point_in_triangle().
│   │       These use triangle-union supersampling and
│   │       are reused by the star and speaker horn.
│   │       draw_network_icon() was left unchanged because
│   │       its signal bars are axis-aligned rectangles
│   │       and therefore do not require anti-aliasing.
│
├── kernel/net/crypto/
│   ├── x509_min.h/c
│   │       M26: Added parsing for extendedKeyUsage
│   │       (OID 2.5.29.37).
│   │       Checks for serverAuth and
│   │       anyExtendedKeyUsage.
│   │
│   ├── x509_trust.c
│   │       M26: If the leaf certificate contains an EKU,
│   │       certificate-chain validation now fails when
│   │       serverAuth is not present.
│
├── kernel/apps/browser.c
│       M26: Fixed the widget positioning bug.
│       browser_content_cb now recalculates the address-bar
│       coordinates from the browser window's actual
│       current position every frame.
│       Previously, the textbox remained fixed at (4,3)
│       after being initialized.
```

All other directories remain the same as M25.

---

## Milestone Summary

| #  | Title                                                           | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| -- | --------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 26 | Icon AA + FAT32 Write + EKU Validation + Widget Positioning Fix | ① Applied triangle supersampling and `isqrt`-based blending to the star, speaker, and weather icons. Confirmed that the signal bars were not an anti-aliasing target because they consist entirely of axis-aligned rectangles. ② Added FAT32 writing, including file creation/overwrite, cluster allocation/release, and root-directory expansion, independently verified using `mkfs.vfat`, `fsck.vfat`, and mtools. ③ Added `extendedKeyUsage`/`serverAuth` validation to certificate-chain verification. ④ Fixed the `browser.c` address bar being rendered at a fixed screen position instead of following the browser window. |

---

# Current Working Features

## M1–25 Features

Same as the M25 documentation. All features remain functional.

At the completion of M26, the following were reconfirmed:

* Full kernel compilation and linking succeed.
* All 40 certificate-chain tests pass.
* No regressions were found.

---

## New in Milestone 26

### Taskbar Icon Anti-Aliasing

The following taskbar icons now have smoother diagonal and curved edges:

* Start-button star
* Volume/speaker icon
* Weather icon (sun/cloud)

The signal-strength icon was confirmed not to require anti-aliasing because it consists entirely of axis-aligned rectangles.

### FAT32 Write Support

`fat32_write_file()` can now create or completely overwrite files in the root directory of a FAT32 USB drive.

Current scope:

* 8.3 filenames only
* Root directory only
* File creation
* Complete file overwrite

### `extendedKeyUsage` Validation

If a leaf certificate contains an `extendedKeyUsage` extension but does not allow `serverAuth`, the TLS connection is rejected.

If the extension is completely absent, the current implementation imposes no EKU restriction.

### Widget Positioning Bug Fix

The browser address bar now follows the actual browser-window position instead of being rendered at a fixed location near the top-left corner of the desktop.

---

# Bugs / Issues Found and Fixed in M26

### 22. Browser Address Bar Rendered at a Fixed Position

`browser.c` initialized the address bar using:

```c
widget_textbox_init(&g_addr_box, 4, 3, ...);
```

This happened only once during window creation.

The widget toolkit expects absolute back-buffer coordinates, while the browser's `content_cb` receives the window's actual current position.

Because the address-bar coordinates were never updated, the address bar remained at `(4, 3)` even when the browser window was moved elsewhere.

The issue was fixed by recalculating the coordinates every frame:

```c
g_addr_box.x = x + 4;
g_addr_box.y = y + 3;
```

---

### 23. FAT32 FSInfo Inconsistency Reported During Verification

During the initial FAT32 verification, `fsck.vfat` reported:

> "Free cluster summary wrong"

Investigation showed that the FAT32 driver does not update the FSInfo sector.

FSInfo contains cached information about the approximate number of free clusters. It is a performance hint rather than part of the actual FAT allocation structure.

The FAT tables and directory structures were verified to be correct, and `fsck.vfat` otherwise reported the filesystem as structurally valid.

Therefore, this was classified as an intentional limitation rather than a filesystem correctness bug.

---

# Known Limitations / Incomplete Features

Most limitations from M25 remain valid:

* No CRL/OCSP support
* Name comparisons use byte-for-byte equality
* No built-in trust store
* Trust-store state requires reloading after reboot
* No CBC padding-oracle protection
* RTC timezone is not adjusted
* Certificate-chain length is limited to six certificates
* Self-issued `pathLen` exceptions are not implemented

The `extendedKeyUsage` limitation has been removed from the list because it was implemented in M26.

### New / Updated Limitations

| Item                    | Current Status                                                                                                                                                                                                            |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| FAT32 write scope       | Create and complete overwrite only, following the same contract as `jetfs_write`. Append, partial modification, deletion, and subdirectory creation are not supported. 8.3 filenames only; LFN creation is not supported. |
| FAT32 FSInfo sector     | Not updated. It is a performance hint and does not affect filesystem correctness. `fsck.vfat` can automatically correct it. See Issue 23 above.                                                                           |
| FAT32 write performance | Each FAT-entry update rereads and rewrites the corresponding sector, with the process repeated for each FAT copy. Large files with many clusters may therefore be slow.                                                   |
| FAT32 UI integration    | Only the internal `fat32_write_file()` API currently exists. A user-facing command or file-manager action is planned for a future milestone.                                                                              |
| Signal-strength icon    | Confirmed in M26 to be outside the anti-aliasing scope because it consists entirely of axis-aligned rectangles.                                                                                                           |

---

# Next Tasks

Prioritized continuation from the M25 roadmap:

1. **Expose FAT32 writing through `jash` / the file manager**
   Example:

   ```text
   usbexport <jetfs-path> <8.3-name>
   ```

   Also consider adding a "Export to USB" option to the file manager.

2. **Implement FAT32 deletion (`fat32_delete`)**
   The natural next filesystem extension after write support.

3. **Complete the remaining certificate-validation work**
   CRL/OCSP and proper Name normalization/comparison. These have relatively high implementation cost compared with their current practical benefit, so they have continued to be deferred.

4. **Improve trust-store usability**
   Carried over from M24:

   * Display trust-store load status in Settings.
   * Add a command to reload the trust store without rebooting.

5. **Add ECDHE support**

6. **Add AC97/HDA digital audio support**

7. **Recover resources when processes terminate**

8. **Apply the reusable widget toolkit throughout the GUI**
   During the M26 codebase audit, it was discovered that `widget_button_t` is currently unused. The manually drawn buttons in Settings and the file manager should eventually be migrated to the widget toolkit.

9. **Perform the 20 GB scenario test**
   Carried over from M23.

---

# Code Style / Design Principles

All principles established through M25 remain valid:

* Keep assembly limited to three files.
* Test actual boot behavior.
* Use host-side harnesses where appropriate.
* Perform a full rebuild whenever headers change.
* Verify visual changes using screenshots or rendered output.
* Avoid large static buffers.
* Be careful with use-after-free conditions.
* Document intentional simplifications.
* When changing API parameter widths/types, inspect all callers.
* Use host sparse-file harnesses for large-scale scenarios.
* Verify cryptographic code using data generated by real third-party tools.
* Default security-sensitive behavior to fail-closed.
* Split verification into multiple independent conditions and verify each one empirically.
* Use real external tools to validate data produced by cryptographic and filesystem implementations.

### New M26 Principles

#### 1. Distinguish Symptoms from Root Causes

When receiving an ambiguous bug report such as "widgets overlap," do not immediately modify the most plausible-looking code.

First:

1. Identify the relevant subsystem.
2. Determine its coordinate/state conventions.
3. Compare those conventions against actual application usage.
4. Locate the concrete inconsistency.
5. Only then implement the fix.

In M26, the three coordinate conventions in `wm.c` were compared:

* `content_cb` → absolute coordinates
* `click_cb` → local coordinates
* Widget toolkit → absolute/back-buffer coordinates

This comparison isolated the actual inconsistency to `browser.c`.

---

#### 2. Cross-Validate Filesystem Formats Using Real External OS Tools

A filesystem format such as JETFS, which is designed and consumed entirely by JetOS, can reasonably be tested primarily with its own implementation.

However, a format such as FAT32 derives much of its value from interoperability with other operating systems.

Testing only our own FAT32 reader against our own FAT32 writer could produce a self-consistent but incompatible implementation.

Therefore, FAT32 output must be independently validated using real external tools such as:

* Linux `fsck.vfat`
* mtools
* Other standards-compliant filesystem utilities where appropriate

M26 followed this principle by creating a real FAT32 image, writing files with the JetOS FAT32 implementation, and then independently verifying the resulting filesystem with Linux tools.
