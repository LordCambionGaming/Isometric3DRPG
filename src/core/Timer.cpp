#include "core/Timer.h"
#include <raylib.h>

Timer::Timer()
    : m_deltaTime(0.0f)
    , m_totalTime(0.0f)
    , m_lastFrameTime(0.0f)
    , m_fps(0)
    , m_frameCount(0)
    , m_fpsTimer(0.0f)
{
    Reset();
}

void Timer::Update() {
    float currentTime = GetTime();
    m_deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;
    m_totalTime = currentTime;

    // Calculate FPS
    m_frameCount++;
    m_fpsTimer += m_deltaTime;

    if (m_fpsTimer >= 1.0f) {
        m_fps = m_frameCount;
        m_frameCount = 0;
        m_fpsTimer = 0.0f;
    }
}

void Timer::Reset() {
    m_deltaTime = 0.0f;
    m_totalTime = 0.0f;
    m_lastFrameTime = GetTime();
    m_fps = 0;
    m_frameCount = 0;
    m_fpsTimer = 0.0f;
}