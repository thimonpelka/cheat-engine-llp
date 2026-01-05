# Memory Scanner - Low Level Programming Project

A Windows-based memory scanner tool for educational purposes, demonstrating low-level programming concepts like process memory manipulation, pointer detection, and multi-threaded programming.

## Project Structure

The project has been refactored into a modular structure with organized directories:

```
cheat-engine-llp/
├── main.c              # Application entry point
├── include/            # Header files
│   ├── types.h         # Shared data structures and constants
│   ├── gui.h           # GUI function declarations
│   ├── scanner.h       # Scanner function declarations
│   ├── freezer.h       # Freezer function declarations
│   └── process_manager.h  # Process manager declarations
├── src/                # Source files
│   ├── gui.c           # User interface implementation
│   ├── scanner.c       # Memory scanning logic
│   ├── freezer.c       # Value freezing functionality
│   └── process_manager.c  # Process enumeration and selection
├── build/              # Compiled object files (generated)
├── Makefile            # Build configuration
└── README.md           # This file
```

### Module Descriptions

**Main**
- Application entry point, window initialization, and message loop

**Types (include/types.h)**
- Shared data structures, constants, and type definitions
- Scan type enumerations, Match and FrozenAddress structs

**GUI Module (src/gui.c, include/gui.h)**
- Window procedure and event handling
- UI element creation and layout
- Match and frozen address list updates

**Scanner Module (src/scanner.c, include/scanner.h)**
- Initial memory scan across process address space
- Refine scan to narrow down matches
- Value comparison for different data types
- Pointer detection and classification
- Architecture detection (x86/x64)

**Freezer Module (src/freezer.c, include/freezer.h)**
- Background thread for continuous memory writes
- Frozen address management
- Thread lifecycle control

**Process Manager Module (src/process_manager.c, include/process_manager.h)**
- Process list population with filtering
- Process information management

## Building

### Prerequisites
- GCC (MinGW or similar for Windows)
- Windows SDK headers

### Compilation

```bash
# Build the project
make

# Clean build artifacts
make clean

# Rebuild everything
make rebuild

# Build and run
make run
```

## Features

1. **Process Attachment**: Browse and attach to running Windows processes
2. **Memory Scanning**: Search process memory for specific values (int, float, double, byte)
3. **Scan Refinement**: Narrow down results by scanning only previous matches
4. **Pointer Detection**: Automatically detect which values are pointers to other matches
5. **Memory Modification**: Change values at found memory addresses
6. **Value Freezing**: Lock values to prevent them from changing (with pause/resume)

## Usage

1. Select a process from the list (use search box to filter)
2. Click "Attach to Process" (may require Administrator privileges)
3. Select scan type and enter value to search
4. Click "Initial Scan" to search memory
5. Change the value in the target application
6. Enter the new value and click "Refine Scan"
7. Repeat refinement until you find the correct address
8. Select a match, enter new value, and click "Modify Selected" or "Freeze Value"

## Educational Concepts Demonstrated

- **Process Memory Access**: Using Windows API to read/write process memory
- **Memory Regions**: Understanding committed pages and memory protection
- **Data Type Handling**: Working with different primitive types at the byte level
- **Pointer Arithmetic**: Detecting and analyzing pointer relationships
- **Multi-threading**: Background thread for continuous memory updates
- **Windows GUI Programming**: Win32 API for creating native Windows applications

## Notes

- Requires Administrator privileges to attach to most processes
- Limited to 10,000 matches for performance reasons
- Supports up to 100 frozen addresses simultaneously
- Displays only first 100 matches in the UI

## Disclaimer

This tool is for educational purposes only. Use responsibly and only on processes you own or have permission to modify.
