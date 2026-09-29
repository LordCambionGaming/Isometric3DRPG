# Isometric RPG

A 3D Isometric Fantasy RPG built with C++, Raylib, and procedural generation.

## Overview

This project is a foundation for a fully-featured 3D isometric RPG with procedurally generated worlds. The game emphasizes computer-generated content over external assets, creating worlds through code-based geometry, noise algorithms, and data-driven systems.

## Features

### Current (Phase 1)
- **3D Isometric Camera**: Full 3D camera with isometric view, zoom, pan, and rotation
- **Procedural World Generation**: Deterministic world creation using seeds
- **Chunk-based Terrain**: Efficient terrain system with runtime mesh generation
- **Modular Architecture**: Clean separation of systems for future expansion
- **Cross-platform Build**: CMake-based build system with GitHub Actions CI

### Planned Features
- Player character and movement
- NPC system with AI
- Combat system with D20 mechanics
- Inventory and item system
- Quest system
- Save/load functionality
- Advanced procedural generation (biomes, structures, dungeons)
- Data-driven content via JSON

## Technical Architecture

### Core Systems
- **Game**: Main game loop and system coordination
- **Renderer**: 3D rendering with Raylib
- **World**: Chunk-based world management
- **Camera**: Isometric 3D camera controller
- **AssetManager**: Resource loading and management
- **AudioManager**: Sound and music system

### Procedural Generation
The game uses deterministic procedural generation:
- **World Seed**: Consistent world generation from seeds
- **Noise-based Terrain**: Multi-octave noise for realistic heightmaps
- **Runtime Mesh Creation**: Terrain meshes generated in real-time
- **Color-coded Biomes**: Height-based terrain coloring

## Building

### Prerequisites
- CMake 3.20 or higher
- C++20 compatible compiler
- Git

### Build Steps
```bash
# Clone repository
git clone <repository-url>
cd IsometricRPG

# Configure and build
cmake -S . -B build
cmake --build build

# Run the game
./build/IsometricRPG
```

### Dependencies
All dependencies are automatically fetched via CMake FetchContent:
- **Raylib 5.0**: Graphics, input, and audio
- **nlohmann/json 3.11.3**: JSON parsing (for future data systems)

## Controls

- **Mouse Drag**: Pan camera
- **Mouse Wheel**: Zoom in/out
- **Arrow Keys**: Rotate camera
- **WASD**: Move camera
- **Home**: Reset to default isometric view
- **R**: Regenerate world with random seed
- **F1**: Toggle debug information
- **F3**: Toggle additional debug info
- **Escape**: Exit game

## Project Structure

```
IsometricRPG/
├── include/           # Header files
│   ├── core/         # Core game systems
│   ├── rendering/    # Rendering and camera
│   ├── world/        # World and terrain
│   ├── assets/       # Asset management
│   └── audio/        # Audio systems
├── src/              # Source files (mirrors include structure)
├── data/             # Game data (JSON configurations)
├── assets/           # External assets (textures, models, audio)
├── saves/            # Save game files
└── .github/          # CI/CD workflows
```

## Development Principles

### Procedural First
- World generated entirely by code
- Minimal dependency on external 3D assets
- Geometric primitives and procedural meshes
- Runtime content creation

### Data-Driven Design
- Game content defined in JSON files
- C++ implements systems, JSON defines content
- Easy content addition without code changes
- Modular and extensible architecture

### Extensible Foundation
- Clean system separation
- Future-proof architecture
- Supports incremental feature addition
- Memory-safe C++ with smart pointers

## GitHub Actions

The project includes automated CI/CD:
- **Build Verification**: Automatic compilation on push/PR
- **Cross-platform**: Ubuntu-based builds
- **Dependency Management**: Automated dependency resolution

## Contributing

This project follows a phase-based development approach:

1. **Phase 1** ✅: Engine Foundation
2. **Phase 2**: Procedural World Enhancement
3. **Phase 3**: Player Character System
4. **Phase 4**: Entity and NPC Systems
5. **Phase 5**: Data-Driven Content
6. **Phase 6**: Combat System
7. **Phase 7**: Inventory and Items
8. **Phase 8**: Quests and NPCs
9. **Phase 9**: Save System
10. **Phase 10**: Polish and Effects

## Architecture Goals

- **Maintainable**: Clear code organization and documentation
- **Scalable**: Support for large worlds and complex systems
- **Performant**: Efficient chunk loading and rendering
- **Modular**: Independent systems with clear interfaces
- **Data-Driven**: Content creation without code modification

## License

[License to be determined]

## World Generation

The game uses a sophisticated procedural generation system:

### Seed-based Generation
- Deterministic: Same seed = same world
- Configurable: Easy to experiment with different worlds
- Reproducible: Perfect for testing and sharing worlds

### Terrain Generation
- Multi-octave Perlin noise
- Height-based biome assignment
- Runtime mesh creation
- Chunk-based streaming (foundation for future optimization)

### Visual Style
- Low-poly, stylized aesthetic
- Readable over photorealistic
- Color-coded terrain types
- Fantasy-themed procedural content

## Debug Information

Press F1 to view debug information:
- Frame rate (FPS)
- Camera position and settings
- World seed information
- Control reference

The debug system provides transparency into the engine's operation and helps with development and troubleshooting.