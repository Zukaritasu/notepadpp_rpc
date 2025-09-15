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
    HANDLE m_pipe;
    bool m_connected;
    int m_pingCounter;
    
    bool sendDiscordMessageSync(uint32_t opcode, const std::string& json, Exception exc) const;
    bool connectToDiscord(__int64 clientId, Exception exc);
    void disconnect();
    std::string generateNonce() const;
    
public:

    DiscordRichPresence() noexcept;
    ~DiscordRichPresence();
    
    void Update(Exception exc = nullptr) noexcept;
    bool Connect(__int64 clientId, Exception exc = nullptr) noexcept;
    void Close(Exception exc = nullptr) noexcept;
    bool SetPresence(const Presence& presence, Exception exc = nullptr) noexcept;
    bool IsConnected() const noexcept { return m_connected; }
    bool CheckConnection(Exception exc = nullptr) const noexcept;
};
