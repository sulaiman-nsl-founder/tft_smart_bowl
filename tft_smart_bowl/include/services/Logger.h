#ifndef SERVICES_LOGGER_H
#define SERVICES_LOGGER_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

namespace Services {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static Logger& getInstance();

    void setMinLevel(LogLevel level);
    LogLevel getMinLevel() const;

    void log(LogLevel level, const char* module, uint32_t eventCode, const char* format, ...);
    void logArgs(LogLevel level, const char* module, uint32_t eventCode, const char* format, va_list args);

    // Register a substring as a secret to be redacted
    void registerSecret(const char* secret);
    void clearSecrets();
    void redact(char* buffer);

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel _minLevel;
    
    // Simple fixed size array to store registered secrets
    static constexpr size_t MAX_SECRETS = 8;
    static constexpr size_t MAX_SECRET_LEN = 64;
    char _secrets[MAX_SECRETS][MAX_SECRET_LEN];
    size_t _secretsCount;
};

} // namespace Services

// Convenience logging macros
#define LOG_DEBUG(mod, code, fmt, ...) Services::Logger::getInstance().log(Services::LogLevel::Debug, mod, code, fmt, ##__VA_ARGS__)
#define LOG_INFO(mod, code, fmt, ...) Services::Logger::getInstance().log(Services::LogLevel::Info, mod, code, fmt, ##__VA_ARGS__)
#define LOG_WARN(mod, code, fmt, ...) Services::Logger::getInstance().log(Services::LogLevel::Warning, mod, code, fmt, ##__VA_ARGS__)
#define LOG_ERROR(mod, code, fmt, ...) Services::Logger::getInstance().log(Services::LogLevel::Error, mod, code, fmt, ##__VA_ARGS__)
#define LOG_CRIT(mod, code, fmt, ...) Services::Logger::getInstance().log(Services::LogLevel::Critical, mod, code, fmt, ##__VA_ARGS__)

#endif // SERVICES_LOGGER_H
