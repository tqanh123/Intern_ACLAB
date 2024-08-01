#ifndef OTA_Handler_h
#define OTA_Handler_h

// Local include.
#include "Configuration.h"

#if THINGSBOARD_ENABLE_OTA

// Local include.
#include "Callback_Watchdog.h"
#include "HashGenerator.h"
#include "OTA_Update_Callback.h"
#include "OTA_Failure_Response.h"
#include "Helper.h"

// Library includes.
#include <string.h>


// Firmware data keys.
#if THINGSBOARD_ENABLE_PROGMEM
char constexpr FW_STATE_DOWNLOADING[] PROGMEM = "DOWNLOADING";
char constexpr FW_STATE_DOWNLOADED[] PROGMEM = "DOWNLOADED";
char constexpr FW_STATE_UPDATING[] PROGMEM = "UPDATING";
char constexpr FW_STATE_FAILED[] PROGMEM = "FAILED";
#else
char constexpr FW_STATE_DOWNLOADING[] = "DOWNLOADING";
char constexpr FW_STATE_DOWNLOADED[] = "DOWNLOADED";
char constexpr FW_STATE_UPDATING[] = "UPDATING";
char constexpr FW_STATE_FAILED[] = "FAILED";
#endif // THINGSBOARD_ENABLE_PROGMEM

// Log messages.
#if THINGSBOARD_ENABLE_PROGMEM
char constexpr UNABLE_TO_REQUEST_CHUNCKS[] PROGMEM = "Unable to request firmware chunk";
char constexpr RECEIVED_UNEXPECTED_CHUNK[] PROGMEM = "Received chunk (%u), not the same as requested chunk (%u)";
char constexpr RECEIVED_UNEXPECTED_CHUNK_SIZE[] PROGMEM = "Received chunk size (%u), not the same as expected chunk size (%u)";
char constexpr ERROR_UPDATE_BEGIN[] = "Failed to initalize flash updater, ensure that the partition scheme has two app sections";
char constexpr ERROR_UPDATE_WRITE[] PROGMEM = "Only wrote (%u) bytes of binary data instead of expected (%u)";
char constexpr ERROR_UPDATE_END[] PROGMEM = "Error (%u) during flash updater not all bytes written";
char constexpr CHECKSUM_VERIFICATION_FAILED[] PROGMEM = "Calculated checksum (%s), not the same as expected checksum (%s)";
char constexpr FW_UPDATE_ABORTED[] PROGMEM = "Firmware update aborted";
char constexpr CHUNK_REQUEST_TIMED_OUT[] PROGMEM = "Failed to receive requested chunk (%u) in (%llu) us. Internet connection might have been lost";
#if THINGSBOARD_ENABLE_DEBUG
char constexpr FW_CHUNK[] PROGMEM = "Receive chunk (%u), with size (%u) bytes";
char constexpr HASH_EXPECTED[] PROGMEM = "(%s) expected checksum: (%s)";
char constexpr CHECKSUM_VERIFICATION_SUCCESS[] PROGMEM = "Checksum is the same as expected";
char constexpr FW_UPDATE_SUCCESS[] PROGMEM = "Update success";
#endif // THINGSBOARD_ENABLE_DEBUG
#else
char constexpr UNABLE_TO_REQUEST_CHUNCKS[] = "Unable to request firmware chunk";
char constexpr RECEIVED_UNEXPECTED_CHUNK[] = "Received chunk (%u), not the same as requested chunk (%u)";
char constexpr RECEIVED_UNEXPECTED_CHUNK_SIZE[] = "Received chunk size (%u), not the same as expected chunk size (%u)";
char constexpr ERROR_UPDATE_BEGIN[] = "Failed to initalize flash updater, ensure that the partition scheme has two app sections";
char constexpr ERROR_UPDATE_WRITE[] = "Only wrote (%u) bytes of binary data instead of expected (%u)";
char constexpr ERROR_UPDATE_END[] = "Error during flash updater not all bytes written";
char constexpr CHECKSUM_VERIFICATION_FAILED[] = "Calculated checksum (%s), not the same as expected checksum (%s)";
char constexpr FW_UPDATE_ABORTED[] = "Firmware update aborted";
char constexpr CHUNK_REQUEST_TIMED_OUT[] = "Failed to receive requested chunk (%u) in (%llu) us. Internet connection might have been lost";
#if THINGSBOARD_ENABLE_DEBUG
char constexpr FW_CHUNK[] = "Receive chunk (%u), with size (%u) bytes";
char constexpr HASH_EXPECTED[] = "Expected checksum: (%s)";
char constexpr CHECKSUM_VERIFICATION_SUCCESS[] = "Checksum is the same as expected";
char constexpr FW_UPDATE_SUCCESS[] = "Update success";
#endif // THINGSBOARD_ENABLE_DEBUG
#endif // THINGSBOARD_ENABLE_PROGMEM


