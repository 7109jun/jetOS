# JetOS Development Status

> Last updated: As of the completion of Milestone 19
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation + linking verified, plus **actual QEMU boot with serial logs/screenshots checked**
> (QEMU was installed in the sandbox starting from Milestone 10, making real-boot verification possible. Since then, every milestone has been verified through actual booting.) Milestone 19's cryptographic code (TLS stack) additionally underwent **host harness testing** (compiled with normal Linux gcc and compared against official NIST/RFC test vectors and actual OpenSSL-generated certificates) — this was done to separately verify protocol/kernel integration issues from the correctness of the cryptographic algorithms themselves.
>
> This document is written so that the entire current state can be understood using only this document + the latest tar file (`JetOS_Milestone19.tar`).

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**
> It is not exactly like Windows, but an independent, free OS that feels familiar from the moment it boots.
> No dependencies on code, DLLs, or SDKs from external operating systems (Windows/Linux). Everything is implemented directly by JetOS.
>
> Starting from Milestone 15, JetOS has aimed to become a **real OS**, rather than an OS that merely looks polished. Its implementation references core Linux design concepts (per-process address spaces, ELF execution, fault isolation) (open-source references — no licensing issues; only concepts/structures were referenced, not code copied directly).
>
> Milestone 19 tackles four high-priority items that were explicitly listed as "known limitations/incomplete" in the M18 documentation: keyboard focus routing, scrolling/widget toolkit, user-pointer validation, and HTTPS/TLS. In particular, TLS follows the same "no external SDK dependency" philosophy even in the cryptographic stack: SHA-256/HMAC/AES-128/RSA (bignum modexp)/minimal X.509 parser/TLS 1.2 state machine were all implemented directly without using standard cryptographic libraries such as OpenSSL.

---

## Build & Run

```bash
# Install environment (Debian/Ubuntu)
sudo apt install mingw-w64 qemu-system-x86 ovmf mtools dosfstools make gcc

# Build
make                                    # Bootloader (mingw) + kernel (native gcc)
make mkjetfs && ./build/mkjetfs build/jetfs.img 16   # JETFS data disk
make esp                                # Create ESP (FAT32) image
./scripts/run_qemu.sh                   # Boot with QEMU+OVMF (includes -netdev user -device rtl8139 by default)

# Prepare test executables (optional)
./scripts/build_test_exe.sh             # PE32+ hello.exe (Win32 MessageBox test, Milestone 9)
./scripts/build_test_native_elf.sh      # Three native ELF files (normal/crash/user-pointer validation tests, M15/16/19)
```

Output files: `build/BOOTX64.EFI`, `build/KERNEL.ELF`, `build/esp.img`, `build/jetfs.img`

### Reproducing the TLS Self-Test in QEMU

The TLS self-test in `kernel_entry.c` requires an actual TLS 1.2 server to be running at the QEMU SLIRP gateway (`10.0.2.2:8443`). Start a self-signed certificate + Python TLS server on the host as follows, then boot QEMU. If neither is running, only this self-test is skipped with `"TIMEOUT"` and the rest of the boot proceeds normally:

```bash
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes -subj "/CN=test.jetos.local"

python3 - <<'EOF'
import http.server, ssl

class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"Hello from JetOS TLS test server via real TLS 1.2!\n"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
ctx.set_ciphers("AES128-SHA256")
ctx.load_cert_chain("cert.pem", "key.pem")

srv = http.server.HTTPServer(("0.0.0.0", 8443), H)
srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
srv.serve_forever()
EOF
```

### Verification Methodology

1. **Host harness testing**: Logic with minimal hardware dependencies, such as `kernel/fs/jetfs.c`, `kernel/exec/pe_loader.c`, and `kernel/exec/elf_loader.c`, is compiled directly with normal Linux gcc by replacing only the lowest-level I/O functions such as `ahci_read/write_sectors` or `jetfs_read` with stubs, then verified against actual files/network captures. In Milestone 19, the same methodology was applied to the cryptographic primitives (`kernel/net/crypto/`), comparing SHA-256/HMAC-SHA256/AES-128 against official NIST/RFC test vectors, and RSA (bignum modexp), the X.509 parser, and PKCS#1 padding against certificates/private keys generated directly with OpenSSL. This caught one AES key-schedule endianness bug on the host before it was integrated into the actual kernel.

