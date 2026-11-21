# TinyOS
TinyOS is a small, toy OS kernel. It uses a monolithic architecture where the kernel manages all system resources, meaning every user task has to go through the kernel to access them.

## What it is

This is a small, toy OS kernel built to understand how operating systems work under the hood. It isn't POSIX-compliant or Linux-like, nor does it aim to be a complete, general-purpose OS. Instead, it was written primarily to explore **concurrency** and **multicore dynamics**. As the sole developer, every feature and architectural choice reflects my own design decisions.

## What It Is Not

This is not yet a complete OS kernel. It is not a general-purpose kernel, and it is not intended to serve as an educational OS kernel.

## Building and Using
This kernel is intended for **experimentation only**. It is **not** meant to run on real hardware real hardware, doing so can damage your machine or leave it in an unbootable state. **Running it under QEMU is strictly recommended**. All testing and development were done in QEMU, and that is the environment I can vouch for.

## Prerequisites

Because this is an x86-based OS, the **host compiler (GCC or clang) and toolchain are sufficient**. no cross-compiler is required (well, cross-compiler is optional).
You will need:
 - `gcc` (the one already on your system. if it isn't, just do `apt install gcc`)
 - `make` (which this project uses. you can have your own build system. this project is build agnostic like all)
 - `qemu-system-i386` or `qemu-system-x86_64` (whichever matches the target)
 - `gdb`: for debugging. Optional, but very useful.

### Build
```bash
git clone https://github.com/malinga93142/TintOS.git
cd TinyOS
make
```
The build produces a kernel image; check the `Makefile` for the exact output path.

### Run in QEMU

```bash
make run
```

### Debug (Optional)
Start QEMU with a GDB stub and attach your debugger:
```bash
make run-tcg-gdb # tcg simply stands for tiny-code generator.
```
In another terminal
```bash
make gdb # this will use a gdb script debug.gdb
```
Or you can do it directly by typing commands:
```bash
gdb boot.elf --ex "target remote :1234
```
