# JetOS Development Status

> Last updated: Milestone 22 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation + linking passed + **actual QEMU boot with serial logs/screenshots verified**
>
> This document + the latest tar file (`JetOS_Milestone22.tar`) are sufficient to fully understand the current state.

---

## Philosophy

> **"An OS for people who want to use Windows but don't have the money."**

Milestone 22 addressed two requested tasks: ① audio (PC speaker tone generation — true digital audio is scoped out as a separate TLS-level task, as explained below) and ② a JETFS upgrade (double indirect blocks, expanding the file size limit from 4.2 MB to a level that can effectively accommodate a 128 MB disk). Both tasks exposed and fixed real bugs during actual boot verification (see "Bug History" below). In particular, the JETFS issue was a serious performance bug that made booting appear to have completely stalled.

---

## Build & Run

Same as the M21 documentation. No additional tools or scripts are required; the existing build flow remains unchanged.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

New jash commands: `beep [freq] [ms]`, `melody <scale|fanfare>`.

### Audio Capture Notes

The QEMU build installed in this development sandbox does not include the `isa-pcspk` (PC speaker) audio device itself (`qemu-system-x86_64 -device help` does not list it at all). Therefore, this environment could only verify that the port I/O sequence was correct; actual waveform/sound output could not be captured. Additional verification is required on real hardware or with another QEMU build that includes audio support (`-audiodev wav,...` + `-machine pc`).

---

## Directory Structure (M22, Changes from M21 Only)

```text
JetOS/
├── kernel/drivers/
│   ├── speaker.c/h    M22 new: PC speaker (PIT channel 2 + port 0x61) tone/melody playback
├── kernel/fs/
│   ├── jetfs.h         M22: added double_indirect field to jetfs_inode_t,
│   │                     recalculated JETFS_MAX_FILE_BLOCKS (4.2 MB → ~4.3 GB)
│   ├── jetfs.c         M22: added double-indirect handling to
│   │                     file_block_map/file_block_free/file_free_all_blocks.
│   │                     Fixed bitmap_alloc_block performance bug
│   │                     (see Bug History #14).
├── kernel/gui/
│   ├── desktop.c       M22: plays a boot-complete fanfare (3 notes) when the GUI starts
├── kernel/apps/
│   ├── jash.c/h        M22: added beep/melody commands
├── kernel/kernel_entry.c   M22: added a JETFS double-indirect self-test
│                            (verifies writing/reading a 4.5 MB file)

```

(All other directories remain the same as M21 — see the previous documentation.)

---

## Milestone Summary (M22 Addition Only)

| #  | Title                 | Core Content                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| -- | --------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 22 | Audio + JETFS Upgrade | ① New PC speaker driver (PIT channel 2 + port 0x61 square-wave tone), jash `beep`/`melody` commands, boot-complete fanfare. ② Added double-indirect blocks to JETFS, increasing the file size limit from 4.2 MB to approximately 4.3 GB (effectively limited by disk capacity). ③ Bug fixes: missing leaf block reclamation when deleting entire files with double-indirect blocks, and an O(n²) performance bug in JETFS bitmap allocation. |

---

## Complete Feature List Currently Working (M21 + M22 Changes)

### All M1–21 Features

(Same as the M21 documentation — all features remain intact. At the end of Milestone 22, all were re-verified: all boot self-tests passed, the desktop GUI was reached successfully, and visual behavior was confirmed with screenshots — no regressions.)

### New in Milestone 22

* **PC Speaker Audio**: `speaker_on/off/beep/play_melody` APIs. jash `beep [freq] [ms]` (default: 880 Hz / 300 ms), `melody scale` (8 notes: Do-Re-Mi-Fa-Sol-La-Ti-Do), `melody fanfare` (3 ascending notes). The GUI automatically plays a short fanfare when desktop initialization completes. **Known limitation**: pure square-wave monophonic tones only (no polyphony), and digital audio (such as WAV) cannot be played. Supporting that would require an AC97/HDA sound card driver + DMA + audio format decoding, which is a separate large-scale task comparable to the TLS stack and is therefore out of scope.
* **JETFS Double-Indirect Blocks**: Maximum file size increased from 4.2 MB to approximately 4.3 GB (effectively limited by disk capacity). This upgrade matches the 128 MB default disk size introduced in M21 — a single file can now nearly fill the entire disk. A 4.5 MB file (well beyond the previous limit) is written and read during every boot, with byte-level comparison to verify correctness.

---

## Bugs Actually Discovered and Fixed (M22)

### 13. **JETFS Double-Indirect Leaf Block Leak When Deleting an Entire File** (M22)

When double-indirect block support was first implemented, `file_free_all_blocks()` reclaimed only the "indirect blocks" referenced by the double-indirect block, but did not reclaim the **actual data (leaf) blocks** referenced by those indirect blocks. In other words, repeatedly creating and deleting large files would silently leak disk space. This was discovered during a code review immediately after implementation. The deletion logic was fixed to read each indirect block and traverse all leaf pointers within it, reclaiming every referenced data block.