2. **Actual QEMU boot + serial logs**: Boot logs are captured using `-serial stdio` or `-serial file:...`, and `STAGE:` / `=== ... SELF-TEST ===` markers are used to verify the success of each stage. The kernel contains boot-time self-tests for each subsystem (NET/TCP/DNS/RING3/user-pointer validation/TLS self-test), providing automatic regression testing whenever the image is rebuilt.

3. **QEMU monitor + screendump**: The GUI is launched using `-monitor unix:...,server,nowait -display none`, then the `screendump` command is sent through the monitor socket to capture a PPM image → converted to PNG → visually inspected. **Important**: In this sandbox environment, QEMU background processes die when the tool invocation ends, so "boot → interaction → capture → shutdown" must **all be performed within a single bash invocation**. The same applies to the TLS self-test — the host TLS server and QEMU must be started within the same invocation so the server remains alive. Also, the delta-to-pixel movement ratio of `mouse_move` is not consistent, making precise coordinate-based automated clicking unreliable — it should primarily be used for static screen capture verification, such as the desktop immediately after boot.

---

## Directory Structure

```text
JetOS/
├── kernel/
│   ├── hal/
│   │   ├── x86_asm_shim.h    Milestone 19: Added RDRAND/CPUID/RDTSC single-instruction helpers
│   │   │                        (3-assembly-file rule maintained — no new assembly file)
│   │   ├── syscall.c/h       Milestone 19: Added user-pointer validation
│   │   │                        (syscall_check_user_range/_cstr) — pointers outside the valid range
│   │   │                        terminate only that thread (same principle as M16)
│   ├── drivers/
│   │   ├── keyboard.c/h      Milestone 19: Added PgUp/PgDn scancodes (for scrolling)
│   ├── gui/
│   │   ├── wm.c/h             Milestone 19: Exposed keyboard focus routing APIs
│   │   │                        (wm_dispatch_key, etc. — previously internal and used only for
│   │   │                        z-order tracking), content_h/scroll_y + scrollbar rendering/dragging,
│   │   │                        PgUp/PgDn handling
│   │   ├── widget.c/h         Milestone 19: New reusable widget toolkit
│   │   │                        (button/textbox/scrollable text renderer)
│   │   ├── desktop.c/h        Milestone 19: Removed g_jash_running special case,
│   │   │                        replaced with general wm_dispatch_key() routing, wired scrollbar dragging
│   ├── net/
│   │   ├── tcp.c/h            Milestone 19: Added tcp_recv_exact() (for TLS record-sized receives)
│   │   ├── tls.c/h            Milestone 19: New minimal TLS 1.2 client
│   │   │                        (TLS_RSA_WITH_AES_128_CBC_SHA256 only)
│   │   └── crypto/            Milestone 19: New cryptographic primitives
│   │                            (all verified with host test vectors)
│   │       ├── sha256.c/h, hmac_sha256.c/h, aes128.c/h
│   │       ├── bignum.c/h     Minimal arbitrary-precision integer implementation for RSA
│   │       │                        public-key operations (modexp)
│   │       ├── der.c/h, x509_min.c/h   Minimal DER parser extracting only the RSA public key from X.509
│   │       ├── rsa_pkcs1.c/h  RSAES-PKCS1-v1_5 public-key encryption
│   │       ├── tls_prf.c/h    TLS 1.2 PRF (HMAC-SHA256 based)
│   │       └── rng.c/h        RDRAND preferred; RTC/PIT/TSC+SHA-256 fallback if unavailable
│   │                            (weak, documented)
│   ├── apps/
│   │   ├── jash.c/h           Milestone 19: Added `https` command, replaced with scrollable output window
│   │   └── browser.c/h        Milestone 19: Actual typeable address bar (direct GUI input —
│   │                             resolves the M18 limitation where URLs could only be specified
│   │                             through jash), https:// URL support, scrollable page body
├── tools/testdata/
│   └── test_elf_baduptr.c    Milestone 19: New crash program for user-pointer validation self-test
```

The rest of the M18 directory structure is unchanged. See the previous document for details.

---

## Milestone Summary

