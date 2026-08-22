# JetOS

JetOS is not simply a project for making yet another operating system.

We make JetOS to understand how computers work at a lower level, to make an operating system ourselves, and to explore the boundary between software and hardware.

JetOS considers making things directly, understanding them directly, and controlling them directly as important as possible.

---

## 1. What is JetOS?

JetOS is an independent operating system project that we are developing ourselves.

An operating system is not simply a program. It is the most basic layer that connects various hardware and software elements such as the computer's memory, processor, storage device, input device, processes, and file system.

JetOS aims to learn how the computers we use actually work by directly implementing the core parts of an operating system.

JetOS does not aim to simply copy an existing operating system.

**Making our own operating system** is the core of JetOS.

---

## 2. Why are we making JetOS?

### We made it because we were bored. That's all.

---

## 3. The Philosophy of JetOS

### Make it ourselves

Whenever possible, we directly implement things rather than relying on large existing systems.

The process of implementing things directly is difficult, but through that process, we can understand the system more deeply.

### Make a system that can be understood

JetOS does not aim to make an operating system that simply works.

We aim for an operating system where we can understand why it works.

### Consider simplicity important

Operating systems are already sufficiently complicated.

Therefore, rather than adding unnecessary complexity, we consider it important to clearly implement the necessary features.

### Do not be afraid of experiments

JetOS is not a project made only to create a finished product.

We develop the operating system by trying new methods, failing, and implementing them again.

Failure is also part of the development process.

### Provide an operating system that can actually be used by users

JetOS does not aim to end as a simple kernel experiment.

Ultimately, it aims to develop into one operating system that can actually be used on a real computer.

---

## 4. Why was it made this way?

In operating system development, completely different problems from the programming we usually encounter appear when going down even a little.

In ordinary programs, the operating system handles many things for us.

However, from the position of making an operating system, we have to directly implement the things that the operating system used to do for us.

We have to manage memory ourselves,

communicate with hardware,

understand the operation of the processor,

handle input and output,

and create a way to store and read files.

In other words, **operating system development is different from making a program on top of a program.**

It is closer to making a program that allows a computer to execute programs.

That is why JetOS deliberately goes down to a low level.

The process itself is not the important reason for making JetOS, but it is important too.

---

## 5. Is OS development difficult?

**It is difficult. Very difficult.**

In ordinary application development, you can use numerous functions already provided by the operating system.

However, when making an operating system, you have to make the foundation yourself.

Even when adding one small feature, it may be necessary to connect it with several other systems.

Also, in an operating system, one small mistake can affect the operation of the entire system.

Even displaying text on the screen may not end simply with a `print()` call in an operating system.

That does not mean OS development is impossible.

Operating system development is **not making everything at once, but making a system one by one.**

You can make the operating system by solving small goals one by one, such as making a keyboard work,

managing memory,

storing files,

and executing programs.

It is difficult, but there is also a lot that can be learned from it.

---

## 6. What does JetOS aim for?

What JetOS ultimately aims for is not simply a kernel demo.

It is to make an operating system that can turn on a computer,

execute programs,

manage files,

interact with users,

and actually be used as one computer system.

However, increasing only the number of features is not the goal.

JetOS considers **the fact that we made it ourselves and that we can understand the system** important, but it also has an educational purpose.

---

## 7. License

The source code of JetOS can be used according to the conditions of the license specified by the project.

### It is the GNU Affero General Public License v3.0.

---

## 8. Conclusion

JetOS did not start from a huge goal.

It started from the question of how computers work.

And one of the most direct ways to answer that question is to make an operating system ourselves.

JetOS is a project that understands computers one by one from the lowest level through that process and ultimately makes one operating system that we made ourselves.

**JetOS is a project that uses an operating system, but is also a project that understands computers while making an operating system, and a project that was made because we were bored.**
