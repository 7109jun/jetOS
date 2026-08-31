# OS Development

This document is intended to briefly explain what operating system development, or **OS Development (OSDev)**, is.

For people contributing to JetOS or starting operating system development, it is helpful to know the basic concepts of how an operating system is made.

This document briefly summarizes the core contents of operating system development based on various materials from OSDev.org.

---

## 1. What is OS Development?

OS Development is the work of directly making an operating system.

Ordinary programs run on top of an already existing operating system.

However, when developing an operating system, the functions that the operating system provided must be made directly.

For example:

* Memory management
* Process management
* Thread management
* Hardware control
* File system
* Input and output
* Networking
* Program execution
* Security and permission management

must be handled by the operating system itself.

Therefore, OS development deals with the operation of computers at a much lower level than ordinary application development.

---

## 2. OS Development is Difficult

Operating system development is difficult work.

Knowledge of several fields is required, such as computer architecture, programming languages, assembly, memory, CPU, and hardware devices.

In addition, parts that the operating system handles for ordinary programs must also be handled directly.

Therefore, even implementing one small feature can take a lot of time.

In operating system development, it is often difficult to get immediate results.

However, seeing the code you made actually boot, control hardware, and execute programs is one of the important fun parts of OS development.

---

## 3. Basic Knowledge Needed

Before starting OS development, the following basic knowledge can be helpful.

### Programming

It is good to understand C, C++, Rust, or another system programming language.

The following concepts are especially important.

* Variables and data types
* Pointers
* Structures
* Functions
* Memory
* Bit operations
* Arrays
* Stack
* Heap

### Computer Architecture

You need to basically understand how a CPU works.

For example:

* CPU registers
* Instructions
* Memory
* RAM
* ROM
* Addresses
* Stack
* Executable files
* Machine code

and so on.

### Assembly

In operating system development, you often need to write code that is close to the CPU directly.

Therefore, it is useful to be able to at least read the assembly of the CPU you are using.

---

## 4. Booting

Just because a computer has been turned on does not mean the operating system's kernel immediately runs.

The hardware and firmware perform the initial boot process, and afterward the operating system must be prepared in memory so that it can be executed.

A **Bootloader** can be used in this process.

The role of a Bootloader is to prepare and execute the operating system's kernel.

JetOS also executes its kernel through a bootloader in a UEFI environment.

---

## 5. Kernel

The Kernel is the core part of an operating system.

The kernel manages hardware resources and provides the necessary functions so that programs can use system resources.

Its main roles include:

* Memory management
* Process management
* Thread management
* Device management
* Interrupt handling
* System calls
* File system management
* Security and permission management

The roles and structure handled by the kernel can differ depending on the structure of the operating system.

---

## 6. Memory Management

Memory is required to execute programs.

The operating system must track memory and provide appropriate memory to programs that need it.

There are concepts such as:

* Physical Memory
* Virtual Memory
* Paging
* Page Table
* Heap
* Stack
* Memory Allocation

In particular, using Virtual Memory can provide an independent address space for each program.

This also plays an important role in memory isolation between processes.

---

## 7. Interrupt

Various events occur in hardware and software in a computer.

A representative method used by the CPU to handle these events is an **Interrupt**.

For example, keyboard input or timer events can generate interrupts.

The operating system must receive interrupts and execute the appropriate handler.

Interrupts are an important element for an operating system to interact with hardware.

---

## 8. Processes and Threads

An operating system must be able to execute multiple programs at the same time.

For this, **Process** and **Thread** are used.

### Process

A Process is an independent unit of execution that represents a running program.

The operating system manages the memory and execution state of each process.

### Thread

A Thread is a unit of work that executes within a process.

A single process can have multiple threads.

The operating system can manage CPU time by switching between multiple threads.

---

## 9. Scheduler

A CPU can execute only a limited number of execution flows at one moment.

Therefore, an operating system uses a **Scheduler** to execute multiple processes and threads.

The Scheduler determines which execution unit will run on the CPU and when.

Through this, multiple programs can operate as if they are running at the same time.

The scheduling method used can differ between operating systems.

---

## 10. User Space and Kernel Space

In an operating system, the area where programs execute and the area where the kernel executes can be separated.

Ordinary programs execute in **User Space**, while the core code of the operating system executes in **Kernel Space**.

