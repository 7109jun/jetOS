# JetOS Development Status

> Last updated: Milestone 25 completion
> Build environment: x86_64-w64-mingw32-gcc (bootloader) + gcc (kernel) + QEMU+OVMF (testing)
> Verification method: Full kernel compilation and linking verification + OpenSSL-based real-world certificate-chain host harness (expanded from 22 cases in M24 to 31 cases in M25, including an actual attempt to forge a signature using a certificate that is not a CA and verification that the attack is blocked) + actual QEMU boot with serial log verification.
>
> This document is written so that the complete current state can be understood using only this document and the latest tar file (`JetOS_Milestone25.tar`).

---

## Philosophy

> **An OS for people who want to use Windows but cannot afford it.**

Until M24, certificate verification effectively meant that a certificate was accepted if "the signature was mathematically valid."

That meant that if an attacker obtained the private key of a leaf certificate (which is not a real CA), they could use it to "sign" arbitrary certificates and insert them into the chain — effectively forging signing authority without being delegated CA privileges.

This milestone closes that gap.

---

## Build & Run

Same as the M24 documentation. No additional changes. See the M24 documentation for details such as preparing the trust store.

```bash
make
make mkjetfs && ./build/mkjetfs build/jetfs.img 128
dd if=/dev/zero of=build/usb.img bs=1M count=128 && mkfs.vfat -F 32 build/usb.img
make esp
./scripts/run_qemu.sh
```

---

## Verification Methodology (M25 Update)

In addition to the seven verification methods used through M24, the OpenSSL-based real-world harness introduced in M24 was expanded.

### 9. Reproducing and Blocking a "Signature Forgery Using a Non-CA Certificate" Attack

A real attack scenario was reproduced by signing a new certificate (`rogue.crt`) using `leaf.key`, the private key of a real leaf certificate that is **not a CA**.

`openssl x509 -req -CA` does not enforce `basicConstraints`, so the certificate can still be created successfully. RSA signature mathematics only proves **who signed the data**; it does not inherently prove that the signer was authorized to act as a CA.

As expected:

```text
x509_verify_signature(&rogue, &leaf.pubkey_n, &leaf.pubkey_e)
```

succeeded because the signature itself was mathematically valid and was genuinely generated using the leaf's private key.

The important test was then:

```text
x509_verify_chain([rogue, leaf], ...)
```

A pre-M25 implementation would have accepted this chain.

M25 must now reject it with the precise reason:

```text
issuer is not a CA certificate
```

The test also verifies that:

* The root certificate parses with `basicConstraints cA:TRUE`.
* The intermediate CA parses with `basicConstraints cA:TRUE` and `pathLenConstraint:0`.
* The leaf certificate has no `basicConstraints` extension.
* A certificate that is not a CA cannot be used as a chain signer.

A total of **31 test cases** were executed:

* 22 cases from M24
* 9 additional M25 cases

**All 31/31 tests passed.**

Existing valid certificate chains were also re-tested and continued to pass, confirming that there was no regression.

---

## Directory Structure (M25)

Only changes compared with M24 are shown.

```text
JetOS/
├── kernel/net/crypto/
│   ├── x509_min.h/c
│   │   └── M25: Added parsing for basicConstraints
│   │       (cA BOOLEAN, pathLenConstraint INTEGER)
│   │       and keyUsage (keyCertSign bit only).
│   │       Added the following fields to x509_cert_t:
│   │       has_basic_constraints
│   │       is_ca
│   │       has_path_len
│   │       path_len
│   │       has_key_usage
│   │       key_cert_sign
│   │
│   ├── x509_trust.h/c
│   │   └── M25: The x509_verify_chain chain-linking loop now
│   │       verifies that the signer (parent) is actually a CA
│   │       with basicConstraints cA:TRUE.
│   │       If keyUsage is present, keyCertSign must also be set.
│   │       pathLenConstraint violations are also checked.
│   │       Each violation is rejected with a specific reason string.
│   │
│   │       The root certificates in the trust store are still
│   │       exempt from these checks. The explicit presence of a
│   │       certificate in the local trust store itself is considered
│   │       the trust basis. This is a documented design decision.
```

All other directories remain identical to M24.

No new source files were added; the milestone was completed by extending existing files.

---

## Milestone Summary

### Milestone 25 — Strengthened basicConstraints / keyUsage Chain Verification

| #  | Title                                                       | Core Changes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| -- | ----------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 25 | Strengthened basicConstraints / keyUsage Chain Verification | ① Added parsing of `basicConstraints` (`cA` / `pathLenConstraint`) and `keyUsage` (`keyCertSign`) to `x509_min`. ② Strengthened `x509_verify_chain` so that it verifies not only signature mathematics but also whether the signer is actually authorized as a CA. Missing CA authorization now causes rejection. ③ Added `pathLenConstraint` enforcement so chains cannot exceed the permitted CA depth. ④ Reproduced an actual signature-forgery scenario using a non-CA certificate with OpenSSL and verified that JetOS blocks it. |

---

## Complete Currently Working Feature List

### All M1–24 Features

Same as the M24 documentation — all features remain intact.

At the completion of Milestone 25, the following were re-verified:

* Full kernel compilation and linking
* Existing valid certificate chains
* Leaf + intermediate chains
* Chains where the server sends the root
* Chains where the server does not send the root
* No regression detected

### New in Milestone 25

* **`basicConstraints` Verification**

  Every signer in the certificate chain must be explicitly designated as a CA with `cA:TRUE`.

  If the extension is completely absent, the certificate is treated as **not authorized to act as a CA** and chain verification fails.

  This prevents an attacker from stealing the private key of a normal server certificate and using it to "sign" another certificate and insert it into a trusted chain.

