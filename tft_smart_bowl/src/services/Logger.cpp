#include "services/Logger.h"
#include <stdio.h>
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif

namespace Services {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : _minLevel(LogLevel::Debug), _secretsCount(0) {
    memset(_secrets, 0, sizeof(_secrets));
}

void Logger::setMinLevel(LogLevel level) {
    _minLevel = level;
}

LogLevel Logger::getMinLevel() const {
    return _minLevel;
}

void Logger::registerSecret(const char* secret) {
    if (!secret || strlen(secret) == 0 || _secretsCount >= MAX_SECRETS) {
        return;
    }
    // Check if already registered
    for (size_t i = 0; i < _secretsCount; ++i) {
        if (strcmp(_secrets[i], secret) == 0) {
            return;
        }
    }
    strncpy(_secrets[_secretsCount], secret, MAX_SECRET_LEN - 1);
    _secrets[_secretsCount][MAX_SECRET_LEN - 1] = '\0';
    _secretsCount++;
}

void Logger::clearSecrets() {
    memset(_secrets, 0, sizeof(_secrets));
    _secretsCount = 0;
}

void Logger::redact(char* buffer) {
    if (_secretsCount == 0 || !buffer) return;

    for (size_t i = 0; i < _secretsCount; ++i) {
        const char* secret = _secrets[i];
        size_t secretLen = strlen(secret);
        if (secretLen == 0) continue;

        char* ptr = buffer;
        while ((ptr = strstr(ptr, secret)) != nullptr) {
            // Overwrite the secret with asterisks
            for (size_t j = 0; j < secretLen; ++j) {
                ptr[j] = '*';
            }
            ptr += secretLen;
        }
    }
}

static const char* getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT";
        default:                 return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const char* module, uint32_t eventCode, const char* format, ...) {
    if (level < _minLevel) return;

    va_list args;
    va_start(args, format);
    logArgs(level, module, eventCode, format, args);
    va_end(args);
}

void Logger::logArgs(LogLevel level, const char* module, uint32_t eventCode, const char* format, va_list args) {
    if (level < _minLevel) return;

    // Get timestamp (ms since boot)
    uint32_t timestamp = 0;
#ifndef NATIVE_TEST
    timestamp = millis();
#endif

    // Format context/message
    char msgBuffer[256];
    vsnprintf(msgBuffer, sizeof(msgBuffer), format, args);
    msgBuffer[sizeof(msgBuffer) - 1] = '\0';

    // Redact any registered secrets
    redact(msgBuffer);

    // Format full log output
    char logBuffer[384];
    snprintf(logBuffer, sizeof(logBuffer), "[%u] [%s] [%s] [Code:%u] %s",
             timestamp, getLevelString(level), module ? module : "SYS", eventCode, msgBuffer);
    logBuffer[sizeof(logBuffer) - 1] = '\0';

#ifndef NATIVE_TEST
    Serial.println(logBuffer);
#else
    printf("%s\n", logBuffer);
#endif
}

} // namespace Services