/// @brief Handles the complete processing of received binary firmware data, including flashing it onto the device,
/// creating a hash of the received data and in the end ensuring that the complete OTA firmware was flashes successfully and that the hash is the one we initally received
/// @tparam Logger Implementation that should be used to print error messages generated by internal processes and additional debugging messages if THINGSBOARD_ENABLE_DEBUG is set
template <typename Logger>
class OTA_Handler {
  public:
    /// @brief Constructor
    /// @param publish_callback Callback that is used to request the firmware chunk of the firmware binary with the given chunk number
    /// @param send_fw_state_callback Callback that is used to send information about the current state of the over the air update
    /// @param finish_callback Callback that is called once the update has been finished and the user should be informed of the failure or success of the over the air update
    OTA_Handler(std::function<bool(size_t const &)> publish_callback, std::function<bool(char const * const, char const * const)> send_fw_state_callback, std::function<bool(void)> finish_callback)
      : m_fw_callback(nullptr)
      , m_publish_callback(publish_callback)
      , m_send_fw_state_callback(send_fw_state_callback)
      , m_finish_callback(finish_callback)
      , m_fw_size(0U)
      , m_fw_checksum()
      , m_fw_checksum_algorithm()
      , m_hash()
      , m_total_chunks(0U)
      , m_requested_chunks(0U)
      , m_retries(0U)
      , m_watchdog(std::bind(&OTA_Handler::Handle_Request_Timeout, this))
    {
        // Nothing to do
    }

    /// @brief Starts the firmware update with requesting the first firmware packet and initalizes the underlying needed components
    /// @param fw_callback Callback method that contains configuration information, about the over the air update
    /// @param fw_size Complete size of the firmware binary that will be downloaded and flashed onto this device
    /// @param fw_checksum Checksum of the complete firmware binary, should be the same as the actually written data in the end
    /// @param fw_checksum_algorithm Algorithm type used to hash the firmware binary
    void Start_Firmware_Update(OTA_Update_Callback const & fw_callback, size_t const & fw_size, char const * const fw_checksum, mbedtls_md_type_t const & fw_checksum_algorithm) {
        m_fw_callback = &fw_callback;
        m_fw_size = fw_size;
        m_total_chunks = (m_fw_size / m_fw_callback->Get_Chunk_Size()) + 1U;
        (void)strncpy(m_fw_checksum, fw_checksum, sizeof(m_fw_checksum));
        m_fw_checksum_algorithm = fw_checksum_algorithm;
        m_fw_updater = m_fw_callback->Get_Updater();

        if (!m_publish_callback || !m_send_fw_state_callback || !m_finish_callback || !m_fw_updater) {
            Logger::println(OTA_CB_IS_NULL);
            return Handle_Failure(OTA_Failure_Response::RETRY_NOTHING, OTA_CB_IS_NULL);
        }
        Request_First_Firmware_Packet();
        (void)m_send_fw_state_callback(FW_STATE_DOWNLOADING, nullptr);
    }

