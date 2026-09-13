# JETFS API and Native Syscall ABI Reference

Headers: `kernel/fs/jetfs.h`, `kernel/hal/syscall.h`, `kernel/exec/elf_loader.h`

## JETFS — JetOS Native Filesystem

### On-Disk Layout

```text
Block 0        : Superblock
Blocks 1..17   : Metadata journal (write-ahead log)
Blocks 18..N   : Inode table (fixed array, maximum 256 inodes)
Blocks N+1..M  : Data block bitmap
Blocks M+1..end: Data blocks
```

The block size is **4096 bytes** (8 AHCI sectors).

Files support 12 direct blocks plus single-, double-, and triple-indirect blocks, allowing a theoretical maximum file size of approximately **4 TB**. In practice, the maximum size is limited by the capacity of the disk image itself.

### Kernel C API

```c
int jetfs_mount(int drive_index, uint64_t lba_offset);

int jetfs_create(const char *path, jetfs_inode_type_t type);
/* JETFS_TYPE_FILE | JETFS_TYPE_DIR */

int jetfs_read(const char *path, void *buffer,
               uint64_t max_size, uint64_t *out_size);

int jetfs_write(const char *path, const void *data, uint64_t size);

int jetfs_append(const char *path, const void *data, uint64_t len);

int jetfs_delete(const char *path);

int jetfs_rename(const char *old_path, const char *new_path);

int jetfs_stat(const char *path,
               jetfs_inode_type_t *out_type,
               uint64_t *out_size);

typedef void (*jetfs_list_cb)(
    const char *name,
    jetfs_inode_type_t type,
    uint64_t size,
    void *ctx
);

int jetfs_list(const char *dir_path, jetfs_list_cb cb, void *ctx);
```

> **Important:** Read/write operations are **not streaming APIs**. `jetfs_read` and `jetfs_write` always operate on the entire file in a single operation.
>
> To build a large file incrementally, call `jetfs_append` repeatedly.

### Read-Only Attribute

```c
int jetfs_set_readonly(const char *path, int readonly);
int jetfs_is_readonly(const char *path);  /* Returns 0 if the path does not exist */
```

When the read-only attribute is enabled, all of the following operations fail:

* `jetfs_write`
* `jetfs_append`
* `jetfs_delete`

`jetfs_rename` is an exception, following the same convention as the Windows read-only attribute.

### Multi-User Ownership

```c
int jetfs_get_owner(const char *path);  /* Returns -1 if the path does not exist */

int jetfs_chown(const char *path, uint32_t new_uid);  /* Root only */
```

Each file records the UID of the caller that created it as its owner.

**Writing, appending, deleting, and renaming are permitted only for the file owner or root (`uid 0`).**

Reading is always allowed regardless of ownership.

JETFS does not implement traditional Unix-style `rwx` permission bits. Instead, it uses a deliberately simple permission model:

```text
Owner + root → write
Everyone     → read
```

### Metadata Journaling and Crash Safety

Updates involving multiple blocks, such as file creation, deletion, and renaming, are grouped into transactions.

The transaction is first written to the journal and then committed to its actual on-disk locations.

If a power failure or system crash occurs during the operation, the journal is replayed during the next mount to restore filesystem consistency.

**File data itself is not journaled. Only metadata is journaled.**

This follows a design similar to the default journaling mode commonly associated with ext3/ext4.

---

# Native Syscall ABI

## ELF64 / Ring3 Programs

JetOS-native ELF64 executables, such as programs produced by `jcc` or `jas`, cannot link against JetAPI because they run as fully isolated Ring3 processes.

Instead, they directly invoke the raw JetOS system-call interface.

### Calling Convention

Programs enter the kernel using a single:

```asm
int $0x80
```

instruction.

Register usage:

| Register | Purpose            |
| -------- | ------------------ |
| `RAX`    | System call number |
| `RDI`    | Argument 0         |
| `RSI`    | Argument 1         |
| `RDX`    | Argument 2         |
| `RAX`    | Return value       |

