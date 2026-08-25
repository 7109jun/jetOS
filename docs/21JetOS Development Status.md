# JetOS Development Status

> Last updated: Milestone 21 completed
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation + linking passed, plus **actual QEMU boot with serial log and screenshot verification**
>
> This document is written so that the current state of JetOS can be understood completely using only this document and the latest tar file (`JetOS_Milestone21.tar`).

---

## Philosophy

> **"An OS for people who want to use Windows but cannot afford it."**

Milestone 21 is a purely visual/UX improvement milestone:

1. Redrew the mouse cursor in the classic Windows-style arrow.
2. Redrew the wallpaper, taskbar, and Start button with gradients to satisfy the requirement that **nothing should ever be a solid color**.
3. Increased the default JETFS/USB disk size from 16MB/32MB to 128MB.
4. Added a new multicolor gradient sunset wallpaper with a sun/moon.
5. Rebuilt the taskbar with a gray star Start button instead of the Windows four-square logo, a weather widget on the left, and volume/time/network widgets on the right.

---

## Build & Run

Same as the M20 documentation, except that the default disk image sizes have been increased:

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128     # 128MB (previously 16MB)
dd if=/dev/zero of=build/usb.img bs=1M count=128         # 128MB (previously 32MB)
mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

The instructions in `scripts/build_test_native_elf.sh`, `scripts/build_test_exe.sh`, and `scripts/run_qemu.sh` have also been updated to use the 128MB sizes.

---

## Directory Structure

### Changes from M20

```text
JetOS/
├── kernel/gui/
│   ├── wm.c
│   │   M21: Cursor redesign, new fill_gradient_h() helper,
│   │        multicolor draw_wallpaper(), gradients for the clock,
│   │        Start button, and taskbar, plus new draw_star(),
│   │        draw_weather_icon(), draw_speaker_icon(), and
│   │        draw_network_icon() helpers.
│   │        Taskbar layout redesigned with a star Start button,
│   │        weather widget, and volume/time/network widgets.
│
├── scripts/
│   ├── build_test_native_elf.sh
│   ├── build_test_exe.sh
│   └── run_qemu.sh
│       M21: Default image sizes updated from 16/32MB to 128MB.
```

All other directories remain identical to M20. See the previous documentation for the complete structure.

---

## Milestone Summary

### M21 — GUI Visual Overhaul

| #  | Title               | Key Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| -- | ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 21 | GUI Visual Overhaul | ① Redesigned the cursor as a 13x16 Windows-style classic arrow with white fill and black outline. ② Replaced the single-tone gradient wallpaper with a three-color indigo → magenta → orange sunset gradient and a circular gradient sun/moon. ③ Applied horizontal gradients to the taskbar, Start button, and window-list buttons. Requirement: "Colors must never be solid." ④ Increased default JETFS/USB image sizes from 16MB/32MB to 128MB. ⑤ Rebuilt the taskbar with a gray five-point star Start button instead of the Windows four-square logo, a weather widget immediately to its right, and volume/time/network widgets on the far right. |

---

## Complete Functionality

### M1–20

All functionality from the M20 documentation remains intact.

At the completion of Milestone 21, the system was re-verified:

* All boot self-tests passed.
* The system successfully reached the desktop GUI.
* Visual output was verified through QEMU screenshots.

### Milestone 21 Changes

* **Mouse cursor:** 13x16 bitmap-based Windows-style classic arrow. Compared with the previous 11x11 cursor, which looked more like an unclear blob, the new cursor is significantly sharper and easier to recognize.
* **Wallpaper:** Three-color vertical gradient from indigo → magenta → orange sunset, with a circular gradient sun/moon in the upper-right. The center is cream-colored and transitions toward the sunset colors at the edges. The existing 2px band rendering method is retained to keep per-frame redraws lightweight.
* **No-solid-color rule:** Taskbar background, Start button, and window-list buttons are all rendered using the `fill_gradient_h()` horizontal gradient helper. The title bar had already used gradients since M17.
* **Disk capacity:** Default JETFS and USB image sizes increased to 128MB from 16MB/32MB.
* **Taskbar redesign:**

  * Start button: Gray five-point star icon instead of the Windows logo + `"START"` text.
  * Left side, immediately after the Start button: Weather widget with a sun/cloud icon and temperature text.

    * **Known simplification:** The displayed value is fixed at `"22C"` because there is currently no real weather API integration.
  * Far right: Volume icon → time → network icon.

    * Volume: Decorative speaker + sound-wave icon. Fixed display because there is currently no audio driver.
    * Time: Existing real-time clock functionality.
    * Network: Four-bar signal icon. **This is the only real-data widget:** it uses `net_available()` to reflect the actual NIC state, displaying green when connected and gray otherwise.

---

## Problems Actually Found and Fixed

### 13. Taskbar Text/Icon Overlap (M21)

During the first rendering verification, the `"START"` text and weather icon were positioned too closely and appeared almost overlapped.

The gap was increased from 8px to 20px, resolving the issue.

This was another case demonstrating why visual verification through QEMU screenshots is necessary.

---

## Known Limitations / Incomplete Features

All limitations listed in the M20 documentation remain valid, including:

* Certificate verification
* ECDHE
* TLS 1.3
* FAT32 write support
* Process resource reclamation
* `fork`/`exec`
* And other previously documented limitations

### New M21 Limitations

| Item           | Status                                                                                                                                                                                                                      |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Weather widget | Decorative fixed value (`"22C"`). No real weather data is currently available. To provide live weather information, a weather API needs to be integrated using the HTTP(S) client already implemented in M19/M20.           |
| Volume widget  | Decorative only. There is currently no audio driver, so real volume control or volume information is unavailable. It can be converted to real data once an audio driver exists.                                             |
| Taskbar space  | If many windows are open (approximately 6–7 or more), window-list buttons may overlap the right-side widget cluster. This simplification existed previously, but the additional widgets reduce the available space further. |

---

## Next Tasks

Priority order, continuing from the M20 documentation:

1. **Certificate verification (trust chain + hostname)** — Still the highest-priority security task.
2. **Make the weather widget use real data** — The HTTP(S) client already exists, so integrating a public weather API should be relatively straightforward.
3. **FAT32 write support.**
4. **ECDHE support.**
5. **Resource reclamation when processes terminate.**
6. **Full adoption of a reusable widget toolkit.**
7. **Audio driver** — Once implemented, the volume widget can be converted to real data.
8. **Taskbar window-list overflow handling** — Prevent window buttons from overlapping the widget cluster when many windows are open.

---

## Code Style / Design Principles

All principles from the M20 documentation remain valid:

* Three-file assembly rule
* Boot self-tests
* Host harness tests
* Full rebuild verification after header changes
* Careful handling of use-after-free issues
* Documentation of intentional simplifications

### Additional M21 Principle

* **Visual changes must be verified with screenshots.**

Changes involving colors, layout, rendering, and other visual elements cannot be adequately verified through serial logs alone. Such changes must be visually checked using QEMU's `screendump` functionality.

The M21 taskbar overlap bug demonstrated the practical necessity of this rule.
