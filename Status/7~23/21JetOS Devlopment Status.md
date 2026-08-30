# JetOS Development Status

> Last updated: Milestone 21 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation + linking passed, followed by **actual QEMU boot verification with serial logs/screenshots**
>
> This document is written so that the current state of JetOS can be fully understood using only this document and the latest tar file (`JetOS_Milestone21.zip`).

---

## Philosophy

> **"An OS for people who want to use Windows but don't have the money."**
>
> Milestone 21 is a purely visual/UX-focused milestone:
> ① Redesign the mouse cursor as a Windows classic-style arrow
> ② Redraw the wallpaper, taskbar, and Start button with gradients according to the requirement that "it must never be completely flat-colored"
> ③ Increase the default disk (JETFS/USB) capacity from 16MB/32MB to 128MB
> ④ Add a new multicolor gradient wallpaper featuring a sunset and a sun/moon
> ⑤ Reorganize the taskbar with a gray star-shaped Start button instead of the Windows four-pane logo, a weather widget on the left, and volume/time/network widgets on the right.

---

## Build & Run

Same as the M20 documentation, except that the default disk image size has been increased:

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128     # 128MB (previously 16MB)
dd if=/dev/zero of=build/usb.img bs=1M count=128         # 128MB (previously 32MB)
mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

The instructions and messages in `scripts/build_test_native_elf.sh`, `scripts/build_test_exe.sh`, and `scripts/run_qemu.sh` have also been updated to use 128MB as the default size.

---

## Directory Structure (M21 Changes Only)

```text
JetOS/
├── kernel/gui/
│   ├── wm.c    M21: redesigned cursor, added fill_gradient_h() helper,
│   │            multicolor draw_wallpaper(), gradients for time/Start button/
│   │            taskbar, added draw_star()/draw_weather_icon()/
│   │            draw_speaker_icon()/draw_network_icon() helpers,
│   │            reorganized taskbar layout
├── scripts/
│   ├── build_test_native_elf.sh, build_test_exe.sh, run_qemu.sh
│   │            M21: default image size updated from 16/32MB to 128MB
```

(All other directories remain identical to M20 — see the previous documentation.)

---

## Milestone Summary (M21 Addition)

| #  | Title               | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| -- | ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 21 | GUI Visual Overhaul | ① Redesigned the cursor as a 13x16 Windows classic arrow with a white fill and black outline ② Replaced the single-tone gradient wallpaper with a three-color vertical gradient from indigo → magenta → orange sunset, with a radial-gradient sun/moon in the upper-right ③ Applied horizontal gradients to the taskbar, Start button, and window-list buttons using `fill_gradient_h()` (requirement: "colors must never be completely flat") ④ Increased default JETFS/USB image capacity from 16MB/32MB → 128MB ⑤ Reorganized the taskbar: gray five-point star Start button (instead of the Windows four-pane logo) + weather widget on the far left + volume/time/network widgets on the far right |

---

## Current Functionality

### All M1–20 Features

(Same as the M20 documentation — everything remains supported. At the end of Milestone 21, all features were rechecked: all boot self-tests passed, the desktop GUI was reached successfully, and the visual result was verified through screenshots.)

### Milestone 21 Changes

* **Mouse cursor**: 13x16 bitmap-based Windows classic arrow. Compared with the previous 11x11 cursor, which looked more like a small blotch, the new cursor is significantly clearer and easier to recognize.
* **Wallpaper**: Indigo → magenta → orange sunset three-color vertical gradient + radial-gradient sun/moon in the upper-right (cream-colored center → sunset-colored edges). The existing 2px band rendering method is retained, keeping the redraw lightweight even when rendered every frame.
* **No-flat-color rule applied**: The taskbar background, Start button, and window-list buttons are all rendered with the `fill_gradient_h()` helper using horizontal gradients. (The title bar has already used gradients since M17.)
* **Disk capacity**: Default JETFS/USB image size increased to 128MB (previously 16MB/32MB).
* **Taskbar reorganization**:

  * Start button: gray five-point star icon (instead of the Windows four-pane logo) + `"START"` text
  * Far left, immediately after the Start button: weather widget (sun + cloud icon + temperature text) — **known simplification**: fixed `"22C"` display because there is no real weather API integration.
  * Far right: volume icon (speaker + sound waves, fixed display — no audio driver) → time (existing clock) → network icon (four signal bars, **the only real-data widget**: reflects actual NIC status using `net_available()`, showing green when connected and gray otherwise)

---

## Problems Found and Fixed (M21)

13. **Taskbar text/icon overlap** (M21): During the first rendering check, the `"START"` text and weather icon appeared almost touching and visually overlapped. After verifying the issue through a screenshot, the spacing was increased from 8px to 20px, resolving the problem. This was a practical example of why QEMU screenshot-based visual verification is necessary.

---

## Known Limitations / Incomplete Features (M21)

All limitations listed in the M20 documentation remain valid (certificate verification, ECDHE, TLS 1.3, FAT32 write support, process resource cleanup, fork/exec, etc.). The following limitations were added in M21:

| Item           | Status                                                                                                                                                                                                                                        |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Weather widget | Decorative fixed value (`"22C"`) — no real weather data. To receive real-time data from the Internet, a weather API needs to be connected using the HTTP(S) client already implemented in M19/M20.                                            |
| Volume widget  | Decorative only — there is no audio driver, so actual volume adjustment/status is not available. Once an audio driver is implemented, this can be replaced with real data.                                                                    |
| Taskbar space  | If many windows are open (approximately 6–7 or more), the window-list buttons may overlap the right-side widget cluster. This was already a simplification, but the additional widgets introduced in M21 further reduced the available space. |

---

## Next Tasks (Priority Order)

1. **Certificate verification (trust chain + hostname)** — still the highest-priority security task.
2. **Make the weather widget use real data** — the HTTP(S) client already exists, so connecting a public weather API should be a relatively small task to turn the widget from decorative into functional.
3. **FAT32 write support**.
4. **ECDHE support**.
5. **Resource cleanup when processes terminate**.
6. **Apply a reusable widget toolkit throughout the GUI**.
7. **Audio driver** — once implemented, the volume widget can also be switched to real data.
8. **Taskbar window-list overflow handling** — prevent the window buttons from overlapping the widget cluster when many windows are open.

---

## Code Style / Design Principles

The M20 principles remain fully applicable, including the three-file assembly rule, boot self-tests, host-harness tests, full rebuild verification after header changes, use-after-free precautions, and documentation of intentional simplifications.

M21 adds the following principle:

* **Visual changes must be verified with screenshots**: Changes involving colors or layout that cannot be validated through serial logs must be visually checked using QEMU's `screendump`. The taskbar overlap issue described above demonstrated why this is necessary.
