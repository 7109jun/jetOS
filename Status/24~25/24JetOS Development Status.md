# JetOS Development Status

> Last updated: Milestone 24 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation and linking verification + **verification of a real three-level certificate chain (root → intermediate → leaf) generated with actual OpenSSL, using production code compiled with host gcc** (same philosophy as the sparse-file harness in M23 — running the exact code that goes into the kernel with a different I/O backend) + actual QEMU boot with serial log verification.
>
> This document is written so that the complete current state can be understood using only this document and the latest tar file (`JetOS_Milestone24.zip`).

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Milestone 24 eliminated one of the biggest known limitations that had remained since M19: **certificate trust-chain verification + hostname verification**.

From M19 through M23, TLS was in a state where "encryption worked, but the server's identity was not verified," leaving it vulnerable to MITM attacks ("HTTPS with encryption only").

This milestone closes that gap by adding direct implementation of RSASSA-PKCS1-v1_5 signature verification (RFC 8017) and a local root CA store.

---

## Build & Run

Same as the M23 documentation. No additional changes.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

### To access real HTTPS sites (New, Required)

Starting with M24, `tls_connect()` (the path used by jash's `https`/`fetchrun` and the browser app) **enforces certificate trust-chain verification by default**.

If the local trust store is empty (the default state), every connection will fail with "no trusted root CA found," regardless of the destination. This is an intentional fail-closed default, considered much safer than silently allowing MITM attacks.

To access real HTTPS sites:

1. Prepare the root CA certificate(s) you want to trust in DER format and concatenate them into a single file:

   ```bash
   openssl x509 -in some-root-ca.pem -outform der -out root1.der
   cat root1.der root2.der > roots.der
   ```

2. Put `roots.der` into the USB image, boot JetOS, and use the existing `usbimport` command to copy it into JETFS at `/system/certs/roots.der`.

3. Reboot. The trust store is read and cached only once, during the first TLS connection after boot, so if the file is changed, a reboot is required for the changes to take effect. This is a known simplification and is listed in the limitations table below.

---

## Verification Methodology (Added in M24)

In addition to the six verification methods used through M23 (host harness tests / QEMU boot + serial logs / QEMU monitor + screendump / host sparse-file harness, etc.):

### 7. Real OpenSSL Certificate Chain, Signature, Chain, and Hostname Verification

A real root CA was generated using `openssl req -x509`. It was then used to sign an intermediate CA, which was used to sign leaf certificates.

The certificates were generated as DER and tested with:

* `www.example.com` + SAN `www.example.com` / `example.com`
* A separate wildcard `*.example.org` case

`kernel/net/crypto/x509_min.c` and `x509_trust.c` were compiled with host gcc, replacing only `jetfs_read` with a host-file-I/O stub while keeping the remaining code exactly the same as the production kernel code.

A total of **22 real-world test cases** were verified:

* Successful parsing of leaf, intermediate, and root certificates
* Correct extraction of CN/SAN
* Hostname matching:

  * Exact match
  * Second SAN entry
  * Wildcard `*.example.org` matching `www.example.org`
  * Wildcard not matching `example.org`
  * Wildcard not matching `a.b.example.org`
  * Mismatch with a completely different domain
* Leaf signature successfully verified using the intermediate CA public key
* Leaf signature verification failing when using the wrong (root) public key
* Intermediate signature successfully verified using the root public key
* Root self-signature verification
* **Modification of one byte in `tbsCertificate` causing signature verification to fail**, proving that the implementation verifies the actual hash rather than merely checking the signature format
* Validity-period verification:

  * `now` inside the validity window
  * Expired
  * Not-yet-valid
* `x509_trust_load` + `x509_verify_chain`:

  * Successful trust-store matching when the server sends only leaf + intermediate
  * Successful trust-store matching when the server also sends the root
  * Expired certificates failing verification
  * **An empty trust store always failing closed**

**Result: 22/22 tests passed.**

### 8. Full Kernel Link Verification

Even in environments without MinGW, the native `gcc` portion can be separately verified by running:

```bash
make build/KERNEL.ELF
```

This verifies the kernel all the way through linking.

The bootloader EFI portion still requires a separate MinGW toolchain. This is a known limitation of this verification method.

---

## Directory Structure (M24)

Only changes compared with M23 are shown.

```text
JetOS/
├── kernel/net/crypto/
│   ├── bignum.h/c
│   │   └── M24: Added bn_byte_len() for calculating the exact byte length
│   │       of a modulus. Shared by the existing TLS logic and X.509
│   │       signature verification.
│   ├── der.h
│   │   └── M24: Added DER_TAG_OCTETSTRING (0x04).
│   ├── x509_min.h/c
│   │   └── M24: Expanded significantly from "RSA public-key extraction"
│   │       to parsing the original tbsCertificate DER, original
│   │       issuer/subject bytes, signature value, CN, SAN (dNSName),
│   │       notBefore, and notAfter.
│   │       Added x509_verify_signature
│   │       (direct RSASSA-PKCS1-v1_5/SHA-256 implementation),
│   │       x509_check_hostname (SAN-first + single wildcard),
│   │       and x509_check_validity.
│   ├── x509_trust.h/c
│   │   └── M24: New. Loads root CAs from
│   │       /system/certs/roots.der in JETFS and performs
│   │       x509_verify_chain (signature chain / issuer-subject
│   │       linkage / validity period / trust anchor verification).
│   │       Fails closed when the trust store is empty.
│
├── kernel/net/
│   ├── tls.h/c
│   │   └── M24: tls_connect() now enforces trust-chain verification
│   │       and hostname verification by default.
│   │       Added tls_connect_ex(..., allow_untrusted) for internal
│   │       self-tests only.
│   │       Added tls_last_error() to expose human-readable failure
│   │       reasons.
│   │       The Certificate message now parses the entire chain
│   │       (up to 6 certificates).
│
├── kernel/kernel_entry.c
│   └── M24: TLS self-tests switched to tls_connect_ex(..., 1).
│       Since the self-tests use self-signed test certificates,
│       trust-chain verification is intentionally bypassed.
│       They continue to verify that encrypted TLS round trips work.
│
├── kernel/apps/jash.c, browser.c
│   └── M24: Removed the previous "[WARNING] connecting without
│       verification" message.
│       On failure, tls_last_error() now exposes specific reasons
│       such as expiration, missing trust store, or hostname mismatch.
```

All other directories remain identical to M23.

---

## Milestone Summary

### Milestone 24 — Certificate Trust Chain + Hostname Verification

| #  | Title                                           | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| -- | ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 24 | Certificate Trust Chain + Hostname Verification | ① Expanded `x509_min` to fully parse `tbsCertificate`, issuer, subject, signature value, validity period, CN, and SAN. ② Directly implemented RSASSA-PKCS1-v1_5 (SHA-256) signature verification according to RFC 8017. ③ Added a JETFS-based local root CA store (`x509_trust.c`) and chain verification including signature chains, issuer-subject linkage, validity periods, and trust anchors; fails closed when the store is empty. ④ Added SAN-first hostname verification with single-wildcard support. ⑤ Changed `tls_connect()` to enforce verification by default, with a separate bypass API only for internal self-tests. ⑥ Improved jash/browser failure messages to display specific failure reasons. |

---

## Complete Currently Working Feature List

### All M1–23 Features

Same as the M23 documentation — everything remains intact.

At the completion of Milestone 24, the following were re-verified:

* Full kernel compilation and linking
* TLS self-tests using self-signed certificates
* No regression detected

### New in Milestone 24

* **Certificate Signature Verification**
  Direct implementation of RSASSA-PKCS1-v1_5 with SHA-256 by calculating `sig^e mod n` and comparing the resulting EM byte-by-byte against:

  `0x00 0x01 FF..FF 0x00 || DigestInfo(SHA-256) || hash(tbsCertificate)`

  No external cryptographic library is used. The existing bignum module from M19 is used.

* **Certificate Chain Verification**
  Parses the complete certificate chain sent by the server (up to 6 certificates), connects issuer and subject fields byte-for-byte, and verifies every signature.

  The final certificate is trusted if its issuer matches one of the roots in the local trust store and its signature verifies against that root.

  Both common cases are supported:

  * The server does not send the root certificate.
  * The server sends the root certificate.

* **Local Root CA Store**
  Instead of embedding a CA bundle into the kernel image, JetOS reads it from `/system/certs/roots.der` in JETFS.

  The user populates it using `usbimport`.

  If the trust store is empty, the default behavior is **"trust nobody" (fail-closed)**.

* **Hostname Verification**
  Uses `subjectAltName` `dNSName` entries first. If SAN is present, CN is not checked, following the recommendation of RFC 6125.

  If SAN is absent, CN is used as a fallback.

  Only a single leftmost-label wildcard such as `*.example.com` is supported. Multi-label and partial-label wildcards are treated as mismatches.

* **`tls_last_error()`**
  TLS failures are no longer reduced to a generic "TLS handshake failed" message.

  Specific reasons such as:

  * Certificate outside validity period
  * Chain linkage failure
  * Signature verification failure
  * Missing trust store
  * Hostname mismatch

  are exposed directly to jash/browser.

---

## Bugs / Issues Found and Fixed in M24

### 19. Missing `bn_byte_len()` Caused a Duplicate "Guess Loop"

This was more of a cleanup issue than a functional bug.

The TLS ClientKeyExchange stage already contained logic for determining the exact modulus byte length. Signature verification required the same calculation.

Instead of maintaining duplicate logic in two locations, `bn_byte_len()` was extracted into `bignum.c` as a shared function.

The existing "guess loop" in `tls.c` was also cleaned up, including the unnecessary unused `modulus_buf` array.

### 20. `serial_write_uint` Was Actually a Static Function Inside `kernel_entry.c`

During development, `tls.c` needed to log the number of loaded trust-store certificates, so `serial_write_uint` was initially used directly.

Compilation produced an `implicit declaration` warning.

Investigation showed that `serial_write_uint` was not a public API declared in `serial.h`; it was a local static function used only inside `kernel_entry.c`.

`serial.h` only exposes `serial_write_hex64`.

The issue was solved by adding a small decimal output helper, `serial_write_small_uint`, directly inside `tls.c`.

Promoting it to a public API was considered unnecessarily broad, so the solution was kept local.

---

## Known Limitations / Incomplete Features (M24)

Most limitations from the M23 documentation remain valid, including the anti-aliasing scope, 20 GB file real-world testing, triple-indirect reduction, and `jetfs_append` performance.

New or updated M24 limitations:

| Item                                                  | Current Status                                                                                                                                                                                                              |
| ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `basicConstraints` / `keyUsage` / `pathLenConstraint` | Not parsed or checked. The implementation does not verify whether an intermediate certificate is actually designated as a CA or whether its path-length constraint is respected. It only verifies the signatures and chain. |
| CRL / OCSP Revocation Checking                        | Not implemented. A leaked/revoked certificate is still trusted if it is within its validity period and its signatures are valid.                                                                                            |
| Name Comparison                                       | Uses exact DER byte comparison instead of RFC 5280 string normalization such as case folding and whitespace normalization.                                                                                                  |
| Trust Store Inclusion                                 | No CA bundle is embedded in the kernel. The user must prepare `/system/certs/roots.der`. If it does not exist, all real HTTPS connections are blocked. See the Build & Run section above.                                   |
| Trust Store Caching                                   | Loaded only once during the first TLS connection after boot. Changes to `roots.der` during the same boot session are not reflected until reboot.                                                                            |
| CBC Padding Oracle Protection                         | Still not implemented (existing limitation since M19 — MAC/padding verification is not constant-time).                                                                                                                      |
| RTC / Time Zone                                       | CMOS RTC is assumed to be UTC without timezone correction (see `rtc.c`). This works with the default QEMU configuration, but on real hardware configured with local time, validity checks may be off by several hours.      |
| Chain Length Limit                                    | Maximum of 6 certificates are processed. Additional certificates are ignored. Chains longer than this are extremely uncommon in practice.                                                                                   |

---

## Next Tasks (Suggested Priority)

Continuing from the M23 documentation:

1. **Add `basicConstraints (CA:TRUE)` / `keyUsage` checks** — the biggest remaining gap in certificate chain verification.
2. **Apply anti-aliasing to the remaining icons** — taskbar star/cloud/speaker/signal-bar icons. This was already a priority in M23 but was delayed while M24 security work was completed.
3. **FAT32 write support.**
4. **ECDHE support.**
5. **AC97/HDA digital audio.**
6. **Resource cleanup when processes terminate.**
7. **Apply the reusable widget toolkit throughout the system.**
8. **Real-hardware verification of the 20 GB scenario** — carried over from M23.
9. **Improve trust-store usability** — for example, display trust-store load status/root count in the Settings app and add a command to reload `roots.der` without rebooting.

---

## Code Style / Design Principles

The principles established through M23 remain fully valid:

* Three assembly files
* Boot-time self-tests
* Host harness tests
* Full rebuild verification after header changes
* Screenshot verification for visual changes
* Avoid large static buffers — use `kmalloc`
* Be careful about use-after-free
* Document simplifications
* When referencing other projects, use concepts only
* Check all call sites when widening API parameter types or counts
* Use host sparse-file harnesses for large-scale scenarios

### Additional M24 Principles

* **Cryptographically sensitive code must be tested against real data generated by third-party tools such as OpenSSL.**

  Testing only with self-created test vectors can allow self-consistent bugs where a parser merely understands the output of its own encoder.

  For M24, real three-level certificate chains were generated with OpenSSL, including normal, wildcard, modified-signature, and expired-certificate cases.

* **Security defaults should fail closed rather than fail open.**

  When the trust store is empty, JetOS does not "allow the connection anyway and display a warning."

  Instead, the connection is blocked entirely.

  This intentionally reverses the behavior used by earlier milestones (M19–M23), where connections were allowed with only a warning.

  The decision is explicitly documented because it is a significant security-policy change.