| #  | Title / Core Content                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| -- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 19 | **Focus routing + scrolling/widgets + user-pointer validation + HTTPS** — ① Generalized keyboard focus tracking automatically based on click/creation order, removing hardcoded special cases. ② Added a reusable widget toolkit + scrollbars, allowing the jash output window and browser body to scroll back. ③ System-call argument pointers are checked to ensure they are inside user memory; if outside, only that process is terminated. ④ Fully implemented SHA-256/HMAC/AES-128/bignum-RSA/minimal X.509 parser directly to complete a TLS 1.2 (RSA-AES128-CBC-SHA256) client, with an actual local TLS server and QEMU verifying an end-to-end handshake + encrypted HTTP round trip. |

---

## Complete List of Currently Working Features

### Hardware / Boot / Kernel Core / Filesystem / Plaintext Networking / GUI / Applications / API Layer

Same as the M18 document — all features remain intact with no regressions. The entire system was re-verified at the completion of Milestone 19.

### New in Milestone 19

* **Generalized keyboard focus**: A clicked or newly opened window automatically receives keyboard input. Multiple text-input windows (jash, Notepad, browser address bar) can be switched between naturally.
* **Scrolling + widget toolkit**: Scrollbars are now available in the jash output window and browser body, using PgUp/PgDn or dragging, allowing previous output and long pages to be viewed again. `kernel/gui/widget.c` separates buttons, textboxes, and scrollable text renderers into reusable components. Previously, each application rendered them directly.
* **User-pointer validation**: If a user process passes a pointer outside its valid memory region to a system call, the kernel does not dereference it and immediately terminates only that thread. `/test_elf_baduptr` performs this self-test on every boot.
* **HTTPS (TLS 1.2)**: The `jash https <host> <port> </path> [save-to]` command and the browser address bar can directly accept `https://` URLs and perform actual encrypted HTTPS GET requests. Only the `TLS_RSA_WITH_AES_128_CBC_SHA256` cipher suite is supported, using RSA key exchange + AES-128-CBC + HMAC-SHA256.

---

## Bugs Actually Found and Fixed

7. **AES-128 key-schedule endianness bug** (M19): Key bytes were copied directly from a `uint8_t*` into a `uint32_t` array and then reinterpreted as words. Because x86 is little-endian, this did not match the big-endian word construction required by FIPS-197. It was discovered through host harness testing against the official FIPS-197 vector. The characteristic symptom was that encryption/decryption roundtrips worked, but the result differed from the official vector. Both directions were internally consistent but produced incorrect AES. The bug was fixed by explicitly assembling key words in big-endian order using shifts during key expansion.

8. **Missing `signature_algorithms` extension in TLS ClientHello** (M19): Although this extension is theoretically optional for the static RSA key-exchange suite because ServerKeyExchange signatures are not required, OpenSSL 3.x servers reject the handshake with a `NO_SUITABLE_SIGNATURE_ALGORITHM` alert when it is absent. The issue was reproduced consistently in the QEMU boot self-test as "Alert received → handshake failure." The exact cause was confirmed by replaying the raw ClientHello bytes from Python on the host. It was fixed by adding a `signature_algorithms` extension advertising only `rsa_pkcs1_sha256`.

9. **SNI rejected when using an IP literal** (M19): During development, the self-test placed an IP address such as `"10.0.2.2"` directly into the SNI hostname field, causing the server to reject it. RFC 6066 specifies that SNI is for hostnames, so IP literals are inappropriate. jash, the browser, and the self-tests were modified so that if the target appears to consist of four dot-separated numeric components, the SNI extension is omitted entirely.

---

## Known Limitations / Incomplete Features

### Core Incomplete Features

| Item                       | Status                                                                                                       |
| -------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Process user-space size    | Fixed at 2 MiB per process; larger programs cannot run                                                       |
| Dynamic linking            | None — only statically linked ELF/PE files                                                                   |
| DNS functionality          | A records only; no CNAME chain resolution or caching                                                         |
| Concurrent Ring3 processes | CR3 switching works, but stress testing with many concurrent processes has not yet been performed            |
| fork/exec model            | None — `elf_execute()` only supports attaching a new address space to a new thread and executing immediately |

### TLS/HTTPS Limitations

