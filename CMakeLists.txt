cmake_minimum_required(VERSION 3.20)
project(IsometricRPG VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Dependencies
include(FetchContent)

# Raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
FetchContent_MakeAvailable(raylib)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/core/Game.cpp
    src/core/GameState.cpp
    src/core/Timer.cpp
    src/rendering/Renderer.cpp
    src/rendering/Camera3DController.cpp
    src/world/World.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/BiomeManager.cpp
    src/assets/AssetManager.cpp
    src/audio/AudioManager.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} raylib nlohmann_json::nlohmann_json)

# Platform-specific linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} GL m pthread dl rt X11)
endif()

# Set working directory for debugging
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)