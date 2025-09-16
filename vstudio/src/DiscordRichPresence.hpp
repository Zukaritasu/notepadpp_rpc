#pragma once

#include <windows.h>
#include <string>
#include <cstdint>
#include <string_view>

typedef void (*Exception)(const std::string& error);

struct Presence {
    std::string state;
    std::string details;
    std::string largeImage;
    std::string largeText;
    std::string smallImage;
    std::string smallText;
    int64_t startTime;
    int64_t endTime;
    
    Presence() : startTime(0), endTime(0) {}
};

struct DiscordIPCHeader {
    uint32_t opcode;
    uint32_t length;
};

class DiscordRichPresence {
private:
    static constexpr size_t MAX_MESSAGE_SIZE = 16384;
    static constexpr int MAX_PIPE_ATTEMPTS = 10;
    static constexpr int PING_INTERVAL = 1800;
    static constexpr int MIN_STRING_LENGTH = 2;
    
    HANDLE m_pipe;
    bool m_connected;
    int m_pingCounter;
    
    bool sendDiscordMessageSync(uint32_t opcode, const std::string& json, Exception exc) const;
    bool connectToDiscord(__int64 clientId, Exception exc);
    void disconnect();
    std::string generateNonce() const;
    std::string escapeJsonString(const std::string& str) const;
    
public:

    /**
     * @brief Default constructor for DiscordRichPresence
     * @details Initializes the instance with default values
     */
    DiscordRichPresence() noexcept;
    
    /**
     * @brief Destructor for DiscordRichPresence
     * @details Closes the connection and releases resources
     */
    ~DiscordRichPresence();
    
    // Disable copy constructor and assignment operator
    DiscordRichPresence(const DiscordRichPresence&) = delete;
    DiscordRichPresence& operator=(const DiscordRichPresence&) = delete;
    
    /**
     * @brief Updates the presence in Discord
     * @param exc Exception callback function (optional)
     * @details Sends the currently configured presence to Discord
     */
    void Update(Exception exc = nullptr) noexcept;
    
    /**
     * @brief Connects to Discord using the client ID
     * @param clientId Discord application ID
     * @param exc Exception callback function (optional)
     * @return true if connection was successful, false otherwise
     */
    bool Connect(__int64 clientId, Exception exc = nullptr) noexcept;
    
    /**
     * @brief Closes the connection with Discord
     * @param exc Exception callback function (optional)
     * @details Disconnects and cleans up the IPC connection with Discord
     */
    void Close(Exception exc = nullptr) noexcept;
    
    /**
     * @brief Sets the presence information
     * @param presence Structure containing the presence data to display
     * @param exc Exception callback function (optional)
     * @return true if set successfully, false otherwise
     */
    bool SetPresence(const Presence& presence, Exception exc = nullptr) noexcept;
    
    /**
     * @brief Checks if connected to Discord
     * @return true if connected, false otherwise
     */
    bool IsConnected() const noexcept { return m_connected; }
    
    /**
     * @brief Verifies the connection status with Discord
     * @param exc Exception callback function (optional)
     * @return true if connection is active, false otherwise
     */
    bool CheckConnection(Exception exc = nullptr) const noexcept;
};
