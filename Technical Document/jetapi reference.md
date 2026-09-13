# JetAPI — JetOS Native API Reference

Header: `include/jetapi/jetapi.h`

JetAPI is the standard interface through which JetOS applications access system services without directly interacting with internal kernel structures such as `wm_window_t` or `jetfs_inode_t`.

```text
Application → JetAPI → JetOS System Services → JetOS Kernel
```

Function names follow forms familiar to Windows developers, but have a `J` suffix to clearly distinguish them as **JetOS-specific APIs rather than Windows APIs**. No Windows DLLs or SDKs are used. Everything is implemented directly by JetOS.

Programs that intend to call this API must be built as JetOS-native ELF64 executables. See the execution model in `03_JETFS_Syscall_Reference.md`.

PE32+ programs built for actual Windows do not use this API. Instead, they use the Win32/MSVCRT compatibility layer described in `02_Win32_MSVCRT_Compat_Reference.md`.

---

## Window Management

```c
typedef int JetWindowHandle; /* -1 = failure */

JetWindowHandle CreateWindowJ(const char *title, int x, int y, int w, int h, uint32_t color);
void MoveWindowJ(JetWindowHandle win, int dx, int dy);
void BringWindowToFrontJ(JetWindowHandle win);
```

* **CreateWindowJ** — Creates a new window. `color` specifies the background color in `0xRRGGBB` format. Returns `-1` on failure.
* **MoveWindowJ** — Moves a window by the specified relative coordinates (`dx`, `dy`).
* **BringWindowToFrontJ** — Brings the specified window to the top and gives it focus.

## File Operations

```c
int CreateFileJ(const char *path, int is_directory);
int ReadFileJ(const char *path, void *buffer, uint64_t max_size, uint64_t *out_size);
int WriteFileJ(const char *path, const void *data, uint64_t size);
int DeleteFileJ(const char *path);          /* Permanent deletion — immediately removes the file */
int RenameFileJ(const char *old_path, const char *new_path);
int MoveToRecycleBinJ(const char *path);    /* Same behavior as jash's rm — moves to the recycle bin */
int IsReadOnlyJ(const char *path);
int SetReadOnlyJ(const char *path, int readonly);

typedef void (*JetFileListCallback)(const char *name, int is_dir, uint64_t size, void *ctx);
int ListDirectoryJ(const char *path, JetFileListCallback cb, void *ctx);
```

All paths are absolute paths relative to the JETFS root, using the `/dir/file` format.

These functions internally delegate to the JETFS API described in `03_JETFS_Syscall_Reference.md`.

* **CreateFileJ** — Creates a new file or directory (`is_directory=1`).
* **ReadFileJ / WriteFileJ** — Read and write an entire file in a single operation. These are not streaming APIs because JETFS itself provides whole-file read/write operations. If partial or streaming-style I/O is required, ELF programs should directly use the `SYS_OPEN`, `READ`, `WRITE_FD`, and `CLOSE` syscalls.
* **DeleteFileJ** — Permanently deletes the specified file or directory immediately. To move an item to the recycle bin instead, use **MoveToRecycleBinJ**.
* **IsReadOnlyJ / SetReadOnlyJ** — Query or modify the read-only attribute. When this attribute is enabled, write and delete operations are rejected. Renaming is an exception, following Windows conventions.
* **ListDirectoryJ** — Calls the supplied callback once for each directory entry, in sequence.

## Memory Management

```c
void *VirtualAllocJ(uint32_t size);
void VirtualFreeJ(void *ptr);
```

These are simple wrappers built on top of the kernel heap (`kmalloc`/`kfree`).

Fine-grained control provided by a real `VirtualAlloc` implementation, such as per-process virtual address-space reservation or page-level commit operations, is not currently supported.

**Known limitation:** `VirtualAllocJ` is a heap wrapper, not a full virtual-memory management API.

## Threads

```c
typedef void (*JetThreadEntry)(void *arg);
int CreateThreadJ(const char *name, JetThreadEntry entry, void *arg);
```

Creates a JetOS kernel thread directly through the kernel thread subsystem (`kernel/proc/thread.h`).

The return value is the thread ID, or `-1` on failure.

These threads share the same address space as the kernel and do **not** provide Ring3 isolation.

If an isolated process is required, use the ELF loader (`elf_execute` and related functionality).

## System Information

```c
typedef struct {
    uint64_t heap_total_bytes;
    uint64_t heap_used_bytes;
    uint64_t pmm_total_pages;
    uint64_t pmm_free_pages;
    uint64_t timer_ticks;
} JetSystemInfo;

void GetSystemInfoJ(JetSystemInfo *out);

typedef void (*JetThreadListCallback)(const char *name, uint32_t id, int state, void *ctx);
void ListThreadsJ(JetThreadListCallback cb, void *ctx);
```

These diagnostic APIs are used by applications such as Task Manager and system settings.

`ListThreadsJ` invokes the callback for every currently existing thread, including terminated threads that have not yet been reclaimed.

---

## Known Limitations

* The window API does not currently provide advanced features such as window styles, menus, or parent-child window relationships. It currently provides basic position, size, and color functionality.
* `VirtualAllocJ` is a wrapper around the kernel heap rather than a true page-level virtual memory management API.
* Threads created with `CreateThreadJ` do not have Ring3 isolation and therefore share an address space with the kernel.
* For isolated execution, applications should use the ELF process execution mechanism instead of `CreateThreadJ`.