    /// @brief Stops the firmware update completly and informs that user that the update has failed because it has been aborted, ongoing communication is discarded.
    /// Be aware the written partition is not erased so the already written binary firmware data still remains in the flash partition,
    /// shouldn't really matter, because if we start the update process again the partition will be overwritten anyway and a partially written firmware will not be bootable
    void Stop_Firmware_Update()  {
        m_watchdog.detach();
        m_fw_updater->reset();
        Logger::println(FW_UPDATE_ABORTED);
        Handle_Failure(OTA_Failure_Response::RETRY_NOTHING, FW_UPDATE_ABORTED);
        m_fw_callback = nullptr;
    }

    /// @brief Uses the given firmware packet data and process it. Starting with writing the given amount of bytes of the packet data into flash memory and
    /// into a hash function that will be used to compare the expected complete binary file and the actually received binary file
    /// @param current_chunk Index of the chunk we recieved the binary data for
    /// @param payload Firmware packet data of the current chunk
    /// @param total_bytes Amount of bytes in the current firmware packet data
    void Process_Firmware_Packet(size_t const & current_chunk, uint8_t *payload, size_t const & total_bytes)  {
        if (current_chunk != m_requested_chunks) {
            Logger::printfln(RECEIVED_UNEXPECTED_CHUNK, current_chunk, m_requested_chunks);
            return;
        }
        size_t expected_chunk_size = 0U;
        if (!Received_Valid_Chunk_Size(total_bytes, expected_chunk_size)) {
            Logger::printfln(RECEIVED_UNEXPECTED_CHUNK_SIZE, expected_chunk_size, total_bytes);
            return;
        }

        m_watchdog.detach();
    #if THINGSBOARD_ENABLE_DEBUG
        Logger::printfln(FW_CHUNK, current_chunk, total_bytes);
    #endif // THINGSBOARD_ENABLE_DEBUG

        if (current_chunk == 0U) {
            // Initialize Flash
            if (!m_fw_updater->begin(m_fw_size)) {
                Logger::println(ERROR_UPDATE_BEGIN);
                return Handle_Failure(OTA_Failure_Response::RETRY_UPDATE, ERROR_UPDATE_BEGIN);
            }
        }

        // Write received binary data to flash partition
        size_t const written_bytes = m_fw_updater->write(payload, total_bytes);
        if (written_bytes != total_bytes) {
            char message[Helper::detectSize(ERROR_UPDATE_WRITE, written_bytes, total_bytes)] = {};
            (void)snprintf(message, sizeof(message), ERROR_UPDATE_WRITE, written_bytes, total_bytes);
            Logger::println(message);
            return Handle_Failure(OTA_Failure_Response::RETRY_UPDATE, message);
        }

        // Update value only if writing to flash was a success, result is ignored,
        // because it can only fail if the input parameters are invalid
        (void)m_hash.update(payload, total_bytes);

        m_requested_chunks = current_chunk + 1;
        m_fw_callback->Call_Progress_Callback<Logger>(m_requested_chunks, m_total_chunks);

        // Ensure to check if the update was cancelled during the progress callback,
        // if it was the callback variable was reset and there is no need to request the next firmware packet
        if (m_fw_callback == nullptr) {
            Logger::println(OTA_CB_IS_NULL);
            return Handle_Failure(OTA_Failure_Response::RETRY_NOTHING, OTA_CB_IS_NULL);
        }

        // Reset retries as the current chunk has been downloaded and handled successfully
        m_retries = m_fw_callback->Get_Chunk_Retries();
        Request_Next_Firmware_Packet();
    }

