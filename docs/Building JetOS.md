# Building JetOS

This document explains how to build JetOS yourself and run it in QEMU.

This document is written based on **Milestone 18**.

---

## 1. Required Environment

The current build and test environment for JetOS is as follows.

* Debian / Ubuntu
* `gcc`
* `mingw-w64`
* `make`
* `qemu-system-x86`
* `OVMF`
* `mtools`
* `dosfstools`

JetOS targets a UEFI environment, and during the current development process, booting is tested using QEMU + OVMF.

---

## 2. Installing Required Packages

On Debian or Ubuntu, the required packages can be installed with the following command.

```bash
sudo apt install mingw-w64 qemu-system-x86 ovmf mtools dosfstools make gcc
```

---

## 3. Building JetOS

Run the following command in the root directory of the JetOS repository.

```bash
make
```

This command builds:

* UEFI bootloader
* JetOS kernel

The bootloader is built using a MinGW-family compiler, while the kernel is built using regular GCC.

After the build is complete, the main output files are:

```text
build/BOOTX64.EFI
build/KERNEL.ELF
```

---

## 4. Creating a JETFS Image

Create the JETFS data disk, which is the file system of JetOS.

```bash
make mkjetfs
./build/mkjetfs build/jetfs.img 16
```

Generated file:

```text
build/jetfs.img
```

---

## 5. Creating an ESP Image

Create the ESP image to be used for UEFI booting.

```bash
make esp
```

Generated file:

```text
build/esp.img
```

---

## 6. Running in QEMU

Once all images are ready, JetOS can be run with the following command.

```bash
./scripts/run_qemu.sh
```

This script boots JetOS using QEMU and OVMF.

The current default execution environment also includes QEMU user networking and an RTL8139 device.

---

## 7. Full Build and Run

To run JetOS from the beginning, proceed in the following order.

```bash
make
make mkjetfs
./build/mkjetfs build/jetfs.img 16
make esp
./scripts/run_qemu.sh
```

---

## 8. Building Test Programs

JetOS also includes test programs for testing executable loading and process functionality.

### PE32+ Test

```bash
./scripts/build_test_exe.sh
```

This creates a PE32+ `hello.exe` to test Win32 compatibility features and the PE loader.

### Native ELF Test

```bash
./scripts/build_test_native_elf.sh
```

This tests Native ELF execution and process isolation functionality.

---

## 9. Build Results

After a successful build, the following main files are generated.

```text
build/
├── BOOTX64.EFI
├── KERNEL.ELF
├── esp.img
└── jetfs.img
```

`BOOTX64.EFI` is the UEFI bootloader, and `KERNEL.ELF` is the JetOS kernel.

`esp.img` is the ESP image for UEFI booting, and `jetfs.img` is the JETFS data disk image of JetOS.

---

## 10. If a Build Problem Occurs

First, check that all required packages are installed.

```bash
sudo apt install mingw-w64 qemu-system-x86 ovmf mtools dosfstools make gcc
```

Then build again from the root directory of the repository.

```bash
make
```

If a problem occurs due to existing build results, check the cleanup command provided by the Makefile and then build again.

---

## 11. Build Environment Summary

| Item                | Technology Used        |
| ------------------- | ---------------------- |
| Target Architecture | x86_64                 |
| Bootloader          | UEFI                   |
| Bootloader Compiler | x86_64-w64-mingw32-gcc |
| Kernel Compiler     | GCC                    |
| Firmware            | OVMF                   |
| Test Environment    | QEMU                   |
| File System Image   | JETFS                  |
| EFI Image           | FAT32 ESP              |

---

## 12. One-Line Summary

The basic JetOS build process is as follows.

```bash
make
make mkjetfs
./build/mkjetfs build/jetfs.img 16
make esp
./scripts/run_qemu.sh
```

Through this process, JetOS can be built and booted in a QEMU + OVMF environment.
