// Copyright (C) 2025 Zukaritasu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <windows.h>
#include <string>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <functional>
#include <atomic>
#include <cstdint>
#include <memory>
#include "PluginThread.h"

typedef std::function<void(const std::string&)> Exception;

struct Presence {
    std::string state;
    std::string details;
    std::string largeImage;
    std::string largeText;
    std::string smallImage;
    std::string smallText;
	std::string repositoryUrl;
    int64_t startTime;
    int64_t endTime;
	bool enableButtonRepository;
    
    Presence() : startTime(0), endTime(0), enableButtonRepository(false) {}

    bool compare(const Presence& other) const {
        return state == other.state &&
               details == other.details &&
               largeImage == other.largeImage &&
               largeText == other.largeText &&
               smallImage == other.smallImage &&
               smallText == other.smallText &&
               repositoryUrl == other.repositoryUrl &&
               startTime == other.startTime &&
               endTime == other.endTime &&
               enableButtonRepository == other.enableButtonRepository;
	}
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
    
    BasicMutex m_mutex;
    HANDLE m_pipe;
    bool m_connected;
    int m_pingCounter;
    std::atomic<__int64> m_lastUpdateTime;
	struct Presence m_presence;
    
    bool sendDiscordMessageSync(uint32_t opcode, const std::string& json, Exception exc);
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

    bool SetIdleStatus(const Presence& presence, Exception exc = nullptr) noexcept;
    
    /**
     * @brief Checks if connected to Discord
     * @return true if connected, false otherwise
     */
    bool IsConnected() const noexcept { return m_connected; }


    bool LastUpdateTimeElapsed(int64_t elapsed) const noexcept {
        return (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - m_lastUpdateTime.load()) >= elapsed;
	}
    
    /**
     * @brief Verifies the connection status with Discord
     * @param exc Exception callback function (optional)
     * @return true if connection is active, false otherwise
     */
    bool CheckConnection(Exception exc = nullptr) noexcept;
};