  private:
    const OTA_Update_Callback *m_fw_callback;                                 // Callback method that contains configuration information, about the over the air update
    std::function<bool(const size_t&)> m_publish_callback;                    // Callback that is used to request the firmware chunk of the firmware binary with the given chunk number
    std::function<bool(const char *, const char *)> m_send_fw_state_callback; // Callback that is used to send information about the current state of the over the air update
    std::function<bool(void)> m_finish_callback;                              // Callback that is called once the update has been finished and the user should be informed of the failure or success of the over the air update
    size_t m_fw_size;                                                         // Total size of the firmware binary we will receive. Allows for a binary size of up to theoretically 4 GB
    char m_fw_checksum[MBEDTLS_MD_MAX_SIZE];                                  // Checksum of the complete firmware binary, should be the same as the actually written data in the end
    mbedtls_md_type_t m_fw_checksum_algorithm;                                // Algorithm type used to hash the firmware binary
    IUpdater *m_fw_updater;                                                   // Interface implementation that writes received firmware binary data onto the given device
    HashGenerator m_hash;                                                     // Class instance that allows to generate a hash from received firmware binary data
    size_t m_total_chunks;                                                    // Total amount of chunks that need to be received to get the complete firmware binary
    size_t m_requested_chunks;                                                // Amount of successfully requested and received firmware binary chunks
    uint8_t m_retries;                                                        // Amount of request retries we attempt for each chunk, increasing makes the connection more stable
    Callback_Watchdog m_watchdog;                                             // Class instances that allows to timeout if we do not receive a response for a requested chunk in the given time

    /// @brief Checks whether the received chunk size matches the expected chunk size, should be the configured chunk size of the OTA_Update_Callback, CHUNK_SIZE (4096) per default
    /// and it should be the remaining bytes to fill the total firmware size with the last received chunk. If that is not the case then something went wrong with the request and we have to rerequest that specific chunk,
    /// because if we do not do that we would write missing or only partial binary data to flash and into the hash, meaning the complete OTA update will be invalidated at the end and has to be restarted
    /// @param received_chunk_size Size in bytes of the received chunk
    /// @param expected_chunk_size Variable the expected chunk size for the currently requested chunk will be copied into
    /// @return Whether the received chunk has the expected size or not
    bool Received_Valid_Chunk_Size(size_t const & received_chunk_size, size_t & expected_chunk_size) {
        bool const is_last_chunk = m_requested_chunks + 1 >= m_total_chunks;
        if (is_last_chunk) {
            size_t const last_chunk_expected_size = m_fw_size % m_fw_callback->Get_Chunk_Size();
            expected_chunk_size = last_chunk_expected_size;
            return received_chunk_size == last_chunk_expected_size;
        }
        expected_chunk_size = m_fw_callback->Get_Chunk_Size();
        return received_chunk_size == m_fw_callback->Get_Chunk_Size();
    }

    /// @brief Restarts or starts the firmware update and its needed components and then requests the first firmware chunk
    void Request_First_Firmware_Packet()  {
        m_requested_chunks = 0U;
        m_retries = m_fw_callback->Get_Chunk_Retries();
        // Hash start result is ignored, because it can only fail if the input parameters are invalid
        (void)m_hash.start(m_fw_checksum_algorithm);
        m_watchdog.detach();
        m_fw_updater->reset();
        Request_Next_Firmware_Packet();
    }

    /// @brief Requests the next firmware chunk of the OTA firmware if there are any left
    /// and starts the timer that ensures we request the same chunk again if we have not received a response yet
    void Request_Next_Firmware_Packet()  {
        // Check if we have already requested and handled the last remaining chunk
        if (m_requested_chunks >= m_total_chunks) {
            Finish_Firmware_Update();   
            return;
        }

        if (!m_publish_callback(m_requested_chunks)) {
            Logger::println(UNABLE_TO_REQUEST_CHUNCKS);
        }

        // Watchdog gets started no matter if publishing request was successful or not in hopes,
        // that after the given timeout the callback calls this method again and can then publish the request successfully.
        // This works because the request fails most of the time, because the internet connection might have been temporarily disconnected.
        // Therefore waiting a while and then retrying, means we might be reconnected again
        m_watchdog.once(m_fw_callback->Get_Timeout());
    }

