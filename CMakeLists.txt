cmake_minimum_required(VERSION 3.16...3.30)
set(CMAKE_POLICY_VERSION_MINIMUM 3.16)

project(Isometric3DRPG VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# FetchContent
include(FetchContent)

# Raylib 5.5 (automatic, no manual install)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

# nlohmann/json v3.11.3
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# Source files (preserve all existing .cpp files from the project)
set(SOURCES
    src/main.cpp
    src/core/Game.cpp
    src/core/GameState.cpp
    src/core/MainMenuState.cpp
    src/core/PauseState.cpp
    src/core/SettingsState.cpp
    src/player/Player.cpp
    src/core/Timer.cpp
    src/core/DiceRollState.cpp
    src/core/CombatSystem.cpp
    src/rendering/Renderer.cpp
    src/rendering/Camera3DController.cpp
    src/world/World.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/BiomeManager.cpp
    src/world/WorldFeatureGenerator.cpp
    src/assets/AssetManager.cpp
    src/audio/AudioManager.cpp
    src/navigation/NavigationSystem.cpp
    src/ui/HUD.cpp
    src/ui/DiceRoller.cpp
    src/utils/Logger.cpp
    
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Target-oriented include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    nlohmann_json::nlohmann_json
)

# System dependencies (Windows / Linux)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE GL m pthread dl rt X11)
endif()

# Copia gli asset (font gotico, ecc.) accanto all'eseguibile
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Working directory for debugging (matches current runtime paths: data/, saves/)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)