The kernel automatically validates every user-space pointer argument to ensure that it falls within the calling process's user address space.

If a pointer lies outside the valid user-space range, **only the offending thread is immediately terminated**.

The kernel as a whole and other processes are unaffected.

---

## System Call Numbers

| Number | Name           | Arguments                                            | Description                                                                                                                    |
| -----: | -------------- | ---------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
|    `0` | `SYS_WRITE`    | `rdi = string pointer`                               | Writes a NUL-terminated string to the serial/debug output. This is not real stdout.                                            |
|    `1` | `SYS_EXIT`     | —                                                    | Terminates the current thread. Does not return.                                                                                |
|    `2` | `SYS_GETTICK`  | —                                                    | Returns the timer tick counter in `RAX`.                                                                                       |
|    `3` | `SYS_BRK`      | `rdi = new brk` (`0` = query only)                   | Expands the process heap. On failure, returns the previous `brk`; there is no separate error code. Shrinking is not supported. |
|    `4` | `SYS_OPEN`     | `rdi = path`, `rsi = mode` (`0` = read, `1` = write) | Returns an fd (`>= 0`) on success, or `-1` on failure.                                                                         |
|    `5` | `SYS_READ`     | `rdi = fd`, `rsi = buffer`, `rdx = maximum bytes`    | Returns the number of bytes actually read (`0` = EOF), or `-1` on error.                                                       |
|    `6` | `SYS_WRITE_FD` | `rdi = fd`, `rsi = buffer`, `rdx = byte count`       | Appends data to the end of the file. Returns the number of bytes actually written, or `-1` on error.                           |
|    `7` | `SYS_CLOSE`    | `rdi = fd`                                           | Returns `0` on success or `-1` on failure.                                                                                     |

---

## Honest File I/O Limitations

The syscall file-I/O model is intentionally simplified to match the capabilities of JETFS. It is **not POSIX-compatible**.

* Writes always append to the end of the file.
* Arbitrary-position overwrites are not supported.
* Reads are sequential from the current per-fd position.
* Seeking or rewinding is not supported.
* Each process can have a maximum of **8 simultaneously open file descriptors**.

---

## Using the Syscalls from C

Because there is no standard library, C programs must either implement their own `int $0x80` assembly trampolines or use the minimal runtime provided in:

```text
tools/jetos_native_shim.h
```

This is the same minimal runtime used when making `jcc.c` and `jas.c` self-hostable on JetOS.

It contains examples of implementing functionality such as:

* `malloc` using a bump allocator
* File I/O
* `strlen`
* `strcmp`
* `memcpy`

All of these can be implemented using only the native syscalls without libc.

---

# Program Execution Model

```c
void elf_load(
    const char *path,
    const char **argv,
    int argc,
    elf_load_result_t *result
);

void elf_run(
    const elf_load_result_t *result
) __attribute__((noreturn));

int elf_execute(const char *path);

int elf_execute_argv(
    const char *path,
    const char **argv,
    int argc,
    void (*stdout_sink)(const char *s)
);
```

### Execution Constraints

* Only statically linked binaries are supported.
* Dynamic linkers and shared libraries are not supported.
* Each process has a fixed **16 MiB user-space region**.
* PIE (Position-Independent Executables) are not supported.
* Binaries must be linked for a fixed address.
* `argc` and `argv` are passed at program entry according to the standard C `main(argc, argv)` convention, using `RDI` and `RSI`.

---

## Build Example

Compile a program using the JetOS-native toolchain (`jcc`/`jas`), insert the resulting binary into a JETFS image using `mkjetfs`, and import it using `jetfs_import`:

```bash
./jcc myprogram.c -o myprogram.elf
./mkjetfs jetfs.img 128
./jetfs_import jetfs.img myprogram.elf /myprogram.elf
```

Then execute it from JetOS:

```text
run /myprogram.elf arg1 arg2
```