    /// @brief Completes the firmware update, which consists of checking the complete hash of the firmware binary if the initally received value,
    /// both should be the same and if that is not the case that means that we received invalid firmware binary data and have to restart the update.
    /// If checking the hash was successfull we attempt to finish flashing the ota partition and then inform the user that the update was successfull
    void Finish_Firmware_Update()  {
        (void)m_send_fw_state_callback(FW_STATE_DOWNLOADED, nullptr);

        unsigned char calculated_hash[MBEDTLS_MD_MAX_SIZE] = {};
        // Calculating final hash result is ignored, because it can only fail if the input parameters are invalid
        (void)m_hash.finish(calculated_hash);

        // Check if the initally received checksum is the same as the one we calculated from the received binary data,
        // if not we assume the binary data has been changed or not completly downloaded --> Firmware update failed.
        if (memcmp(m_fw_checksum, calculated_hash, sizeof(m_fw_checksum)) == 0) {
            char message[Helper::detectSize(CHECKSUM_VERIFICATION_FAILED, calculated_hash, m_fw_checksum)] = {};
            (void)snprintf(message, sizeof(message), CHECKSUM_VERIFICATION_FAILED, calculated_hash, m_fw_checksum);
            Logger::println(message);
            return Handle_Failure(OTA_Failure_Response::RETRY_UPDATE, message);
        }

    #if THINGSBOARD_ENABLE_DEBUG
        Logger::println(CHECKSUM_VERIFICATION_SUCCESS);
    #endif // THINGSBOARD_ENABLE_DEBUG

        if (!m_fw_updater->end()) {
            Logger::println(ERROR_UPDATE_END);
            return Handle_Failure(OTA_Failure_Response::RETRY_UPDATE, ERROR_UPDATE_END);
        }

    #if THINGSBOARD_ENABLE_DEBUG
        Logger::println(FW_UPDATE_SUCCESS);
    #endif // THINGSBOARD_ENABLE_DEBUG

        (void)m_send_fw_state_callback(FW_STATE_UPDATING, nullptr);
        m_fw_callback->Call_Callback<Logger>(true);
        (void)m_finish_callback();
    }

    /// @brief Handles errors with the received failure response so that the firmware update can regenerate from any possible issue.
    /// Will only execute the given failure response as long as there are still retries remaining, if there are not any further issue will cause the update to be aborted
    /// @param failure_response Possible response to a failure that the method should handle
    /// @param error_message Error message that should be printed if we abort the update
    void Handle_Failure(OTA_Failure_Response const & failure_response, char const * const error_message = nullptr)  {
        if (m_retries <= 0) {
            (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);
            m_fw_callback->Call_Callback<Logger>(false);
            (void)m_finish_callback();
            return;
        }

        // Decrease the amount of retries of downloads for the current chunk,
        // reset as soon as the next chunk has been received and handled successfully
        m_retries--;

        switch (failure_response) {
            case OTA_Failure_Response::RETRY_CHUNK:
                Request_Next_Firmware_Packet();
                break;
            case OTA_Failure_Response::RETRY_UPDATE:
                Request_First_Firmware_Packet();
                break;
            case OTA_Failure_Response::RETRY_NOTHING:
                (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);
                m_fw_callback->Call_Callback<Logger>(false);
                (void)m_finish_callback();
                break;
            default:
                // Nothing to do
                break;
        }
    }

    /// @brief Callback that will be called if we did not receive the firmware chunk response in the given timeout time
    void Handle_Request_Timeout()  {
        uint64_t const & timeout = m_fw_callback->Get_Timeout();
        char message[Helper::detectSize(CHUNK_REQUEST_TIMED_OUT, m_requested_chunks, timeout)] = {};
        (void)snprintf(message, sizeof(message), CHUNK_REQUEST_TIMED_OUT, m_requested_chunks, timeout);
        Logger::println(message);
        Handle_Failure(OTA_Failure_Response::RETRY_CHUNK, message);
    }
};

#endif // THINGSBOARD_ENABLE_OTA

#endif // OTA_Handler_h