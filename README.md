# CS2 Debug Overlay

A transparent debug overlay application for Counter-Strike 2 that receives drawing commands via named pipes and renders them using raylib.

### Core Components

#### 1. **OverlayApplication** (`include/overlay_application.h`, `src/overlay_application.cpp`)
- Main application orchestrator
- Manages lifecycle of all components
- Coordinates initialization and shutdown
- Handles the main game loop

#### 2. **OverlayRenderer** (`include/overlay_renderer.h`, `src/overlay_renderer.cpp`)
- Handles all rendering operations using raylib
- Manages 3D and 2D drawing commands
- Provides frame management (begin/end)
- Renders debug information

#### 3. **PipeClient** (`include/pipe_client.h`, `src/pipe_client.cpp`) [Windows only]
- Manages named pipe communication with CS2
- Processes incoming packets in a separate thread
- Thread-safe command storage and retrieval
- Automatic reconnection on pipe failures

#### 4. **WindowManager** (`include/window_manager.h`, `src/window_manager.cpp`) [Windows only]
- Handles target window detection (CS2 window)
- Manages window positioning and sizing
- Provides window boundary information

#### 5. **Packet Definitions** (`include/packets.h`)
- Defines all communication protocols
- Packet structures for drawing commands
- World update information

### Key Features

- **Modular Design**: Each component has a single responsibility
- **Platform Abstraction**: Windows-specific code is isolated
- **Thread Safety**: Proper synchronization for multi-threaded operations
- **Resource Management**: RAII-based resource handling
- **Error Handling**: Comprehensive error checking and reporting
- **Extensibility**: Easy to add new drawing commands or features

### Build Requirements

- C++17 compatible compiler
- raylib library
- Windows SDK (for Windows builds)

### Usage

The application automatically:
1. Finds the Counter-Strike 2 window
2. Creates a transparent overlay matching the game window
3. Connects to the named pipe for receiving draw commands
4. Renders received commands in real-time

### Drawing Commands Supported

- **Lines**: 3D line drawing between two points
- **Text**: Both screen-space and world-space text rendering
- **World Updates**: Camera position and orientation synchronization

### Thread Architecture

- **Main Thread**: Handles rendering and UI updates
- **Packet Thread**: Manages named pipe communication (Windows only)
- **Synchronization**: Mutex-protected shared data structures
