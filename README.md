# Memory Scanner (Educational Project)

## Overview

This application is a **simple Windows memory scanner** written in **C using the Win32 API**.
It was created for **educational purposes** to demonstrate how user‑space applications store and modify values in memory at runtime.

The tool allows attaching to a running process, scanning its memory for specific values, refining results, modifying values, and freezing memory addresses.

This project is intended for **offline, single‑player, freely available software only**.

## Features

* List and search running processes
* Attach to a selected process
* Scan memory for:
  * Integer (`int`)
  * Float (`float`)
  * Double (`double`)
  * Byte (`unsigned char`)
* Refine scans to narrow down results
* Modify selected memory values
* Freeze values to prevent them from changing
* Pause/resume frozen addresses
* Simple graphical user interface (Win32)

## Building

Use the provided Makefile:

```sh
make build
```

This produces:

```text
scanner.exe
```

## Usage

1. **Run as Administrator**
2. Launch a target application (offline, single‑player)
3. Start `scanner.exe`
4. Select a process from the list
5. Click **Attach to Process**
6. Choose a scan type (int, float, double, byte)
7. Enter a value and click **Initial Scan**
8. Change the value in the target application
9. Enter the new value and click **Refine Scan**
10. Select a match and:

    * Modify the value
    * Freeze the value

Double‑click a frozen entry to pause or resume it.
