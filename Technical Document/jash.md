# jash — JetOS Built-in Shell Command Reference

Source: `kernel/apps/jash.c`

jash is the default command-line shell built into JetOS.

It can be opened through the terminal icon on the desktop or started automatically during system boot.

## Files / Directories

| Command                   | Description                                                                      |
| ------------------------- | -------------------------------------------------------------------------------- |
| `pwd`                     | Print the current working directory                                              |
| `cd <path>`               | Change the current directory                                                     |
| `ls [path]`               | List directory contents                                                          |
| `mkdir <path>`            | Create a directory                                                               |
| `touch <path>`            | Create an empty file                                                             |
| `cat <path>`              | Print file contents                                                              |
| `write <path> <text>`     | Overwrite a file with text                                                       |
| `rm <path>`               | Delete an item by moving it to the recycle bin — not immediately deleted         |
| `recyclebin` / `trash`    | Show the contents of the recycle bin                                             |
| `restore <name>`          | Restore an item from the recycle bin to its original location                    |
| `purge <name\|all>`       | Permanently delete an item from the recycle bin; cannot be recovered             |
| `search <query>` / `find` | Recursively search for files/directories whose names contain the specified query |
| `chmod <ro\|rw> <path>`   | Enable or disable the read-only attribute                                        |
| `chown <uid> <path>`      | Change file ownership — root only                                                |

## User Accounts

| Command                     | Description                             |
| --------------------------- | --------------------------------------- |
| `whoami`                    | Display the current user's name and UID |
| `useradd <name> <password>` | Create a new account — root only        |
| `su <name> <password>`      | Switch to another user account          |

## Program Execution

| Command                 | Description                                                                                       |
| ----------------------- | ------------------------------------------------------------------------------------------------- |
| `run <path> [args...]`  | Execute an ELF64 or PE32+ (`.exe`) executable. Additional arguments are passed through as `argv`. |
| `install <path> <name>` | Register/install a program                                                                        |
| `uninstall <name>`      | Unregister the program and move its executable to the recycle bin                                 |
| `programs` / `apps`     | List installed programs                                                                           |

## Networking

| Command                                                | Description                                           |
| ------------------------------------------------------ | ----------------------------------------------------- |
| `net`                                                  | Display NIC MAC address, IP address, and gateway      |
| `arp <ip>`                                             | Resolve a MAC address using ARP                       |
| `ping <ip>`                                            | Send an ICMP echo request (example: `ping 10.0.2.2`)  |
| `nslookup <hostname>`                                  | Perform a DNS lookup                                  |
| `http <host> <port> </path> [save-to]`                 | Perform an actual TCP GET request                     |
| `https <host> <port> </path> [save-to]`                | Perform an actual TLS 1.2 HTTPS GET request           |
| `httprun` / `httpsrun <host> <port> </path>`           | Download an ELF executable and immediately execute it |
| `browser <host> <port> </path>`                        | Open a GUI browser window                             |
| `usbls` / `usbimport <name> [/path]` / `usbrun <name>` | List, import, or execute files from a FAT32 USB drive |

## System

| Command                 | Description                                         |
| ----------------------- | --------------------------------------------------- |
| `shutdown` / `poweroff` | Shut down the system                                |
| `reboot` / `restart`    | Reboot the system                                   |
| `mem`                   | Display memory usage statistics (heap/PMM)          |
| `kbdstat`               | Display keyboard IRQ and buffer diagnostic counters |
| `vinput`                | Display virtio-input (tablet) diagnostics           |

## Other

| Command                                           | Description                        |
| ------------------------------------------------- | ---------------------------------- |
| `base64 encode <text>` / `base64 decode <base64>` | Encode/decode Base64 data          |
| `beep [freq] [ms]` / `melody <scale\|fanfare>`    | Play sounds through the PC speaker |
| `help`                                            | Display the command list           |
| `exit`                                            | Exit the shell                     |

## Usage Examples

```text id="8r6q1x"
jash /> useradd guest guestpass
useradd: created (uid=1)

jash /> write /hello.txt Hello JetOS
File written

jash /> run hello.exe arg1 arg2
launched.

jash /> whoami
root (uid=0)
```
