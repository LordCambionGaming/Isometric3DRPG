
#include "utils/Logger.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>


std::mutex Logger::s_mutex;
std::ofstream Logger::s_logFile;
bool Logger::s_initialized = false;

void Logger::Initialize(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized) return;
    
    // Create logs directory if it doesn't exist
    try {
        std::filesystem::create_directories("logs");
    } catch (...) {
        // Ignore errors creating directory
    }
    
    s_logFile.open(logPath, std::ios::app);
    if (s_logFile.is_open()) {
        s_initialized = true;
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_logFile.is_open()) {
        s_logFile.close();
    }
    s_initialized = false;
}

static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_now;
    #if defined(_WIN32) || defined(_WIN64)
    // Versione Windows (MSVC): i parametri sono invertiti rispetto a POSIX
    gmtime_s(&tm_now, &time_t_now);
#else
    // Versione Linux / macOS / POSIX
    gmtime_r(&time_t_now, &tm_now);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

static std::string GetLevelString(Logger::LogLevel level) {
    switch (level) {
        case Logger::LogLevel::Debug: return "[DEBUG]";
        case Logger::LogLevel::Info:  return "[INFO]";
        case Logger::LogLevel::Warning: return "[WARNING]";
        case Logger::LogLevel::Error:  return "[ERROR]";
        case Logger::LogLevel::Fatal:  return "[FATAL]";
        default: return "";
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (!s_initialized || !s_logFile.is_open()) return;
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    s_logFile << GetTimestamp() << " " << GetLevelString(level) << " " << message << "\n";
}

void Logger::LogDebug(const std::string& message) {
    Log(LogLevel::Debug, message);
}

void Logger::LogInfo(const std::string& message) {
    Log(LogLevel::Info, message);
}

void Logger::LogWarning(const std::string& message) {
    Log(LogLevel::Warning, message);
}

void Logger::LogError(const std::string& message) {
    Log(LogLevel::Error, message);
}

void Logger::LogFatal(const std::string& message) {
    Log(LogLevel::Fatal, message);
}
