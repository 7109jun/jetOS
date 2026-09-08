# JetOS Development Status

> Last updated: Milestone 43 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU 8.2 + OVMF
> (newly installed in the verification sandbox during this session)
> Current directive: "Fix the window/taskbar overflow bug, Unicode rendering,
> strengthen network encryption, JetOS self-hosted development environment,
> assembly compiler, and anything else considered important."

---

## Milestone 43 — Native jcc Port: JetOS Runs Its Own Compiler

**The first complete version of a "JetOS can develop JetOS" environment.**
The jcc from M30 was previously a host tool built with host gcc. It could generate code for JetOS, but the compilation itself ran on the host CPU. This time, `jcc.c` was made capable of being built as an ELF64 program that runs directly in Ring3 on JetOS.

* Added `tools/jetos_native_shim.h` — a minimal runtime providing `malloc/calloc/realloc` (a bump allocator built on SYS_BRK), file I/O (`SYS\_OPEN/READ/WRITE\_FD/CLOSE`), `strlen/strcmp/memcpy/memset/strdup/strtol`, using only JetOS system calls without libc.
* Wrapped `jcc.c` with the `__JETOS_NATIVE__` macro so it can be built both as a host binary (unchanged, with no regressions) and as a native binary (reusing `tools/testdata/elf_link.ld`, with options such as `-nostdlib -static -mgeneral-regs-only`).
* Because `elf_loader.c` did not yet pass `argv/argc` (a known limitation), native jcc used fixed paths (`/jcc_in.c` → `/jcc_out.elf`) for this milestone.

**Two actual crashes were discovered and fixed during real execution testing:**

1. **General Protection Fault** — gcc -O2 automatically vectorized the malloc/memcpy copy loop using SSE (`movdqa`), but the kernel did not initialize SSE state for Ring3 processes. This was fixed by applying `-mgeneral-regs-only`, already used by the kernel build, to the native jcc build as well. objdump confirmed that all SSE instructions were completely eliminated.
2. **Page Fault (NULL dereference)** — the shim's `realloc()` did not follow the standard C rule (`realloc(NULL, size) == malloc(size)`). When the token array performed its first allocation with the old pointer set to NULL, it attempted to read a nonexistent old block. Adding a NULL check fixed the issue.

Both bugs were identified precisely using `[USER FAULT] ... rip=...` kernel logs and objdump disassembly, then fixed.

**Final real-execution verification (complete loop):** On actual JetOS running inside QEMU —

```text
jash /> run /jcc.exe
launched (native ELF, isolated address space).
jash /> jcc: /jcc_in.c -> /jcc_out.elf compilation complete
jash /> run /jcc_out.elf
launched (native ELF, isolated address space).
Compiled BY jcc running ON JetOS itself! sum(0..10) check below.
RESULT: 55 (correct)
```

The jcc running on JetOS read C source code, compiled it, and the resulting program ran again on JetOS and produced the correct result. JetOS completed the entire **"write source → compile → execute"** cycle by itself, without relying on the host computer.

### Known Limitations (M43)

| Item                                                 | Status                                                                                           |
| ---------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| `elf_loader.c` does not pass `argv/argc`             | Reason native jcc/jas use fixed paths — next candidate for implementation                        |
| Native jcc `malloc` has no `free()` (bump allocator) | Practically acceptable because jcc is short-lived; host jcc also originally did not use `free()` |
| jas has not yet been built natively                  | `jetos_native_shim.h` was designed to be shared — next candidate                                 |
| QEMU sendkey `"underscore"` key name is invalid      | `"shift-minus"` must be used instead; test infrastructure issue unrelated to JetOS               |

---

## Milestone 39 — Fundamental Fix for the Window/Taskbar "Overflow" Bug

**Cause:** `console_t` (`kernel/drivers/console.h`) had never had a clipping concept from Milestone 1 onward. `console_put_pixel` only checked the full screen dimensions, while `wm_draw()` passed the global `g_con`, which could draw across the entire screen, directly to each window's `content_cb` (the callback used by applications to draw their own window contents). If an application drew beyond its own window dimensions (`w->w`, `w->h`) due to scrolling, long text, window resizing, etc., those pixels were drawn directly over other windows, the taskbar, or the desktop — causing the "overflow" bug.

**Fix:**

* Added `clip_x0/y0/x1/y1` fields to `console_t`, along with new `console_set_clip()` / `console_reset_clip()` APIs. `console_put_pixel` now checks the clip rectangle. Since this function is the single low-level entry point for all higher-level drawing functions (`fill_rect`, `hline`, `vline`, text, widgets), **a single-point modification automatically applies clipping to the entire rendering system**.
* `wm_draw()`: narrowed the clip region to the window's client area immediately before calling `content_cb`, then restored it afterward.
* Window title labels on taskbar buttons and title-bar title text are also clipped to their respective column widths. Long titles that previously overflowed into adjacent buttons or minimize/close buttons were caused by the same underlying issue.

