# Ring Buffer Linux Kernel Module

A Linux kernel module implementing a **multi-buffer ring (circular) buffer** with dynamic buffer resizing via `ioctl`.

## Features
- **Four independent ring buffers**, selected by device minor number.
- **Dynamic buffer resizing** (`ioctl`) within **256 B – 16 KB**.
- **Query current buffer size** (`ioctl`).
- Blocking operations:
  - `read()` blocks when the buffer is empty.
  - `write()` blocks when the buffer is full.
- Proper synchronization using semaphores and wait queues.

---

## System Requirements
- Linux kernel 2.0 (or compatible API for kernel modules).
- Root privileges to load/unload kernel modules.

---
