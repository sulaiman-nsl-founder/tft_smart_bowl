#ifndef CORE_ERRORS_H
#define CORE_ERRORS_H

#include <stdint.h>

namespace Core {

enum class ErrorCategory : uint8_t {
    Success = 0,
    System,
    I2C,
    SPI,
    Sensor,
    Storage,
    Network,
    Protocol,
    Security
};

enum class ErrorCode : uint32_t {
    None = 0,
    
    // System errors
    Unknown = 1,
    Timeout,
    InvalidState,
    InvalidArgument,
    NoMemory,
    NotImplemented,
    
    // Peripheral/Bus errors
    BusInitFailed,
    BusCollision,
    DeviceNotFound,
    DeviceReadFailed,
    DeviceWriteFailed,
    DeviceBusy,
    
    // Sensor weight/cal errors
    CalCorrupted,
    CalInvalid,
    SensorDrift,
    SensorUnstable,
    
    // Storage errors
    CardMountFailed,
    FileNotFound,
    FileWriteFailed,
    FileReadFailed,
    StorageFull,
    ChecksumMismatch,
    
    // Network/MQTT errors
    ConnectionLost,
    DnsLookupFailed,
    TlsHandshakeFailed,
    PublishFailed,
    SubFailed,
    OversizedPayload,
    
    // App level
    SessionActive,
    SessionInactive,
    MenuInvalid
};

struct Error {
    ErrorCategory category;
    ErrorCode code;
    const char* message;

    constexpr Error() : category(ErrorCategory::Success), code(ErrorCode::None), message("Success") {}
    constexpr Error(ErrorCategory cat, ErrorCode cod, const char* msg = "") : category(cat), code(cod), message(msg) {}

    constexpr bool isSuccess() const { return category == ErrorCategory::Success; }
    constexpr explicit operator bool() const { return category != ErrorCategory::Success; }
};

inline constexpr Error Success() {
    return Error(ErrorCategory::Success, ErrorCode::None, "Success");
}

} // namespace Core

#endif // CORE_ERRORS_H