This separation is used to restrict programs from directly accessing important memory belonging to the kernel or other programs.

When a program needs to use a kernel function, it can request the kernel through a **System Call**.

---

## 11. Program Loading

For an operating system to execute a program, it must read the executable file and place it into memory.

This is called **Program Loading**.

The operating system must understand the format of the executable file, place the necessary code and data at appropriate memory locations, and then execute the program.

Representative executable file formats include ELF and PE.

JetOS also implements functionality for loading and executing executable files.

---

## 12. File System

An operating system must be able to store data and read it again.

For this, a **File System** is used.

A file system manages data on a storage device in the form of files, directories, and so on.

When creating a file system, the following problems must be considered.

* File storage
* File reading
* File deletion
* Directory management
* Free space management
* File size
* Data integrity

JetOS uses its own file system, **JETFS**.

---

## 13. Device Driver

For an operating system to use hardware, it needs a way to communicate with that hardware.

This is handled by a **Device Driver**.

To use devices such as keyboards, mice, storage devices, and network cards, processing appropriate for each piece of hardware may be required.

A driver acts as a connection between the operating system and hardware.

---

## 14. Networking

In modern operating systems, networking is also an important part.

Implementing networking requires handling multiple layers and protocols.

For example:

* Ethernet
* ARP
* IPv4
* ICMP
* TCP
* UDP
* DNS
* HTTP

and so on.

Networking is much more complicated than simply sending data.

Various elements such as packet processing, address management, connection state, and error handling must be considered.

---

## 15. Testing

Operating systems are more difficult to test than ordinary programs.

If a problem occurs in an operating system, not only one program may terminate, but the entire system may stop.

Therefore, a testing environment is important in operating system development.

Representative methods include:

* Using an emulator such as QEMU
* Using a virtual machine
* Testing on real hardware
* Using a debugger
* Checking kernel logs
* Writing test programs

and so on.

JetOS also uses QEMU as an important testing environment during development.

---

## 16. Cross Compiler

When developing an operating system, there can be problems with simply using a normal system compiler.

This is because a compiler made for the libraries and runtime of the currently running operating system may not be suitable for the newly created operating system.

Therefore, using a **Cross Compiler** is one of the common methods in OS development.

A Cross Compiler is a compiler configured to generate code for the operating system being developed.

Using one allows the operating system currently being used and the operating system being developed to be separated during the build process.

---

## 17. Version Control

Operating system development can take a long time.

As the code grows and the number of features increases, managing changes also becomes important.

Therefore, using a Version Control System such as Git is recommended.

Using version control allows:

* Tracking changes
* Restoring previous versions
* Experimental changes
* Collaboration between multiple developers
* Code backup

and so on.

JetOS is also developed using Github.

---

## 18. There Is No Correct Development Order

Not every operating system is made in the same order.

The development order can differ depending on the goals and design of the operating system.

Generally, after creating a bootable kernel, features such as memory management, interrupts, scheduling, device support, file systems, user space, and program execution can be added step by step.

However, this is not necessarily one correct order that must be followed.

In operating system development, it is important to choose a development order appropriate for the purpose of the project.

---

## 19. The Most Important Thing

There is no single fixed path in OS development.

Some operating systems may use a monolithic kernel, while others may use a microkernel.

Some projects may target specific hardware, while others may support multiple architectures.

The design of an operating system depends on the goals of the project.

Therefore, rather than simply following the structure of another operating system, it is important to **understand why it was designed that way**.

---

## 20. Starting OS-Dev

Operating system development is not a project that can be finished in a short time.

At first, even displaying text on the screen can feel like a big goal.

After that, numerous problems involving memory, interrupts, processes, file systems, networking, and so on appear.

However, if you solve small goals one by one, the system gradually grows.

OSDev.org also emphasizes that operating system development is difficult and time-consuming, while explaining that there is a great sense of achievement in seeing a system you made yourself actually work.

---

## References

This document summarizes the core concepts based on operating system development materials from OSDev.org.

For more detailed information and various tutorials, refer to the OSDev Wiki.

* https://wiki.osdev.org/Introduction
* https://wiki.osdev.org/Getting_Started
* https://wiki.osdev.org/Creating_an_Operating_System
* https://wiki.osdev.org/Barebones