**Verification:**

* Host unit test (`hosttest/test_clip.c`) — included a scenario that precisely reproduces the bug (drawing a 50x50 rectangle inside a 30x30 clip must not leak outside the clip). All 5 test cases passed.
* Booted the actual system in QEMU and verified login → account creation → desktop via screenshots, with no regressions.

---

## Milestone 40 — Font: Added Essential Programming Symbols

The 5x7 bitmap font (`kernel/drivers/font5x7.h`) already contained English letters, numbers, and some symbols, but was missing all of `( ) { } [ ] < > ; = + * % & | ^ ~ # $ @ ' "`. When an unsupported character was typed, `find_glyph` silently replaced it with a blank space. As a result, even a single line of C code could not be displayed correctly in Notepad. This was a practical obstacle to the self-hosting goal.

* Added 21 missing symbols as new 5x7 bitmap glyphs.
* Changed `find_glyph` so that genuinely unknown characters are rendered as a visible placeholder box (`□`) instead of a blank. This eliminates the previous inability to distinguish between "invisible" and "unknown character."

**Verification:** During an actual QEMU boot, confirmed via screenshot that symbols such as `=`, `>`, and `+` were rendered correctly in Notepad (`F5=save F6=load Ctrl+`) and the `jash` prompt (`jash />`). Before the fix, those symbols simply disappeared.

---

## Milestone 41 — Added DHE_RSA to TLS (Forward Secrecy)

The existing TLS client (`kernel/net/tls.c`, M19/M24) only supported the static key exchange `TLS_RSA_WITH_AES_128_CBC_SHA256`. This had a fundamental limitation: if the server's private key was compromised later, previously recorded traffic could also be decrypted.

**Added:** `TLS_DHE_RSA_WITH_AES_128_CBC_SHA256` (`0x0067`) — derives the premaster secret using a new ephemeral Diffie-Hellman key pair for every connection.

* Generalized the RSA **signature verification** function (`rsa_pkcs1_verify_sha256`) and moved it from the X.509 certificate-chain verification code (`kernel/net/crypto/x509_min.c`) into `kernel/net/crypto/rsa_pkcs1.c`. This allows it to be reused for `ServerKeyExchange` signature verification without code duplication.
* Added parsing of `ServerKeyExchange` (`dh_p/dh_g/dh_Ys` + signature). Signature verification failure is treated as a potential **man-in-the-middle attack**, immediately aborting the handshake.
* Added generation of the client's ephemeral DH key pair, transmission of `Yc = g^x mod p`, and calculation of `premaster = Ys^x mod p`. All of this was implemented using the existing `bn_modexp` in `bignum.c`, requiring no additional big-number operations.
* ClientHello now prefers DHE while retaining RSA as a fallback, preserving compatibility with servers that do not support DHE.

**Verification:**

* Tested `rsa_pkcs1_verify_sha256` with a real 2048-bit RSA key/signature generated by OpenSSL. Valid signatures were accepted, while independently corrupted hashes, signatures, and public keys were correctly rejected. All 4 host test cases passed, confirming interoperability with actual OpenSSL rather than only internal compatibility.
* **Actual JetOS running inside QEMU successfully completed a real network handshake with an OpenSSL test server that enforced only `DHE-RSA-AES128-SHA256` with no RSA fallback.** It then successfully sent an encrypted HTTP GET request and decrypted the 1,571-byte response (`kernel/kernel_entry.c` built-in TLS self-test, targeting `10.0.2.2:8443`). Log messages were also updated to accurately reflect the cipher suite actually negotiated (`tls_last_cipher_has_forward_secrecy()` added).

---

## Milestone 42 — jas: JetOS Native x86-64 Assembler

Added `tools/jas.c` — an "assembly subset" host tool following the same philosophy as jcc (M30, C subset compiler). It directly emits machine code without an external assembler or linker and produces static ELF64 executables with exactly the same layout as those generated by jcc (single `PT_LOAD`, RWX, `LINK_BASE=0x100000000`).

**Supported scope** (intentionally narrow):

* Intel-style syntax (`mov dst, src`), 64-bit registers only (`rax..r15`).
* Memory operands limited to `[reg+disp]` (SIB/index/RIP-relative/`[label]` direct references are not supported — if a label address is required, it must first be loaded into a register with `mov reg, label`).
* `mov/movabs`, `add/sub/and/or/xor/cmp`, `inc/dec`, `push/pop`, `lea`, `jmp/jcc` (all `je,jne,jl,jle,jg,jge,jb,jbe,ja,jae` — all rel32, with no short-jump optimization), `call/ret/leave/nop`, and `int 0x80` (JetOS syscall ABI).
* Directives: `.text` / `.data`, `label:`, `.quad` / `.byte` / `.asciz`.

