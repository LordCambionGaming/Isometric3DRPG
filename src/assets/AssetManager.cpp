#include "assets/AssetManager.h"
#include "utils/Logger.h"
#include <filesystem>
#include <cmath>
#include <raylib.h>

// ---------------------------------------------------------------------------
// NormalizeMixamoGLBUnits
// I GLB esportati da Blender+Mixamo hanno i vertici in metri ma le ossa in
// centimetri (la scala Blender 0.01 e' applicata all'oggetto armatura invece
// che ai dati). Questo causa che UpdateModelAnimation() deformi il mesh in
// modo errato (scala 100x nelle translazioni delle ossa).
//
// Rileva automaticamente se il modello e' in questa condizione (bindPose root
// bone translation magnitude >> 1.0 m, tipicamente ~110 cm) e normalizza:
//   - model.bindPose[i].translation *= 0.01
//   - anim.framePoses[frame][i].translation *= 0.01  per ogni anim/frame/bone
// ---------------------------------------------------------------------------
static void NormalizeBindPose(Model& model) {
    if (model.boneCount <= 0 || !model.bindPose) return;
    // Controlla la magnitudine della prima bone (root, tipicamente Hips)
    const Vector3& root = model.bindPose[0].translation;
    const float mag = std::sqrt(root.x*root.x + root.y*root.y + root.z*root.z);
    if (mag < 2.0f) return;  // gia' in metri o nessuna traslazione
    const float s = 0.01f;
    TraceLog(LOG_INFO, "[ASSET] Normalizing Mixamo bone translations from cm to m (root mag=%.1f)", mag);
    for (int i = 0; i < model.boneCount; ++i) {
        model.bindPose[i].translation.x *= s;
        model.bindPose[i].translation.y *= s;
        model.bindPose[i].translation.z *= s;
    }
}

static void NormalizeAnimations(ModelAnimation* anims, int count, int boneCount) {
    if (!anims || count <= 0 || boneCount <= 0) return;
    for (int a = 0; a < count; ++a) {
        if (anims[a].frameCount <= 0 || !anims[a].framePoses) continue;
        // Controlla il primo frame del primo bone
        const Vector3& root = anims[a].framePoses[0][0].translation;
        const float mag = std::sqrt(root.x*root.x + root.y*root.y + root.z*root.z);
        if (mag < 2.0f) continue;  // gia' normalizzata
        const float s = 0.01f;
        for (int f = 0; f < anims[a].frameCount; ++f) {
            for (int b = 0; b < anims[a].boneCount; ++b) {
                anims[a].framePoses[f][b].translation.x *= s;
                anims[a].framePoses[f][b].translation.y *= s;
                anims[a].framePoses[f][b].translation.z *= s;
            }
        }
    }
}

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

        // Normalizza unita' Mixamo (cm -> m) per bindPose se necessario
        NormalizeBindPose(*model);

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

ModelAnimation* AssetManager::LoadAnimations(const std::string& filename, int& outCount) {
    outCount = 0;
    if (!m_initialized) return nullptr;

    // Check if already loaded
    auto cached = m_animations.find(filename);
    if (cached != m_animations.end()) {
        outCount = cached->second.count;
        return cached->second.data;
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Animation file not found: " + fullPath);
        return nullptr;
    }

    int count = 0;
    ModelAnimation* animations = ::LoadModelAnimations(fullPath.c_str(), &count);
    if (!animations || count <= 0) {
        Logger::LogError("No animations found in: " + fullPath);
        if (animations) ::UnloadModelAnimations(animations, count);
        return nullptr;
    }

    // Normalizza unita' Mixamo (cm -> m) nelle framePoses se necessario
    NormalizeAnimations(animations, count, count > 0 ? animations[0].boneCount : 0);

    AnimationSet set;
    set.data = animations;
    set.count = count;
    m_animations[filename] = set;
    outCount = count;
    return animations;
}

void AssetManager::UnloadAnimations(const std::string& filename) {
    auto it = m_animations.find(filename);
    if (it != m_animations.end()) {
        ::UnloadModelAnimations(it->second.data, it->second.count);
        m_animations.erase(it);
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

bool AssetManager::HasAnimations(const std::string& filename) const {
    return m_animations.find(filename) != m_animations.end();
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

    // Unload all animation sets
    for (auto& pair : m_animations) {
        ::UnloadModelAnimations(pair.second.data, pair.second.count);
    }
    m_animations.clear();

    // Unload all fonts
    for (auto& pair : m_fonts) {
        ::UnloadFont(*pair.second);
    }
    m_fonts.clear();
}

std::string AssetManager::GetFullPath(const std::string& filename) const {
    return std::string(ASSETS_PATH) + filename;
}