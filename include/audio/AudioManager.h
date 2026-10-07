#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <memory>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool Initialize();
    void Shutdown();

    // Sound management
    Sound* LoadSound(const std::string& filename);
    void PlaySound(const std::string& filename);
    void UnloadSound(const std::string& filename);

    // Music management
    Music* LoadMusic(const std::string& filename);
    void PlayMusic(const std::string& filename);
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();
    void UpdateMusic();

    // Volume control
    void SetMasterVolume(float volume);
    void SetSoundVolume(float volume);
    void SetMusicVolume(float volume);

    float GetMasterVolume() const { return m_masterVolume; }
    float GetSoundVolume() const { return m_soundVolume; }
    float GetMusicVolume() const { return m_musicVolume; }

    bool IsMusicPlaying() const;

private:
    std::string GetFullPath(const std::string& filename) const;
    void SaveSettings() const;

    std::unordered_map<std::string, std::unique_ptr<Sound>> m_sounds;
    std::unordered_map<std::string, std::unique_ptr<Music>> m_music;

    float m_masterVolume;
    float m_soundVolume;
    float m_musicVolume;

    std::string m_currentMusic;
    bool m_initialized;

    static constexpr const char* AUDIO_PATH = "assets/audio/";
};