# Libraries and Assembly, Get Out

## 1. Just Get Out

The title of this book is strange.

**Libraries and Assembly, Get Out.**

That's exactly what it means.

The idea is to keep libraries and assembly as far away as possible.

Of course, libraries aren't bad.

Assembly isn't bad either.

Both are extremely useful.

The problem starts when you begin using too much of them.

You add one library.

Then that library requires another library.

Then that library requires another library.

And at some point, your project contains more code written by other people than code you actually wrote.

Assembly can be similar.

At first, directly controlling the CPU feels cool.

Then the amount of assembly code gradually increases.

Eventually, you have more assembly code than C code.

So this book says:

**Libraries? Get rid of them whenever possible.**

**Assembly? 9 is fine, but around 10 it starts getting painful.**

---

## 2. Adding a Library

You create a project.

You need one feature.

So you add a library.

Done?

No.

The library requires another library.

So you add that too.

Then there is another dependency.

Add that one too.

Eventually, this happens.

My Project
↓
Library A
↓
Library B
↓
Library C
↓
Library D
↓
Library E
↓
I have no idea what this is

I wanted to use A, so why do I need E?

This is dependency hell.

It can become an even bigger problem in low-level projects.

The versions have to match.

The build environment has to match.

The platform has to match.

The compiler has to match.

The ABI has to match.

And one day, you update something and suddenly the project won't build.

**"It worked yesterday."**

You changed one library, and suddenly the entire project is falling apart.

> #### I fucking hate libraries.

---

## 3. So Let's Remove the Libraries

Of course, this doesn't mean you should implement every library yourself.

There are plenty of situations where using a well-tested library is much better.

But if you're bringing in a huge dependency just for one simple feature in a small project, it's worth thinking about.

For example, if you need a huge library just to process a string:

Maybe implementing it yourself would be simpler.

If you need a complicated dependency tree just to read one file:

Maybe implementing only the part you actually need would be better.

Especially in projects such as operating systems, bootloaders, compilers, and runtimes, reducing external dependencies can be extremely important.

You have more control over the code.

The build process becomes simpler.

Debugging becomes easier.

Distribution becomes easier.

And most importantly:

**"Where the hell did this code come from?"**

becomes a question you have to ask less often.

---

## 4. 3–10 Pieces of Assembly Are Enough

Now, assembly.

Assembly is difficult.

Really difficult.

There are registers.

There is memory.

There is the stack.

There are calling conventions.

There are CPU instructions.

And one tiny mistake can make the entire program silently die.

So just because you like assembly doesn't mean you need to write the entire project in it.

Actually, the opposite.

**Use as little as possible.**

Use assembly only where it is genuinely necessary.

Boot process.

CPU initialization.

Specific low-level functions.

That's it.

In an extreme sense:

**3 pieces of assembly are enough.**

Really, this doesn't mean you should use exactly three instructions.

The point is this:

> Assembly isn't the protagonist of the project. It's a tool you pull out when you actually need it.

If C or C++ can solve the problem, there is usually no reason to write it in assembly.

---

## 5. Would Making Everything in Assembly Be Cool?

At first, it is.

mov ...
mov ...
push ...
pop ...
jmp ...

It feels like you're directly controlling the CPU.

But once the code reaches thousands of lines, things change.

Debugging becomes difficult.

Maintenance becomes difficult.

The code gets longer.

It becomes harder for humans to read.

A small change can affect other parts of the program.

So we minimize assembly.

Things the CPU genuinely needs to handle directly can be written in assembly.

The complicated logic above that can be written in a higher-level language.

Assembly
↓
Low-Level Interface
↓
C / C++
↓
Application Logic

We create boundaries like this.

The goal isn't to eliminate assembly.

**The goal is to distinguish where assembly is necessary from where it isn't.**

---

## 6. Libraries Are the Same

The goal isn't to completely eliminate libraries either.

Use them when you need them.

If there is a good library, using it can be the smart choice.

The problem is adding libraries without thinking.

"I need this feature."
↓
Add a library.
↓
"Oh, I need this too."
↓
Add another library.
↓
"It doesn't build."
↓
Fix the version.
↓
"Now something else is broken."
↓
Fix it.
↓
"Why the hell do I even need this library?"

If this keeps happening, eventually you can't tell whether you're controlling your project or the libraries are controlling your project.

So think about it first.

**Do I really need it?**

Would implementing it myself be simpler?

How many dependencies will this add?

Can I maintain this later?

Ask these questions first.

---

## 7. Making Everything Yourself Isn't Always Better

There is an important problem here.

"So should I just make everything myself?"

No.

That's another extreme.

Make your own cryptography library.

Make your own complicated image codec.

Make your own huge network protocol.

Make your own proven database.

If you start implementing absolutely everything yourself, you enter **implementation hell** instead.

So the point is:

**It's not "remove everything."**

**Remove what you don't need.**

---

## 8. The Idea of Minimal Dependencies

A good project doesn't necessarily have zero dependencies.

A good project is one where you can explain **why each dependency exists.**

For example:

Compiler
├─ Runtime
└─ Standard Library

This can be perfectly reasonable.

But:

Project
├─ Library A
│   ├─ Library B
│   │   ├─ Library C
│   │   │   ├─ Library D
│   │   │   └─ Library E
│   │   └─ Library F
│   └─ Library G
└─ Library H

And if the only thing you're actually using is one function from A?

Then it's worth thinking about.

---

## 9. It's Even More Important in Low-Level Projects

Think about operating system development.

An operating system has to talk directly to hardware.

You need a bootloader.

You need memory management.

You need drivers.

You need a filesystem.

You need networking.

When external dependencies keep increasing in a project like this, things become more complicated.

Especially in a kernel, you can't simply use every library you would normally use in a user-space application.

The environment itself is different.

That's why implementing the minimum functionality you actually need can sometimes be the more natural approach.

---

## 10. In the End, It's About Control

The reason for reducing libraries,

and the reason for reducing assembly,

ultimately comes down to one thing.

**Control.**

I want to know what my code is doing.

I want to know why the build failed.

I want to know why the program crashed.

I want to know what code is actually being executed.

I want to know which dependencies are actually required.

If that's what you want, don't make the project more complicated than it needs to be.

---

## 11. So Let's Draw the Conclusion

Assembly is difficult.

So use as little of it as possible.

Libraries are convenient.

But too many of them lead to dependency hell.

So remove the ones you don't need.

If implementing a small feature yourself is simpler, implement it yourself.

If implementing a huge, well-tested feature yourself would be ridiculously complicated, use a library.

And if assembly is genuinely necessary somewhere, use assembly.

In the end, the important thing isn't an extreme rule.

**Keep only what you need.**

Assembly.

Libraries.

Code.

Dependencies.

**If you don't need it, get out.**

That's the title of this book.

# Use Only the Libraries You Need, Get the Rest Out. Use 3–10 Pieces of Assembly, Get the Rest Out.