| Item                                 | Status                                                                                                                                                                              |
| ------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Certificate trust-chain verification | None — CA signatures/chains are not verified. Self-signed certificates are accepted.                                                                                                |
| Hostname verification                | None — the certificate's CN/SAN is not checked against the target host.                                                                                                             |
| MITM protection                      | Essentially none — the connection is encrypted, but the peer's identity is not verified.                                                                                            |
| Supported cipher suites              | Only `TLS_RSA_WITH_AES_128_CBC_SHA256`. ECDHE suites are not supported. Connections to many modern servers that disable RSA key exchange or require forward secrecy may fail.       |
| TLS version                          | TLS 1.2 only. TLS 1.3/1.1/1.0/SSL are unsupported.                                                                                                                                  |
| Padding-oracle protection            | None — CBC padding/MAC verification is not constant-time, including non-constant-time `memcmp`.                                                                                     |
| RNG quality                          | RDRAND is used when available. Otherwise, RTC/PIT/TSC+SHA-256 is used as a fallback. This fallback is cryptographically weak and should not be used for security-critical purposes. |
| Certificate chain                    | Only the leaf certificate is parsed. Real sites requiring an intermediate CA chain may fail.                                                                                        |

### GUI Incomplete Features

| Item                    | Status                                                                                                                                                                            |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Copy/paste              | None                                                                                                                                                                              |
| Reusable widget toolkit | Buttons/textboxes/scrollable text were introduced in Milestone 19, but existing applications such as Settings color swatches have not yet been refactored to use the new widgets. |
| Confirmation dialogs    | No confirmation dialog when closing Notepad, such as "Close without saving?" Only `*` is shown in the title.                                                                      |
| True modal MessageBox   | No focus lock — other windows can still be manipulated while it is open.                                                                                                          |
| Mouse-wheel scrolling   | Only 3-byte PS/2 packets are supported, with no wheel. Scrolling is available only through PgUp/PgDn or scrollbar dragging.                                                       |

### Other

| Item               | Status                                                 |
| ------------------ | ------------------------------------------------------ |
| Audio              | No driver                                              |
| Win32 API coverage | Partial; many APIs are unimplemented and become no-ops |
| PE loader          | EXEs dependent on the CRT cannot run                   |

---

## Next Tasks

1. **Certificate verification (trust chain + hostname)** — The biggest security gap in the current TLS implementation. First decide how the CA root store should be provided: a small set of hardcoded root CAs, user-added roots, or another method.
2. **ECDHE support (forward secrecy)** — Requires elliptic-curve scalar multiplication such as P-256. With only RSA key exchange currently supported, many modern servers that disable it cannot be accessed.
3. **TLS 1.3** — Eventually necessary as more modern servers begin completely disabling TLS 1.2.
4. **Apply the reusable widget toolkit throughout the system** — Refactor existing applications such as Settings and File Manager to use `kernel/gui/widget.c`.
5. **Make the user-space region dynamically expandable (`brk`/`mmap`-like)** — Currently fixed at 2 MiB.
6. **fork/exec-style process model** — Required for pipelines, background execution, and similar functionality.
7. **Intermediate CA certificate-chain parsing** — Sites requiring a certificate chain currently fail because only the leaf certificate is examined.
8. **Mouse-wheel support** — Add the PS/2 IntelliMouse extension with 4-byte packets.

---

## Reference: Code Style / Design Principles

* **Assembly is limited to three files**: `kernel/hal/x86_asm_shim.h` (port I/O, segments, Ring3 entry, RDRAND/CPUID/RDTSC, etc. at the single-instruction level), `kernel/hal/context_switch.c` (thread switching), and `kernel/hal/syscall_entry.c` (system-call trampoline). When RDRAND/CPUID/RDTSC were added in Milestone 19, no new assembly file was created; they were added to `x86_asm_shim.h` in accordance with this rule. Everything else is 100% C. C++/Rust are allowed if necessary and approved, but neither is currently used.
* **Every new subsystem must add a "boot-time self-test" to `kernel_entry.c`**, so regression status can be immediately verified through serial logs every time the image is rebuilt. Examples include NET/TCP/DNS/RING3/user-pointer validation/TLS self-tests.
* **Cryptography/protocol code must undergo host harness testing first**: It is integrated into the kernel only after comparison against standard vectors (NIST/RFC) or real data generated with OpenSSL. Thanks to this ordering, the AES endianness bug in Milestone 19 was caught much faster without QEMU debugging.
* **Watch for use-after-free patterns**: In file parsers such as PE/ELF, always re-check that no pointer referencing the contents of a buffer is used immediately after `kfree(buf)`. This type of mistake has already occurred twice.
* **Known simplifications must be explicitly documented in code comments**, including why the simplification was made and what must be done next to make it complete. Security-critical simplifications such as TLS's lack of certificate verification must be especially prominent at the top of the relevant header files.
