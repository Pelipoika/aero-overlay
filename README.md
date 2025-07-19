# CS2 Debug Overlay

A lightweight, transparent overlay for Counter-Strike 2, rendering real-time debug visuals via shared memory.

## Features
- **Transparent overlay** auto-attaches to CS2 window
- **Draw commands**: 3D lines, text (screen/world), world updates
- **Real-time rendering** with raylib
- **Thread-safe** command handling
- **Windows only**

## Quick Start
1. Build with C++17 and raylib
2. Launch CS2
3. Run the overlay (auto-detects CS2 window)

## Main Components
- OverlayApplication: App lifecycle & main loop
- OverlayRenderer: All rendering (raylib)
- SharedMemoryClient/PipeClient: Receives draw commands
- WindowManager: Handles overlay window

## Requirements
- C++17 compiler
- raylib
- Windows SDK

---
Easy to extend for new debug visuals. For details, see source code and headers.
