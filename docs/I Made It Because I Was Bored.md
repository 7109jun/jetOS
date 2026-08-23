# JetOS — I Made It Because I Was Bored

## 1. Why Am I Making JetOS?

Why did I make JetOS?

I just wanted to make an operating system.

That's it.

There is no special reason.

I didn't make it to make money, and I didn't make it to replace existing operating systems.

One day, I just wanted to make an operating system.

So I started making one.

But once I actually started, I realized there was an enormous amount of work to do.

Something as ordinary as

```cpp
printf("meat is good");
```

can't simply run without an operating system.

Something has to display the text, manage memory, and provide a way to execute programs.

"Wait, I need all of this just to print one thing?"

So I started making these things one by one.

And I kept making more.

That's JetOS.

Why did I make it?

**Because I just wanted to make it.**

## 2. Why Is OS Development Difficult?

OS development is difficult.

But you don't need to study every reason why from the beginning.

You'll understand once you actually try it.

In a normal program, you can write something like this:

```cpp
printf("meat is good");
```

And the text appears on the screen.

That's it.

But when you're making the operating system itself, things are different.

Wait.

There is no `printf()`.

Then you have to make it yourself.

You need a screen to display text.

You need to deal with the graphics hardware to use the screen.

You need memory.

You need a way to execute programs.

Eventually, you realize that a huge amount of code is hiding behind one simple line.

So OS development is difficult.

Actually, it's not just difficult.

**It's an abyss.**

But you don't necessarily need to study everything from the beginning.

**You still need to know C and Assembly, though.**

It's easier to just do things first and think about them later.

I find it easier to think as if I live in a one-dimensional world.

## 3. Screw the Theory 💀

When you start OS development, it looks like there is an enormous amount of stuff you have to study.

CPU Architecture.

Paging.

Interrupts.

GDT.

IDT.

Memory Management.

Processes.

Schedulers.

Filesystems.

TCP/IP.

And so on.

But if you study all of these before starting, it gets boring.

So this book doesn't do that.

We don't study every concept beforehand.

**We just start building.**

You need something.

You don't know how it works.

Then you look it up.

For example, you want to display text on the screen.

So you try it.

It doesn't work.

You find out why.

You discover the framebuffer.

You use it.

Then another problem appears.

The memory is acting strangely.

So you learn about memory.

Another problem appears.

The addresses are strange.

So you learn about virtual memory.

That's how it goes.

```text
I want to do something
↓
Just build it
↓
It doesn't work
↓
Why?
↓
Learn the concept you need
↓
Fix it
↓
Build the next thing
```

This is not a book about studying first and then building.

**It's a book about learning while building.**

## 4. Let's Just Boot It

If we're going to make an operating system, we should at least make the operating system run.

Nothing complicated is necessary.

The first goal is simple.

**Make my code run on a computer.**

Write the JetOS code.

Build it.

Make the bootloader.

Run QEMU.

And JetOS starts up.

At first, there is nothing.

Just a black screen.

But when you see that black screen, you can think:

"Good. Now I can start building from here."

## 5. QEMU Is Awesome

When you develop an OS, you end up using QEMU a lot.

Because you can break things without worrying about it.

You messed up the kernel.

It dies.

Close QEMU.

Build again.

Run it again.

It dies again.

Fix it again.

You don't have to keep doing this on a real PC.

With QEMU, you can just keep breaking things and rebuilding them.

```text
Code
↓
Build
↓
QEMU
↓
It breaks
↓
Fix it
↓
QEMU again
```

Keep repeating this.

And eventually, it works.

That feels good.

So this book will use QEMU a lot.

**QEMU is awesome.**

## 6. So, What Do We Build Now?

JetOS boots.

But it can't do anything.

So we build things one by one.

Display text on the screen.

Read the keyboard.

Read the mouse.

Manage memory.

Read files.

Execute programs.

Create windows.

Connect to the network.

And keep building.

Along the way, a bunch of weird terms will keep appearing.

`kmalloc`

`Paging`

`Interrupt`

`IDT`

`Ring 3`

`System Call`

`TCP`

`PE32+`

You don't need to know them from the beginning.

**Learn them when you need them.**

## 7. Learn About Kernel Memory

Let's say we write code like this:

```cpp
void* memory = kmalloc(4096);
```

We called `kmalloc()` because we needed some memory.

Then a problem appears.

**How do we know where that memory actually is?**

That's where we start digging.

We learn what a memory address is.

We learn how RAM works.

We learn how the kernel uses memory.

We learn why Paging exists.

We learn why Page Tables exist.

Then we look at JetOS's memory management code again.

The code that looked strange at first starts to make sense.

This is how we learn in this book.

We don't start by saying:

"This is a concept called Paging."

Instead, we start with:

**"Why is this address weird?"**

## 8. And We Keep Building

There is no fixed answer in this book.

Because JetOS keeps changing.

Today's JetOS might not be the same as tomorrow's JetOS.

A new driver might be added.

The memory management system might change.

The GUI might change.

The network stack might change.

A bug might force us to completely redesign something.

And that's okay.

**That's OS development.**

You don't create a perfect architecture from the beginning and implement everything exactly according to it.

You build it.

Break it.

Fix it.

Build it again.

And gradually make it better.

This book records that process.

## JetOS — I Made It Because I Was Bored

This book isn't here to teach you how to create a perfect operating system.

It isn't a complete textbook on operating system theory, either.

We just make JetOS.

**I started because I was bored.**

Then I needed a bootloader.

I needed memory.

I needed a keyboard.

I needed a mouse.

I needed a GUI.

I needed a filesystem.

I needed networking.

I needed program execution.

So I built them one by one.

And whenever I got stuck, I looked into it.

**That's what this book is.**
