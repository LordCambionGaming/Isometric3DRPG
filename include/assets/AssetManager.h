#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <memory>

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    bool Initialize();
    void Shutdown();

    // Texture management
    Texture2D* LoadTexture(const std::string& filename);
    void UnloadTexture(const std::string& filename);

    // Model management
    Model* LoadModel(const std::string& filename);
    void UnloadModel(const std::string& filename);

    // Skeletal animation management. The ModelAnimation array is owned by the AssetManager and stays
    // valid until UnloadAnimations()/Clear()/Shutdown(). Loaded once per file, then served from cache.
    ModelAnimation* LoadAnimations(const std::string& filename, int& outCount);
    void UnloadAnimations(const std::string& filename);

    // Font management
    Font* LoadFont(const std::string& filename);
    void UnloadFont(const std::string& filename);

    // Asset existence check
    bool HasTexture(const std::string& filename) const;
    bool HasModel(const std::string& filename) const;
    bool HasAnimations(const std::string& filename) const;
    bool HasFont(const std::string& filename) const;

    // Clear all assets
    void Clear();

private:
    std::string GetFullPath(const std::string& filename) const;

    std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;

    struct AnimationSet {
        ModelAnimation* data = nullptr;
        int count = 0;
    };
    std::unordered_map<std::string, AnimationSet> m_animations;
    std::unordered_map<std::string, std::unique_ptr<Font>> m_fonts;

    bool m_initialized;
    static constexpr const char* ASSETS_PATH = "assets/";
};