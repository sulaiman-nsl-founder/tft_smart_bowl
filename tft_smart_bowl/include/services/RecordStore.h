#ifndef SERVICES_RECORD_STORE_H
#define SERVICES_RECORD_STORE_H

#include <stdint.h>
#include <stddef.h>
#include "core/Result.h"
#include "drivers/SdCard.h"

namespace Services {

/**
 * @brief Service for writing atomic records to the SD card.
 * 
 * Uses a temp file + rename pattern to ensure partial writes
 * don't corrupt the data file in the event of sudden power loss.
 */
class RecordStore {
public:
    static RecordStore& getInstance();

    /**
     * @brief Initialize the record store (check SD card).
     */
    void begin();

    /**
     * @brief Atomically append a record to a file.
     * 
     * @param filename Base filename (e.g. "/queue.dat")
     * @param data Pointer to data buffer.
     * @param length Length of data in bytes.
     * @return Success or error code.
     */
    Core::Result<void> appendRecord(const char* filename, const uint8_t* data, size_t length);

    /**
     * @brief Read all records from a file sequentially.
     * 
     * @param filename File to read.
     * @param callback Function called for each valid record (pointer to data, length).
     * @return Number of valid records read, or error.
     */
    Core::Result<size_t> readAllRecords(const char* filename, void (*callback)(const uint8_t*, size_t));

    /**
     * @brief Atomically replace a file's contents with a single record/buffer.
     */
    Core::Result<void> writeAtomic(const char* filename, const uint8_t* data, size_t length);

private:
    RecordStore() = default;
    ~RecordStore() = default;
    RecordStore(const RecordStore&) = delete;
    RecordStore& operator=(const RecordStore&) = delete;

    uint32_t calculateCrc32(const uint8_t* data, size_t length) const;

    // Standard versioned record header
    struct RecordHeader {
        uint32_t magic;      // 0x5245434F ("RECO")
        uint16_t version;    // 1
        uint16_t length;     // Data length
        uint32_t crc32;      // CRC32 of the data payload
    } __attribute__((packed));

    static constexpr uint32_t RECORD_MAGIC = 0x5245434F;
    static constexpr uint16_t RECORD_VERSION = 1;
};

} // namespace Services

#endif // SERVICES_RECORD_STORE_H
