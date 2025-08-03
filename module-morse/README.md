# Morse Code Transmitter – Linux Kernel Module

A Linux 2.0 kernel module that transmits characters as Morse code signals using the top-left corner of the screen as a light indicator.

## Features
- **Character device (write-only)**:
  - Accepts uppercase and lowercase ASCII letters, digits, and spaces (space indicates a word pause).
  - Ignores unsupported characters.
- **Morse code transmission**:
  - Signals are transmitted visually by changing the color of the top-left character on the screen.
  - Transmission is non-blocking: `write()` returns after inserting data into the buffer, not after full transmission.
- **Configurable timing parameters via `ioctl`**:
  - Dot duration
  - Dash duration
  - Symbol pause
  - Letter pause
  - Word pause
- **Eight independent devices** identified by minor numbers.
- **Buffered transmission**:
  - Per-device circular buffer (default 256 bytes, adjustable 0–1024 bytes).
  - Resizing the buffer via `ioctl` preserves stored data.
  - Transmission continues even after the device is closed.
  - Blocking `write()` when the buffer is full (no busy waiting).
- **Synchronization** ensures safe concurrent access from multiple processes.
- Implemented as a **loadable kernel module**.

---

## System Requirements
- Linux kernel 2.0 or compatible.
- Root privileges to load and configure the module.

---