### 14. **JETFS Bitmap Allocator O(n²) Performance Bug** (M22, Most Serious)

The `bitmap_alloc_block()` function used to find a free data block by:

1. Starting the scan from bit 0 on every allocation.
2. Re-reading the disk block containing a bit every time an individual bit was checked.

This meant that even when checking 32,768 bits within the same disk block, `block_read` was performed repeatedly for every single bit.

When files were limited to a maximum of 1,036 blocks (~4.2 MB), this was tolerable. However, after the double-indirect upgrade greatly increased the possible file size, writing the 4.5 MB self-test file required approximately 1,152 block allocations. Each allocation repeatedly scanned and re-read increasingly large portions of the bitmap from the beginning, effectively creating **O(n²) disk I/O**. As a result, booting appeared to hang for more than 45 seconds.

The initial investigation was complicated by a separate bootloader crash caused by a 5 MB static buffer, described in Bug #15 below.

The fix reused the "next free position hint" pattern already used by the PMM (`kernel/mm/pmm.c`):

1. Scan starting from the hint position.
2. Read each bitmap disk block only once, then scan all 32,768 bits contained within it.

After the fix, the same self-test completed within a few seconds.

### 15. **Large Static Array in Kernel Image Causing Bootloader Crash** (M22)

For the self-test, two 5 MB buffers (one for writing and one for reading) were initially declared directly in the kernel source as:

```c
static uint8_t buf[5*1024*1024];
```

This increased the kernel ELF image's BSS by several megabytes. JetOS's bootloader loads the kernel at a fixed physical address of 2 MiB using `AllocateAddress`. As the image grew, that region overlapped with the UEFI buffer containing the `KERNEL.ELF` file that had just been loaded. Physical page allocation therefore failed:

```text
FATAL: kernel segment physical page allocation failed
```

The system could not boot at all.

The solution was to replace the static arrays with `kmalloc()` allocations at runtime. The heap receives pages separately from the PMM, so the kernel image size is no longer affected.

**Lesson:** With this bootloader design, do not place large static buffers directly in the kernel source. Use `kmalloc()` for large test/runtime buffers.

---

## Known Limitations / Incomplete Features (M22)

All limitations from the M21 documentation remain valid. New M22 limitations:

| Item                               | Status                                                                                                                                                                                             |
| ---------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Digital audio (WAV, etc.) playback | Not available — only monophonic PC speaker square waves. AC97/HDA driver + DMA + audio decoding would be required; this is a separate large-scale task comparable to the TLS stack.                |
| Audio polyphony                    | Not supported — the PC speaker hardware itself is fundamentally limited to one frequency at a time.                                                                                                |
| Real audio verification            | The QEMU build in this sandbox does not contain the `isa-pcspk` audio device, so only port I/O logic was verified; actual sound/waveform output was not verified.                                  |
| JETFS performance                  | The hint-based allocator is still somewhat linear during the initial scan for each file. It is simpler than a production-grade tree-based free-space map, but sufficient for the current use case. |
| Volume widget (taskbar)            | Still decorative since M21 — the presence of an audio driver does not automatically make it reflect actual playback state; separate integration work is required.                                  |

---

## Next Tasks (Priority Order, Continuing from M21 Documentation)

1. **Certificate verification (trust chain + hostname)** — still the highest-priority security task.
2. **Make the weather widget use real data.**
3. **FAT32 write support.**
4. **ECDHE support.**
5. **AC97/HDA digital audio** — the next step beyond the PC speaker. Once WAV playback is available, there will be a reason to make the taskbar volume widget reflect actual audio state.
6. **Resource reclamation when processes terminate.**
7. **Full adoption of a reusable widget toolkit.**
8. **Real-world audio verification** — verify that `beep`/`melody` can actually be heard on real hardware or a QEMU build with audio support.

---

## Reference: Code Style / Design Principles (Consistency)

All principles from M21 remain valid:

* Three assembly files.
* Boot self-tests.
* Host harness tests.
* Full rebuild verification after header changes.
* Screenshot verification for visual changes.
* Careful attention to use-after-free issues.
* Document simplifications.

Additional M22 principles:

* **Do not put large static buffers in the kernel image**: Because the bootloader loads the kernel at a fixed physical address, increasing the image size can cause it to overlap with temporary buffers used by the bootloader and make boot fail entirely (Bug #15). Large test/runtime buffers must always be allocated with `kmalloc()`.
* **Whenever increasing the size of a data structure or capacity limit (file size, disk size, etc.), re-evaluate the performance assumptions in all functions that operate on that data structure**: As demonstrated by Bug #14, an O(n) operation that was harmless when the scan range was small can become an effectively O(n²) operation once the scale increases.
