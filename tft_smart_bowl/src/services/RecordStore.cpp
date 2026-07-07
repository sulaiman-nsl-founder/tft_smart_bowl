#include "services/RecordStore.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <SD.h>

namespace Services {

RecordStore& RecordStore::getInstance() {
    static RecordStore instance;
    return instance;
}

void RecordStore::begin() {
    // Nothing special to initialize; SD card mount state is checked dynamically.
}

uint32_t RecordStore::calculateCrc32(const uint8_t* data, size_t length) const {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

Core::Result<void> RecordStore::appendRecord(const char* filename, const uint8_t* data, size_t length) {
    if (!Drivers::SdCard::getInstance().isMounted()) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::DeviceNotFound, "SD Card not mounted"));
    }

    if (length > 0xFFFF) { // Max length due to uint16_t in header
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Record too large"));
    }

    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Failed to open file for append"));
    }

    RecordHeader header;
    header.magic = RECORD_MAGIC;
    header.version = RECORD_VERSION;
    header.length = static_cast<uint16_t>(length);
    header.crc32 = calculateCrc32(data, length);

    size_t headerWritten = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(RecordHeader));
    size_t dataWritten = file.write(data, length);
    
    file.flush();
    file.close();

    if (headerWritten != sizeof(RecordHeader) || dataWritten != length) {
        LOG_ERROR("REC", 801, "Failed to write complete record to %s", filename);
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Incomplete write"));
    }

    return Core::Result<void>();
}

Core::Result<size_t> RecordStore::readAllRecords(const char* filename, void (*callback)(const uint8_t*, size_t)) {
    if (!Drivers::SdCard::getInstance().isMounted()) {
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::DeviceNotFound, "SD Card not mounted"));
    }

    if (!SD.exists(filename)) {
        return 0; // No file = 0 records
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Failed to open file for read"));
    }

    size_t validRecords = 0;
    while (file.available() >= sizeof(RecordHeader)) {
        RecordHeader header;
        if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(RecordHeader)) != sizeof(RecordHeader)) {
            break; // Truncated header
        }

        if (header.magic != RECORD_MAGIC || header.version != RECORD_VERSION) {
            LOG_ERROR("REC", 802, "Invalid magic/version in %s", filename);
            break; // Corrupt file stream
        }

        if (file.available() < header.length) {
            LOG_ERROR("REC", 803, "Truncated payload in %s", filename);
            break; // Truncated payload
        }

        uint8_t* buffer = new uint8_t[header.length];
        if (file.read(buffer, header.length) != header.length) {
            delete[] buffer;
            break; // Read error
        }

        uint32_t crc = calculateCrc32(buffer, header.length);
        if (crc == header.crc32) {
            validRecords++;
            if (callback) {
                callback(buffer, header.length);
            }
        } else {
            LOG_ERROR("REC", 804, "CRC mismatch in %s", filename);
            // In a real robust system, we might search for the next MAGIC bytes. 
            // Here, we just abort reading to prevent spewing bad data.
            delete[] buffer;
            break;
        }

        delete[] buffer;
    }

    file.close();
    return Core::Result<size_t>(validRecords);
}

Core::Result<void> RecordStore::writeAtomic(const char* filename, const uint8_t* data, size_t length) {
    if (!Drivers::SdCard::getInstance().isMounted()) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::DeviceNotFound, "SD Card not mounted"));
    }

    String tmpName = String(filename) + ".tmp";

    // 1. Write to temp file
    File file = SD.open(tmpName, FILE_WRITE);
    if (!file) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Failed to create tmp file"));
    }

    size_t written = file.write(data, length);
    file.flush();
    file.close();

    if (written != length) {
        SD.remove(tmpName);
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Incomplete tmp write"));
    }

    // 2. Atomically rename (remove original first if needed by ESP32 SD library)
    if (SD.exists(filename)) {
        SD.remove(filename);
    }
    
    if (!SD.rename(tmpName, filename)) {
        LOG_ERROR("REC", 805, "Rename failed: %s -> %s", tmpName.c_str(), filename);
        SD.remove(tmpName);
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::Storage, Core::ErrorCode::InvalidState, "Rename failed"));
    }

    return Core::Result<void>();
}

} // namespace Services