* **`keyUsage (keyCertSign)` Verification**

  If a `keyUsage` extension is present, the `keyCertSign` bit must be set for the certificate to sign certificates.

  If the `keyUsage` extension is absent, the certificate is accepted in this respect, because RFC 5280 does not make `keyUsage` mandatory for every CA certificate.

* **`pathLenConstraint` Verification**

  An intermediate CA can specify how many additional CA certificates may appear below it.

  JetOS now enforces this constraint.

  For example, if an intermediate CA has:

  ```text
  pathlen:0
  ```

  inserting another intermediate CA beneath it causes chain verification to fail.

---

## Bugs / Issues Found and Fixed in M25

There were no separate implementation bugs discovered during this milestone.

M25 primarily closed the security gap already identified in M24.

Instead, the following important security property was confirmed during testing.

### 21. "The Signature Is Valid" and "The Signer Is Authorized to Sign" Are Different Questions

The `rogue.crt` test demonstrated this distinction directly.

`rogue.crt` was signed by the leaf certificate's private key even though the leaf certificate was not a CA.

Therefore:

```text
x509_verify_signature
```

correctly succeeded because the RSA signature mathematics was valid.

However:

```text
x509_verify_chain
```

must reject the chain because the signer does not have CA authorization.

This was confirmed in the actual test.

The test therefore establishes the following security rule:

> **Signature verification success does not imply trust. Both signature verification and signer authorization must succeed before a certificate chain can be trusted.**

This test is intentionally retained as a regression test because without it, a future refactoring could accidentally bypass the CA-authorization check without immediately being noticed.

---

## Known Limitations / Incomplete Features (M25)

Most limitations from the M24 documentation remain valid:

* CRL/OCSP revocation checking is not implemented.
* Name comparison uses exact DER byte matching.
* The trust store is not embedded in the kernel.
* Trust-store changes require a reboot because the store is cached after the first TLS connection.
* CBC padding-oracle protection is still missing.
* RTC timezone correction is not implemented.
* Certificate-chain processing is limited to 6 certificates.

The following M24 limitation has now been removed:

* `basicConstraints`
* `keyUsage`
* `pathLenConstraint`

New or updated M25 limitations:

| Item                            | Current Status                                                                                                                                                                                                                                                                                                                                                                                                           |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `extendedKeyUsage`              | Still not checked. For example, JetOS cannot currently reject a certificate that is intended only for code signing (`id-kp-codeSigning`) rather than TLS server authentication (`id-kp-serverAuth`).                                                                                                                                                                                                                     |
| Remaining `keyUsage` bits       | Only `keyCertSign` is checked. Other bits such as `digitalSignature` and `keyEncipherment` are not interpreted. Therefore, JetOS does not currently verify whether a leaf certificate is explicitly designated for TLS server authentication. A leaf certificate with only `keyCertSign` and no `digitalSignature` could theoretically still pass. This is extremely uncommon in practice but remains a theoretical gap. |
| `pathLenConstraint` calculation | The RFC 5280 exception for self-issued certificates is not implemented. JetOS simply counts them. This is a conservative and stricter simplification that could matter in rare CA operational scenarios such as key rollover.                                                                                                                                                                                            |
| Root `basicConstraints`         | Root certificates in the local trust store are exempt from the CA checks described above. Therefore, if a user accidentally places a certificate without `CA:TRUE` into `roots.der`, it can still operate as a trust anchor. This is an intentional design decision described above.                                                                                                                                     |

---

## Next Tasks (Suggested Priority)

Continuing from the M24 documentation:

1. **Apply anti-aliasing to the remaining icons** — taskbar star/cloud/speaker/signal-bar icons. This has been postponed since M23; now that the security work is complete, it should finally be addressed.
2. **FAT32 write support.**
3. **ECDHE support.**
4. **AC97/HDA digital audio.**
5. **Resource cleanup when processes terminate.**
6. **Apply the reusable widget toolkit throughout the system.**
7. **Real-hardware verification of the 20 GB scenario** — carried over from M23.
8. **Improve trust-store usability** — carried over from M24: display load status in the Settings app and add a reload command without requiring a reboot.
9. **`extendedKeyUsage (id-kp-serverAuth)` verification** — the most practically relevant remaining certificate-validation gap.

---

## Code Style / Design Principles

All principles established through M24 remain valid:

* Three assembly files
* Boot-time self-tests
* Host harness tests
* Full rebuild verification after header changes
* Screenshot verification for visual changes
* Avoid large static buffers
* Be careful about use-after-free
* Document simplifications
* When referencing other projects, use concepts only
* Check all call sites when widening API parameter types or counts
* Use host sparse-file harnesses for large-scale scenarios
* Cryptographically sensitive code must be tested using real data generated by third-party tools
* Security defaults should fail closed rather than fail open

### Additional M25 Principle

* **Break "verification success" into multiple independent AND conditions and test each condition with concrete evidence.**

  Certificate-chain trust is not determined by a single property. Multiple conditions must all be satisfied:

  ```text
  valid signature
      AND
  authorized CA signer
      AND
  valid certificate period
      AND
  valid hostname
      AND
  trusted chain
  ```

  Testing only a normal valid certificate proves very little. If one condition is accidentally removed, the normal case may continue to pass.

  A much stronger test is to deliberately construct a malicious input that violates exactly one condition and verify that the implementation rejects it **for that specific reason**.

  The M25 non-CA signer attack provides exactly this type of evidence: the RSA signature itself is valid, but the chain is rejected because the signer is not authorized to act as a CA.

  This makes future refactoring safer because accidentally bypassing the CA-authorization check is likely to be caught by the regression harness.
