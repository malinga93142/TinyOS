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
$ git clone https://github.com/malinga93142/TinyOS.git
$ cd TinyOS
$ make
```
The build produces a kernel image; check the `Makefile` for the exact output path.

### Run in QEMU

```bash
$ make run
```

### Debug (Optional)
Start QEMU with a GDB stub and attach your debugger:
```bash
$ make run-tcg-gdb # tcg simply stands for tiny-code generator.
```
In another terminal
```bash
$ make gdb # this will use a gdb script debug.gdb
```
Or you can do it directly by typing commands:
```bash
$ gdb boot.elf --ex "target remote :1234
```


### Source Layout
All source code lives in two directories:
 - `include/` -- Kernel headers
 - `src/` -- Kernel implementation
Everything else in the tree (notes, experimentation, scripts, etc.,) is supporting material and not part of the build.

### Kernel Capabilities
The kernel is small, but it already does the following:
 - **SMP (symmetric multiprocessing) -- multiple CPUs are brought and run kernel in parallel. Per-CPU data is kept separate, and shared state is protected where it matters. This is the kernel's main area of interest.
 - **Simple task scheduling** -- a basic scheduler picks tasks to run. it is deliberately simple; the goal is to understand scheduling behavior, not to compete with production schedulers.
 - **Paging** -- virtual memory is enabled via paging. Address space setup (see `src/boot.S`, `src/paging.c`, and `src/pmm.c`) and page-table management are handled by the kernel.
 - **Physical Memory Management** -- Physical frames are tracked and handed out to the rest of the kernel. This is the layer that backs paging and any kernel allocations. This management uses simple bitmap allocation and next-scan heuristic.
 - **Syscall interface** -- user code can request the services to kernel through a defined syscall numbers. The interface is minimal and grows as needed.
 - **User/kernel separation** -- user tasks run with restricted privilege and their own address space, and must trap into the kernel to do anything privileged. This separation is what makes the syscall interface meaningful.

### References I Used to Learn
These are the books, courses, and hands-on work that taught me most of what this kernel does. I learned from them; the mistakes in this kernel are mine.

#### Operating Systems
 - Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau -- *Operating Systems: Three Easy Pieces*
 - xv6: a simple, Unix-like teaching operating system
 - Daniel P. Bovet and Macro Cesati - Understanding the Linux Kernel, O'Reily. I used it to see how a real kernel organizes memory addressing, processes, interrupts, and memory management. useful when I wanted to check what a real general purpose or production level kernel does before simplifying it here.

#### Building an OS from Scratch
- **Philipp Oppermann** -- Writing an OS in Rust, https://os.phil-opp.com/ . A step-by-step series that walks through bootloader, VGA output, interrupts, paging, and heap allocation on x86_64, explaining the hardware mechanics along the way . I used it for the practical, incremental method of getting a kernel to boot and then layering features on top.
- My own experiments with the Linux kernel. Beyond the books, I spent time building, modifying, and poking at the Linux kernel directly, and much of my understanding of how the pieces above behave in practice rather than how they are described came from that work.

#### Tools and documentation
- **QEMU** and **GDB**, plus x86_64 architecture manuals (Intel or AMD). These were reference material for running and debugging the kernel, not tutorial reading.

### Languages Used

The kernel is written in **C** (~95%) and **assembly** (~5%).

C does the bulk of the work: scheduler, paging, physical memory management, the syscall path, and the user/kernel split. Assembly is used only where it is actually required, and nowhere else. That means boot and low-level entry paths, context switching, interrupt and exception entry and exit, and the few instructions that C cannot express, or cannot guarantee the compiler will emit the way the hardware requires.

The split is deliberate. Assembly is hard to read, hard to review, and easy to get subtly wrong, so it stays limited to the places where there is no reasonable alternative.
