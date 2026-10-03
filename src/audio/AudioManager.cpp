#include "audio/AudioManager.h"
#include "utils/Logger.h"
#include <filesystem>
#include <raymath.h>

AudioManager::AudioManager()
    : m_masterVolume(1.0f)
    , m_soundVolume(1.0f)
    , m_musicVolume(1.0f)
    , m_initialized(false)
{
}

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize() {
    try {
        InitAudioDevice();

        if (!IsAudioDeviceReady()) {
            Logger::LogError("Failed to initialize audio device!");
            return false;
        }

        // Create audio directory if it doesn't exist
        std::filesystem::create_directories(AUDIO_PATH);

        SetMasterVolume(m_masterVolume);

        m_initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        Logger::LogError("Failed to initialize AudioManager: " + std::string(e.what()));
        return false;
    }
}

void AudioManager::Shutdown() {
    if (!m_initialized) return;

    // Stop any playing music
    StopMusic();

    // Unload all sounds
    for (auto& pair : m_sounds) {
        ::UnloadSound(*pair.second);
    }
    m_sounds.clear();

    // Unload all music
    for (auto& pair : m_music) {
        ::UnloadMusicStream(*pair.second);
    }
    m_music.clear();

    CloseAudioDevice();
    m_initialized = false;
}

Sound* AudioManager::LoadSound(const std::string& filename) {
    if (!m_initialized) return nullptr;

    // Check if already loaded
    auto it = m_sounds.find(filename);
    if (it != m_sounds.end()) {
        return it->second.get();
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Sound file not found: " + fullPath);
        return nullptr;
    }

    try {
        auto sound = std::make_unique<Sound>(::LoadSound(fullPath.c_str()));

        if (sound->stream.buffer == nullptr) {
            Logger::LogError("Failed to load sound: " + fullPath);
            return nullptr;
        }

        Sound* result = sound.get();
        m_sounds[filename] = std::move(sound);
        return result;
    }
    catch (const std::exception& e) {
        Logger::LogError("Exception loading sound " + filename + ": " + std::string(e.what()));
        return nullptr;
    }
}

void AudioManager::PlaySound(const std::string& filename) {
    Sound* sound = LoadSound(filename);
    if (sound) {
        ::PlaySound(*sound);
    }
}

void AudioManager::UnloadSound(const std::string& filename) {
    auto it = m_sounds.find(filename);
    if (it != m_sounds.end()) {
        ::UnloadSound(*it->second);
        m_sounds.erase(it);
    }
}

Music* AudioManager::LoadMusic(const std::string& filename) {
    if (!m_initialized) return nullptr;

    // Check if already loaded
    auto it = m_music.find(filename);
    if (it != m_music.end()) {
        return it->second.get();
    }

    std::string fullPath = GetFullPath(filename);

    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        Logger::LogError("Music file not found: " + fullPath);
        return nullptr;
    }

    try {
        auto music = std::make_unique<Music>(::LoadMusicStream(fullPath.c_str()));

        if (music->stream.buffer == nullptr) {
            Logger::LogError("Failed to load music: " + fullPath);
            return nullptr;
        }

        Music* result = music.get();
        m_music[filename] = std::move(music);
        return result;
    }
    catch (const std::exception& e) {
        Logger::LogError("Exception loading music " + filename + ": " + std::string(e.what()));
        return nullptr;
    }
}

void AudioManager::PlayMusic(const std::string& filename) {
    // Stop current music if playing
    StopMusic();

    Music* music = LoadMusic(filename);
    if (music) {
        ::PlayMusicStream(*music);
        m_currentMusic = filename;
    }
}

void AudioManager::StopMusic() {
    if (!m_currentMusic.empty()) {
        auto it = m_music.find(m_currentMusic);
        if (it != m_music.end()) {
            ::StopMusicStream(*it->second);
        }
        m_currentMusic.clear();
    }
}

void AudioManager::PauseMusic() {
    if (!m_currentMusic.empty()) {
        auto it = m_music.find(m_currentMusic);
        if (it != m_music.end()) {
            ::PauseMusicStream(*it->second);
        }
    }
}

void AudioManager::ResumeMusic() {
    if (!m_currentMusic.empty()) {
        auto it = m_music.find(m_currentMusic);
        if (it != m_music.end()) {
            ::ResumeMusicStream(*it->second);
        }
    }
}

void AudioManager::UpdateMusic() {
    if (!m_currentMusic.empty()) {
        auto it = m_music.find(m_currentMusic);
        if (it != m_music.end()) {
            ::UpdateMusicStream(*it->second);
        }
    }
}

void AudioManager::SetMasterVolume(float volume) {
    m_masterVolume = Clamp(volume, 0.0f, 1.0f);
    ::SetMasterVolume(m_masterVolume);
}

void AudioManager::SetSoundVolume(float volume) {
    m_soundVolume = Clamp(volume, 0.0f, 1.0f);
    // Individual sound volumes would be set when playing
}

void AudioManager::SetMusicVolume(float volume) {
    m_musicVolume = Clamp(volume, 0.0f, 1.0f);

    if (!m_currentMusic.empty()) {
        auto it = m_music.find(m_currentMusic);
        if (it != m_music.end()) {
            ::SetMusicVolume(*it->second, m_musicVolume);
        }
    }
}

bool AudioManager::IsMusicPlaying() const {
    if (m_currentMusic.empty()) return false;

    auto it = m_music.find(m_currentMusic);
    if (it != m_music.end()) {
        return ::IsMusicStreamPlaying(*it->second);
    }

    return false;
}

std::string AudioManager::GetFullPath(const std::string& filename) const {
    return std::string(AUDIO_PATH) + filename;
}
