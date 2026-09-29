#pragma once

class Timer {
public:
    Timer();
    ~Timer() = default;

    void Update();

    float GetDeltaTime() const { return m_deltaTime; }
    float GetTotalTime() const { return m_totalTime; }
    int GetFPS() const { return m_fps; }

    void Reset();

private:
    float m_deltaTime;
    float m_totalTime;
    float m_lastFrameTime;
    int m_fps;
    int m_frameCount;
    float m_fpsTimer;
};