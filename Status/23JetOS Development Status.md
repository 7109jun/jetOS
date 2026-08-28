# JetOS Development Status

> Last updated: Milestone 23 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compile/link verification + **actual QEMU boot with serial logs/screenshots**
>
> * Milestone 23 also added a **host sparse-file harness test** for large-file scenarios.
>
> This document + the latest tar file (`JetOS_Milestone23.tar`) are intended to provide everything needed to fully understand the current state of the project.

---

## Philosophy

> **"An OS for people who want to use Windows but cannot afford it."**
>
> Milestone 23 has two main goals:
> ① Remove as much of the "pixel art" appearance from the GUI as possible (font/shape anti-aliasing)
> ② Increase the maximum JETFS file size to 20GB (with a structural capacity far beyond that).
>
> The second part conceptually references the design ideas behind ReactOS and Linux/ext2 — specifically, the classic idea that "adding another level of blocks containing pointers increases the addressable space by a factor of 1024 each time" (direct + single indirect + double indirect + triple indirect blocks). The implementation was written from scratch in the style of this project; no code was copied or ported.

---

## Build & Run

Same as the M22 documentation. No additional steps are required.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

### Actually testing 20GB-class files

JETFS can now structurally support a single file of approximately **4TB**, but the actual usable file size is limited by the size of the disk image itself.

To actually test a 20GB file, the disk image itself must be large enough:

```bash
./build/mkjetfs build/jetfs.img 20480   # 20480MB = 20GB
```

**Warning:** `tools/mkjetfs.c` (the tool actually distributed with the project) writes zeros across the entire data area when formatting; it does **not** create a sparse file. Therefore, specifying 20GB really consumes approximately 20GB of physical disk space.

If you are working in a development sandbox with limited free disk space and want to experiment with large images, see the sparse-file host testing methodology below.

### Verification Methodology (Added in M23)

In addition to the three methods used through M22 (host harness tests / QEMU boot + serial logs / QEMU monitor + screendump), M23 adds:

6. **Large files are verified using a host sparse-file harness:**
   Crossing the triple-indirect boundary (approximately **4.004GB**) with real data would require writing several gigabytes. Doing this directly through the QEMU boot test is impractical in this sandbox because of both execution time and available disk space.

   Instead, `kernel/fs/jetfs.c` is compiled directly with host GCC, while only `ahci_read/write_sectors` are stubbed out and replaced with `pread`/`pwrite` operations against a host file.

   The backing disk is a **real sparse file** whose size is established using `ftruncate`; untouched regions therefore consume virtually no physical disk space.

   This setup was used to create and verify an actual **4.3GB file** while exercising the exact same `jetfs.c` code that is compiled into the kernel. This is therefore not a separate reimplementation for testing, but rather the production filesystem code running against a different I/O backend.

---

## Directory Structure

### M23 changes compared with M22

```text
JetOS/
├── kernel/drivers/
│   ├── console.c/h
│   │                    M23: Replaced with an integer-only
│   │                    bilinear-interpolation anti-aliased glyph renderer.
│   │                    Added the public console_blend_pixel() API
│   │                    (blends a color into the actual framebuffer value
│   │                    using coordinates + coverage (0–256)).
│
├── kernel/gui/
│   ├── wm.c
│   │                    M23: Applied isqrt-based subpixel anti-aliasing
│   │                    to the edges of the sun/moon circles in the
│   │                    wallpaper, removing visible stair-stepping.
│
├── kernel/fs/
│   ├── jetfs.h
│   │                    M23: Expanded jetfs_inode_t.size from uint32_t
│   │                    to uint64_t. Added the triple_indirect field.
│   │                    Recalculated JETFS_MAX_FILE_BLOCKS to include
│   │                    triple-indirect blocks (~4.0GB → ~4.0TB).
│   │
│   ├── jetfs.c
│   │                    M23: Added triple-indirect handling to
│   │                    file_block_map/free/free_all_blocks.
│   │                    Unified write/append logic through file_write_from().
│   │                    Added the new jetfs_append() API for appending
│   │                    large files in chunks.
│
├── include/jetapi/jetapi.h
├── kernel/jetapi/jetapi.c
│   │                    M23: Expanded ReadFileJ/WriteFileJ/
│   │                    JetFileListCallback size parameters to uint64_t
│   │                    to propagate the JETFS API changes.
│
├── kernel/apps/jash.c
├── kernel/apps/file_manager.c
├── kernel/shell/terminal.c
├── kernel/gui/desktop.c
├── kernel/exec/pe_loader.c
├── kernel/exec/elf_loader.c
├── kernel/compat/win32.c
│                        M23: Updated all call sites to match the expanded
│                        jetfs/jetapi size parameters.
│                        This also fixed pointer type mismatches where
│                        uint32_t* values were passed to uint64_t APIs.
│                        Such mismatches can compile with only a warning
│                        in C and can otherwise lead to stack memory
│                        corruption.
```

