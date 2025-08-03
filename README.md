# Repository Overview

This repository contains **two Linux kernel modules** designed as educational projects for character device drivers:

1. **Ring buffer**
2. **Morse Code Transmitter** – A buffered Morse code transmitter that uses screen signals (upper-left corner of the console) to represent dots, dashes, and pauses.

Both modules are implemented as **loadable kernel modules (LKM)**, compiled outside the kernel source tree, and controlled via standard Linux tools (`insmod`, `rmmod`, `lsmod`). They demonstrate concepts such as:
- Character device registration
- Buffer management
- Synchronization mechanisms
- Use of timers and I/O control (ioctl)
- Interaction with hardware/console display

---

## Compilation

### Requirements
- The kernel must be properly configured before building the modules:
```zsh
  make menuconfig
```

### Compilation Using Script

To compile the module, run:
```zsh
  ./gcc-module.sh module.c
```
This will produce an output file module.o.

# Module Management

### Load the Module

To insert the module into the kernel:

```zsh
insmod module
```

### List Loaded Modules

To display all currently loaded modules along with their reference counters:

```zsh
lsmod
```

### Remove the Module
To remove the module from the kernel:

```zsh
rmmod module
```
