#include "assets/AssetManager.h"
#include "utils/Logger.h"
#include <filesystem>

AssetManager::AssetManager()
    : m_initialized(false)
{
}

AssetManager::~AssetManager() {
    Shutdown();
}

bool AssetManager::Initialize() {
    try {
        // Create assets directory if it doesn't exist
        std::filesystem::create_directories(ASSETS_PATH);

        m_initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        Logger::LogError("Failed to initialize AssetManager: " + std::string(e.what()));
        return false;
    }
}

void AssetManager::Shutdown() {
    if (!m_initialized) return;

    Clear();
    m_initialized = false;
}

Texture2D* AssetManager::LoadTexture(const std::string& filename) {
    if (!m_initialized) return nullptr;

    // Check if already loaded
    if (HasTexture(filename)) {
        return m_textures[filename].get();
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Texture file not found: " + fullPath);
        return nullptr;
    }

    try {
        auto texture = std::make_unique<Texture2D>(::LoadTexture(fullPath.c_str()));

        if (texture->id == 0) {
            Logger::LogError("Failed to load texture: " + fullPath);
            return nullptr;
        }

        Texture2D* result = texture.get();
        m_textures[filename] = std::move(texture);
        return result;
    }
    catch (const std::exception& e) {
        Logger::LogError("Exception loading texture " + filename + ": " + std::string(e.what()));
        return nullptr;
    }
}

void AssetManager::UnloadTexture(const std::string& filename) {
    auto it = m_textures.find(filename);
    if (it != m_textures.end()) {
        ::UnloadTexture(*it->second);
        m_textures.erase(it);
    }
}

Model* AssetManager::LoadModel(const std::string& filename) {
    if (!m_initialized) return nullptr;

    // Check if already loaded
    if (HasModel(filename)) {
        return m_models[filename].get();
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Model file not found: " + fullPath);
        return nullptr;
    }

    try {
        auto model = std::make_unique<Model>(::LoadModel(fullPath.c_str()));

        if (model->meshCount == 0) {
            Logger::LogError("Failed to load model: " + fullPath);
            return nullptr;
        }

        Model* result = model.get();
        m_models[filename] = std::move(model);
        return result;
    }
    catch (const std::exception& e) {
        Logger::LogError("Exception loading model " + filename + ": " + std::string(e.what()));
        return nullptr;
    }
}

void AssetManager::UnloadModel(const std::string& filename) {
    auto it = m_models.find(filename);
    if (it != m_models.end()) {
        ::UnloadModel(*it->second);
        m_models.erase(it);
    }
}

Font* AssetManager::LoadFont(const std::string& filename) {
    if (!m_initialized) return nullptr;

    // Check if already loaded
    if (HasFont(filename)) {
        return m_fonts[filename].get();
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Font file not found: " + fullPath);
        return nullptr;
    }

    try {
        auto font = std::make_unique<Font>(::LoadFont(fullPath.c_str()));

        if (font->texture.id == 0) {
            Logger::LogError("Failed to load font: " + fullPath);
            return nullptr;
        }

        Font* result = font.get();
        m_fonts[filename] = std::move(font);
        return result;
    }
    catch (const std::exception& e) {
        Logger::LogError("Exception loading font " + filename + ": " + std::string(e.what()));
        return nullptr;
    }
}

void AssetManager::UnloadFont(const std::string& filename) {
    auto it = m_fonts.find(filename);
    if (it != m_fonts.end()) {
        ::UnloadFont(*it->second);
        m_fonts.erase(it);
    }
}

bool AssetManager::HasTexture(const std::string& filename) const {
    return m_textures.find(filename) != m_textures.end();
}

bool AssetManager::HasModel(const std::string& filename) const {
    return m_models.find(filename) != m_models.end();
}

bool AssetManager::HasFont(const std::string& filename) const {
    return m_fonts.find(filename) != m_fonts.end();
}

void AssetManager::Clear() {
    // Unload all textures
    for (auto& pair : m_textures) {
        ::UnloadTexture(*pair.second);
    }
    m_textures.clear();

    // Unload all models
    for (auto& pair : m_models) {
        ::UnloadModel(*pair.second);
    }
    m_models.clear();

    // Unload all fonts
    for (auto& pair : m_fonts) {
        ::UnloadFont(*pair.second);
    }
    m_fonts.clear();
}

std::string AssetManager::GetFullPath(const std::string& filename) const {
    return std::string(ASSETS_PATH) + filename;
}