(All other directories remain the same as M22. See the previous documentation.)

---

## Milestone Summary

| #  | Title                      | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| -- | -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 23 | Anti-Aliasing + JETFS 20GB | ① Integer-only bilinear interpolation for smoother font rendering, greatly reducing the pixel-art appearance; subpixel anti-aliasing for the sun/moon edges in the wallpaper. ② Added triple-indirect blocks to JETFS (conceptually inspired by ext2/Linux, with the code written from scratch), expanding the structural file-size limit from ~4GB to ~4TB, comfortably exceeding the requested 20GB target. Expanded `inode.size` to 64-bit and added `jetfs_append()`. ③ Fixed an anti-aliasing coverage calculation shift bug and corrected pointer type mismatches caused by expanding jetfs/jetapi size parameters to 64-bit. |

---

## Complete Current Feature Set

### M1–22

All features from the M22 documentation remain available.

At the end of Milestone 23, they were re-verified:

* All boot self-tests pass.
* The system successfully reaches the desktop GUI.
* Visual output was verified using screenshots.
* No regressions were observed.

### New in Milestone 23

* **Anti-aliased font rendering:**
  The 5×7 bitmap font is enlarged using integer-only bilinear interpolation, producing significantly smoother diagonal and curved shapes than the previous nearest-neighbor scaling. Because the background framebuffer value is read before blending, the result works naturally regardless of what is behind the text, including gradients.

* **Anti-aliased circular shapes:**
  The sun/moon circles in the wallpaper use integer square-root (`isqrt`) based subpixel blending, eliminating visible stair-stepping along their edges.

* **JETFS triple-indirect blocks:**
  The structural maximum file size has been expanded from approximately 4GB to approximately 4TB. This comfortably supports the requested 20GB-per-file target. However, actually storing a 20GB file still requires a disk image large enough to contain it.

* **`jetfs_append()`:**
  A new API for appending data to the end of a file. Since the kernel has limited RAM (256MB in the test environment), attempting to write a 20GB file through a single `jetfs_write()` call is inherently impractical. `jetfs_append()` allows large files to be created incrementally using chunks.

---

## Bugs Actually Found and Fixed in M23

### 16. Anti-Aliasing Coverage Shift Bug Caused All Text to Disappear

The bilinear interpolation coverage was normalized to the 0–256 range using:

```c
total >> 16
```

However, the actual maximum value of `total` was **65536 (= 2^16)**, meaning the correct operation was:

```c
total >> 8
```

The extra 8-bit shift caused every glyph's coverage to become zero. As a result, all text became completely transparent and disappeared from the screen.

The issue was discovered after the first QEMU screenshot showed that icons were visible but all their label text was missing.

A small host-side test program was then used to print the coverage map of a single `'A'` glyph. This confirmed the cause. After the fix, the same test produced a normal gradient coverage map.

---

### 17. Prevented a Recurrence of the Static Large-Test-Buffer Bootloader Collision

This was originally encountered in M22.

While planning how to validate the 20GB scenario in M23, it became clear that attempting to write a real 20GB dataset directly during the QEMU boot self-test could recreate the same problem as M22 bug #15: a large static buffer inside the kernel image could collide with the bootloader's fixed-address loading area.

Even using `kmalloc()` would not solve the fundamental problem, because the kernel's available RAM in the test environment is only approximately 256MB. A 20GB in-memory buffer is obviously impossible.

Therefore, instead of attempting to "prove" 20GB support through the boot self-test itself, the project adopted the host sparse-file harness approach.

This was a deliberate and documented scope decision rather than an attempt to hide the limitation.

---

### 18. Discovered That `mkjetfs` Uses Real Zero-Filling Instead of Sparse Allocation

While preparing a 4.3GB test image using the actual distributed `mkjetfs.c`, it was discovered that the tool writes zeros across the entire data area.

This consumed nearly all available disk space in the sandbox. At the time, only approximately 3.6GB of free space remained.

