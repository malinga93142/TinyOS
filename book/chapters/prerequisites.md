# Prerequisites & Environment Setup

Before building a kernel from the ground up, you need to set up a clean, predictable development environment. Operating system development is extremely sensitive to toolchain behaviors, linker scripts, and target architectures.

This guide outlines the software, host setup, and foundational knowledge required to follow along smoothly.


## 1. Host Operating System

The instructions and build scripts in this book assume a **GNU/Linux** host environment (Ubuntu/Debian, Fedora, or Arch Linux are ideal).

* **macOS** users can develop using a Docker container or a Linux virtual machine, as macOS native toolchains (like Clang/lld) handle ELF binaries and multiboot headers differently.
* **Windows** users are strongly encouraged to use **WSL2 (Windows Subsystem for Linux)** running Ubuntu.


## 2. Required Software Packages

To compile the assembly stubs, link raw binary images, create bootable ISOs, and test your kernel in an emulator, install the following packages:

### On Ubuntu / Debian / WSL:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso grub-pc-bin grub-common qemu-system-x86_64 mtools

```

### On Fedora:

```bash
sudo dnf install @development-tools nasm xorriso grub2-tools qemu-system-x86

```

### What Each Tool Does:

* **`build-essential`**: Provides GCC, GNU `make`, and core compilation utilities.
* **`nasm` or GNU `gas**`: Used for writing low-level x86 assembly. (The listings in this book use GNU `gas` in AT&T syntax).
* **`xorriso`**: Required to generate bootable ISO images containing GRUB and your kernel binary.
* **`grub-pc-bin` / `grub-common**`: Supplies GRUB bootloader modules needed to create a Multiboot2-compliant ISO.
* **`qemu-system-x86_64`**: The hardware emulator used to boot and debug your kernel without risking your host machine.

## 3. The Cross-Compiler Consideration

While you *can* sometimes use your host's default GCC compiler (`gcc`), doing so for OS development can introduce subtle bugs because the host compiler targets your host OS (e.g., Linux user-space with glibc linked by default).

For a production-grade OS, you should ideally build a **GCC cross-compiler targeting `x86_64-elf**`.

You can verify if you need one by checking your compiler target:

```bash
gcc -dumpmachine

```

If it outputs something like `x86_64-linux-gnu`, your compiler expects a Linux runtime. For early-stage kernel development, we will bypass standard libraries entirely (`-ffreestanding`, `-nostdlib`), making a bare-metal `x86_64-elf` cross-compiler the cleanest choice. (Detailed instructions for building an `x86_64-elf` cross-compiler can be found on the OSDev Wiki).

## 4. Recommended Background Knowledge

Building an operating system touches every layer of computer systems. You will feel right at home if you are comfortable with:

* **C Programming:** Pointers, manual memory management, bitwise operations (`&`, `|`, `<<`, `>>`), and volatile variables.
* **Assembly Language:** Basic x86 registers (`rax`, `rsp`, `rip`, control registers like `cr0`, `cr3`), stack frames, and calling conventions.
* **Linker Scripts:** Understanding text, data, and bss sections, and how Virtual Memory Addresses (VMA) differ from Load Memory Addresses (LMA).
* **Computer Architecture:** How CPUs fetch instructions, caches, and basic memory management unit (MMU) concepts.

## 5. Verifying Your Setup

Run the following command to verify that your QEMU and build tools are ready:

```bash
qemu-system-x86_64 --version

```

If QEMU responds with version information and your assembler/compiler are accessible, you are ready to tackle Chapter 1 and bring your kernel into long mode!
