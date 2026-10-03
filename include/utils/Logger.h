#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <mutex>

class Logger {
public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    static void Initialize(const std::string& logPath = "logs/game.log");
    static void Shutdown();

    static void Log(LogLevel level, const std::string& message);
    static void LogDebug(const std::string& message);
    static void LogInfo(const std::string& message);
    static void LogWarning(const std::string& message);
    static void LogError(const std::string& message);
    static void LogFatal(const std::string& message);

private:
    static std::mutex s_mutex;
    static std::ofstream s_logFile;
    static bool s_initialized;
};
