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

### Implemented Features
- **Player Character System**: Movement on tile grid with height-aware costs (same level: 1, uphill: 3, downhill: 1)
- **Water System**: Non-traversable water with lake and river generation
- **Biome System**: Height-based biome assignment (Ocean, Beach, Plains, Forest, Hills, Mountains, Desert, Swamp)
- **Navigation System**: Tile-based navigation for player movement
- **Dice Board 3D**: Dedicated screen with 3D D20 die, dedicated font, and game pause during rolls
- **Combat System**: D20 mechanics with critical success/failure handling
- **Main Menu State**: Full menu system with option selection
- **Pause Menu State**: Pause functionality accessible via ESC key
- **Settings State**: Game settings configuration
- **Debug Information**: F1 for basic debug, F3 for additional debug info
- **World Streaming**: Chunk generation and loading around player position

## Technical Architecture

### Core Systems
- **Game**: Main game loop and system coordination
- **Renderer**: 3D rendering with Raylib 5.5
- **World**: Chunk-based world management with streaming
- **Camera**: Isometric 3D camera controller
- **AssetManager**: Resource loading and management (textures, models, fonts)
- **AudioManager**: Sound and music system
- **CombatSystem**: D20 combat mechanics
- **NavigationSystem**: Tile-based navigation for player movement

### Procedural Generation
The game uses deterministic procedural generation:
- **World Seed**: Consistent world generation from seeds
- **Noise-based Terrain**: Multi-octave noise for realistic heightmaps
- **Runtime Mesh Creation**: Terrain meshes generated in real-time
- **Color-coded Biomes**: Height-based terrain coloring

### World Features
- **Terrain Generation**: Voxel/block-based terrain with height levels
- **Chunk System**: Efficient chunk loading and unloading around player
- **Water Features**: Lakes, rivers, and sea generation
- **Biome Manager**: Dynamic biome assignment based on environment conditions

### Procedural Village
Village models and their seeded spawn layout are generated with:

```bash
python src/assets/models/entities/generate_village_assets.py
python src/assets/models/entities/compose_village.py --seed 12345 --houses 7
```

The layout is loaded relative to the player's spawn. Keep `--seed` aligned with the world seed in `Game.cpp` when regenerating it. Village models and NPCs are rendered only while their chunks are in the world's render range.

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
- **Raylib 5.5**: Graphics, input, and audio
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
- **ESC**: Open Pause Menu during gameplay
- **Escape**: Exit game

## Project Structure

```
IsometricRPG/
├── include/           # Header files
│   ├── core/         # Core game systems (Game, GameState, CombatSystem)
│   ├── rendering/    # Rendering and camera
│   ├── world/        # World and terrain generation
│   ├── assets/       # Asset management
│   ├── audio/        # Audio systems
│   └── utils/        # Utility classes (Logger)
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

### Modular Design
- Clean system separation
- Independent systems with clear interfaces
- Memory-safe C++ with smart pointers
- Extensible architecture for future features

### Performance-Oriented
- Chunk-based world streaming
- Efficient terrain generation
- Optimized rendering pipeline
- Smart resource management

## GitHub Actions

The project includes automated CI/CD:
- **Build Verification**: Automatic compilation on push/PR
- **Cross-platform**: Ubuntu-based builds
- **Dependency Management**: Automated dependency resolution

## Contributing

This project follows a phase-based development approach:

1. **Phase 1** ✅: Engine Foundation (Core systems, world generation, player movement)
2. **Phase 2**: Procedural World Enhancement (Biomes, water features, terrain optimization)
3. **Phase 3**: Player Character System (Movement costs, navigation, pathfinding)
4. **Phase 4**: Entity and NPC Systems (NPC implementation)
5. **Phase 5**: Data-Driven Content (JSON configurations)
6. **Phase 6**: Combat System (D20 mechanics, combat rolls)
7. **Phase 7**: Inventory and Items (Item system)
8. **Phase 8**: Quests and NPCs (Quest system)
9. **Phase 9**: Save System (Save/load functionality)
10. **Phase 10**: Polish and Effects (Visual effects, UI polish)

## Architecture Goals

- **Maintainable**: Clear code organization and documentation
- **Scalable**: Support for large worlds and complex systems
- **Performant**: Efficient chunk loading and rendering
- **Modular**: Independent systems with clear interfaces
- **Data-Driven**: Content creation without code modification (future)

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

## Planned Features

- NPC system with AI behavior
- Inventory and item system
- Quest system
- Save/load functionality
- Advanced procedural generation (structures, dungeons)
- Data-driven content via JSON configurations
- Enhanced UI/UX polish
- Additional visual effects
- Multiplayer support (future consideration)

## License

[License to be determined]