**Verification** (three levels — one step beyond jcc's M30 verification, since QEMU was available this time):

1. **Encoding verification with objdump/readelf (independent tools)** — a calculation/branch test program was disassembled to confirm that the generated instructions exactly matched the intended instructions.
2. **Actual CPU dynamic execution** (mapped into the exact virtual address with `mmap` and executed on a real CPU, not an emulator) — verified that `(5+7)*3-2` correctly produced `rax=34`, and that a `sum(0..10)` loop + function call + memory store/load program correctly produced `rax=55`.
3. **Execution inside actual JetOS running in QEMU** — assembled a hello-world program with jas (`int 0x80` calling `SYS_WRITE`), placed it into jetfs, and executed it with `run /hello.exe` from jash. The screen output `"Hello from jas (JetOS native assembler)!"` was confirmed by screenshot. **While jcc's M30 output could not be boot-tested at the time because QEMU was unavailable (documented as a known limitation), this milestone verified the same execution path end-to-end on actual JetOS under QEMU for the first time.**

**Makefile:** Build with `make jas` (same pattern as `jcc`).

---

## Known Limitations / Incomplete (M42)

| Item                                                              | Status                                                                                           |
| ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| jas: SIB/index addressing and RIP-relative addressing unsupported | Out of scope (see above)                                                                         |
| jas: Direct `[label]` memory references unsupported               | Load the address into a register first with `mov reg,label`                                      |
| jas: No short-jump (2-byte) optimization                          | All branches use rel32 (5–6 bytes) — only increases code size, does not affect correctness       |
| TLS: ECDHE unsupported                                            | `bignum.c` only supports integer modular arithmetic over prime moduli; no elliptic-curve support |
| TLS: No CRL/OCSP revocation checking                              | Carried over since M24                                                                           |
| M38 VirtIO-Input persistent event reception issue                 | Out of scope for this session; carried over unchanged                                            |

---

## Milestone 43 — Native jcc Port: JetOS Runs Its Own Compiler

**The first complete version of a "JetOS can develop JetOS" environment.** `jcc.c` itself was made capable of being built as an ELF64 program that runs directly in Ring3 on JetOS.

* Added `tools/jetos_native_shim.h` — a minimal runtime using only JetOS system calls instead of libc, providing `malloc/calloc/realloc` (a bump allocator on `SYS_BRK`), file I/O (`SYS_OPEN/READ/WRITE_FD/CLOSE`), and `strlen/strcmp/memcpy/memset/strdup/strtol`.
* Wrapped `jcc.c` with `__JETOS_NATIVE__` so both host builds (unchanged) and native builds (`-nostdlib -static -mgeneral-regs-only`, reusing `tools/testdata/elf_link.ld`) are supported.

**Two actual crashes were discovered and fixed during real execution testing:**

1. **General Protection Fault** — gcc -O2 automatically vectorized the malloc/memcpy copy loop using SSE instructions. The kernel did not initialize SSE state for Ring3 processes. Applying `-mgeneral-regs-only` to the native jcc build fixed the issue.
2. **Page Fault (NULL dereference)** — the shim's `realloc()` did not obey the standard C rule `realloc(NULL, size) == malloc(size)`, causing a crash. A NULL check fixed the issue.

**Final real-execution verification (complete loop):** On actual JetOS running in QEMU —

```text
jash /> run /jcc.exe
jcc: /jcc_in.c -> /jcc_out.elf compilation complete
jash /> run /jcc_out.elf
Compiled BY jcc running ON JetOS itself! sum(0..10) check below.
RESULT: 55 (correct)
```

The jcc running on JetOS read C source code, compiled it, and the resulting program ran again on JetOS and produced the correct result.

---

## Milestone 44 — Added argc/argv Passing to elf_loader

Resolved the M43 limitation where `elf_loader.c` did not pass `argv/argc`, forcing native jcc to use fixed paths.

* `elf_load()` now accepts `argv`/`argc`, writes the strings and a NULL-terminated `char* argv[]` pointer array directly into the lowest page of the user stack, and records the user virtual address of that array in `elf_load_result_t.argv_user_ptr`.
* Added `hal_enter_user_mode_args()` (the existing `hal_enter_user_mode` was left untouched, preserving compatibility with existing callers such as `ring3_test.c`). Immediately before entering via `iretq`, it sets `RDI=argc` and `RSI=argv`, allowing the entry point to use the standard SysV ABI `main(int argc, char **argv)` signature.
* `jash`'s `run <path> <args...>` now passes `<args...>` directly as the program's `argv[1..]`. Previously, `run` only processed the first argument and completely ignored the rest.

**Verification:** An actual QEMU test using a minimal native program that prints argc/argv exactly as received:

```text
run /argvtest.elf hello world

argv test: argc=3
  argv[0] = '/argvtest.elf'
  argv[1] = 'hello'
  argv[2] = 'world'
```

The result matched exactly.

---

## Known Limitations (M45 Consolidated)

| Item                                                                                            | Status                                                                                                                                    |
| ----------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Final execution verification of a jas-generated output file                                     | Assembly and argv passing were confirmed, but execution with a filename accidentally containing `@` was not fully verified — next session |
| "Additional items considered important"                                                         | Not started due to time constraints this session — candidates: JETFS journaling, audio output, file permissions/multi-user support        |
| M38 VirtIO-Input persistent event reception issue                                               | Out of scope for this session; carried over unchanged                                                                                     |
| QEMU sendkey `"underscore"` key name is invalid; `"shift-2"` generates `@` instead of digit `2` | Test infrastructure issue, unrelated to JetOS                                                                                             |

---

## Milestone 45 — jas Native Build + argv Support

Updated `jas.c` in the same way as M43's jcc: wrapped it with `__JETOS_NATIVE__` so it can be built for both host and native targets. Both `jcc_native` and `jas_native` were also updated to use the real argv provided by M44.

* Added the remaining minimal libc replacements required by jas.c to `tools/jetos_native_shim.h`: `strchr/strrchr/strncpy/strtoll`, ctype functions (`isspace/isxdigit/isdigit/tolower`), and a custom `stdarg.h`-based variadic formatter for the real multiple-`%s` formatting used by jas's `die()`.
* Declared `_start` in both `jcc.c` and `jas.c` as `void _start(long argc, char **argv)`, receiving `RDI/RSI` directly according to the SysV ABI, allowing M44's real argv to be passed without CRT.
* Both `jcc_native` and `jas_native` now use real command-line arguments when `argc>=2/3`, while safely falling back to the previous fixed paths when no arguments are provided.

**Verification:** After a native build (with no SSE instructions confirmed), actual QEMU execution of:

```text
run /jas.exe /hello.s /hello@.elf

jas: /hello.s -> /hello@.elf assembly complete
```

confirmed that jas received real command-line arguments rather than fixed paths and successfully completed the assembly. **(Follow-up verification: in the next QEMU session, the test was retried with a clean filename, and executing the generated `h2.elf` successfully printed `"Hello from jas (JetOS native assembler)!"`, completing the full round trip.)**

---

## Milestone 46 — Read-Only File Protection Enforced at the Kernel Level

M34 implemented file permissions at the software level using a separate list file (`/system/readonly.txt`) and requiring each caller to check before saving/deleting. This meant the restriction could be bypassed by calling `jetfs_write` directly — a limitation that had been honestly documented. This milestone removes that limitation at the kernel level.

* Added a `flags` field to `jetfs_inode_t` (`JETFS_FLAG_READONLY`). Existing images remain backward-compatible because `mkjetfs` clears the entire inode table to zero when creating an image, resulting in no attributes by default.
* `jetfs_write` / `jetfs_append` / `jetfs_delete` now directly check this flag internally. Even if `perm.c` is bypassed, direct calls to `jetfs_write` are blocked — **true kernel-level enforcement**. `jetfs_rename` is intentionally not blocked, following the convention of Windows' read-only attribute.
* Refactored `perm.c` into a thin layer delegating to `jetfs_set_readonly` / `jetfs_is_readonly`. Callers such as jash's `chmod` / `rm` / `write` were not changed by even a single line.
* The legacy `/system/readonly.txt` list is automatically migrated at boot by `perm_migrate_legacy_list()`, which reads the list once, converts entries to the new inode flags, and then deletes the legacy list file itself.

**Verification:** Full cycle on actual JetOS running in QEMU:

```text
write /t.txt hi          -> File written
chmod ro /t.txt          -> Read-only enabled
write /t.txt bye         -> write: Read-only file (blocked)
chmod rw /t.txt          -> Write access restored
write /t.txt bye         -> File written (success)
```

The behavior matched expectations exactly.

---

## Next Tasks (Priority Order, M46 Updated)

1. **JETFS journaling / crash consistency** — long-standing carry-over; highest priority.
2. **Actual audio output** (AC97/HDA) — carried over since M34.
3. **jas SIB addressing.**
4. **M38 VirtIO-Input persistent event reception issue.**
5. **True multi-user support** (per-owner permissions) — M46 only introduced a single "read-only" flag; owner/group/other permission concepts are still not implemented.