The oversized test image was immediately removed to recover the disk space.

A separate sparse-only host testing tool was then created. It uses `ftruncate()` to establish the disk size and explicitly writes only the small regions that actually need non-zero data, such as:

* the superblock
* inode tables
* bitmaps

The sparse approach is used **only by the test tool**.

The actual distributed `tools/mkjetfs.c` intentionally continues to zero-fill the entire disk image for safety and predictability. This means that anyone creating a real 20GB JETFS image must have approximately 20GB of actual free disk space available.

---

## Known Limitations / Incomplete Areas

All limitations listed in the M22 documentation remain valid.

The following limitations were added or updated in M23:

| Item                         | Status                                                                                                                                                                                                                                                                                                                                                                                                       |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Anti-aliasing coverage       | Anti-aliasing is currently applied to font rendering and the sun/moon circles in the wallpaper. Desktop icons (folders, terminal, diamonds, etc.) and taskbar icons (stars, clouds, speakers, signal bars) do not yet use anti-aliasing. These are mostly simple geometric shapes and therefore appear less jagged even without it.                                                                          |
| 20GB file real-world usage   | Structurally supported, but actually creating a 20GB file requires a disk image of at least that size. The distributed `mkjetfs` is not sparse and therefore requires real disk writes for the entire image. This may take considerable time.                                                                                                                                                                |
| Triple-indirect shrink       | When a file is rewritten to a smaller size, double-indirect structures are reclaimed where applicable. However, in an extreme shrink operation (for example, 4GB+ → a few bytes), the entire now-unnecessary triple-indirect tree is not immediately reclaimed. The remaining tree is eventually reclaimed when the file itself is deleted through `file_free_all_blocks()`. This is a known simplification. |
| `jetfs_append()` performance | Each call currently reopens the file, looks up the inode again, and walks the final blocks again. Hundreds of thousands of very small append operations could therefore be inefficient. Larger chunks (for example, tens to hundreds of MB) are recommended to reduce the number of calls.                                                                                                                   |

---

## Next Tasks

In priority order, continuing from the M22 roadmap:

1. **Certificate verification (trust chain + hostname verification)** — still the highest-priority security task.
2. **Apply anti-aliasing to the remaining icons** — taskbar stars/clouds/speaker/signal bars.
3. **FAT32 write support.**
4. **ECDHE support.**
5. **AC97/HDA digital audio.**
6. **Resource reclamation when processes terminate.**
7. **Apply the reusable widget toolkit throughout the GUI.**
8. **Real-world 20GB scenario verification** — on a system with more than 20GB of available disk space, create a genuine 20GB JETFS image using `mkjetfs`, boot it in QEMU, and use `usbimport`/`httprun` and related functionality to handle large files end-to-end.

---

## Code Style / Design Principles

All principles from M22 remain valid:

* Three assembly files.
* Boot self-testing.
* Host harness testing.
* Full rebuild after header/API changes.
* Screenshot verification for visual changes.
* Avoid large static buffers.
* Use `kmalloc()` for dynamically sized buffers.
* Be careful about use-after-free.
* Document intentional simplifications.

M23 adds the following principles:

### 1. When referencing other projects, use concepts — not code

When studying well-known designs from projects such as ReactOS or Linux, only adopt the underlying concepts.

For example, when implementing the triple-indirect block system, understand **why** the design works and then implement it from scratch using this project's existing coding style, variable naming, comments, and error-handling conventions.

Do not copy or directly port source code or reproduce another project's internal structure.

### 2. When widening API size/count parameters, update every call site — including pointer types

When changing an API from `uint32_t` to `uint64_t`, search the entire codebase for all callers and make sure the pointer types are updated as well.

In C, passing a `uint32_t*` where a `uint64_t*` is expected may result in a compiler warning rather than an error. If left unfixed, this can cause silent memory corruption, including stack corruption.

After the change:

```bash
grep -i warning
```

should be used to verify that no relevant warnings remain.

### 3. Validate large-data scenarios using host sparse-file harnesses

For large-data scenarios, compile the actual production code (such as `jetfs.c`) directly with host GCC and replace only the low-level I/O functions with test stubs.

Use a sparse backing file created with `ftruncate()` so that GB- to TB-scale scenarios can be tested without consuming the entire available disk space.

This keeps the tested filesystem logic identical to the production kernel implementation while making large-scale testing practical in constrained development environments.
