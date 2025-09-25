#include "DiscordRichPresence.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

DiscordRichPresence::DiscordRichPresence()  noexcept
    : m_pipe(INVALID_HANDLE_VALUE), m_connected(false), m_pingCounter(0) { }

DiscordRichPresence::~DiscordRichPresence() {
    Close();
}

std::string DiscordRichPresence::generateNonce() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    return std::to_string(dis(gen));
}

std::string DiscordRichPresence::escapeJsonString(const std::string& str) const {
    std::string escaped;
    escaped.reserve(str.size() + 10); // Reserve extra space for escape characters
    
    for (char c : str) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c >= 0 && c < 32) {
                    // Control characters
                    escaped += "\\u";
                    escaped += "0000";
                    escaped[escaped.size() - 4] = '0' + ((c >> 12) & 15);
                    escaped[escaped.size() - 3] = '0' + ((c >> 8) & 15);
                    escaped[escaped.size() - 2] = '0' + ((c >> 4) & 15);
                    escaped[escaped.size() - 1] = '0' + (c & 15);
                } else {
                    escaped += c;
                }
                break;
        }
    }
    return escaped;
}

bool DiscordRichPresence::connectToDiscord(__int64 clientId, Exception exc) {
    for (int i = 0; i < MAX_PIPE_ATTEMPTS; i++) {
        std::string pipeName = R"(\\?\pipe\discord-ipc-)" + std::to_string(i);
        m_pipe = ::CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        
        if (m_pipe != INVALID_HANDLE_VALUE) {
            std::string handshake = R"({"v":1,"client_id":")" + std::to_string(clientId) + R"("})";
            if (sendDiscordMessageSync(0, handshake, exc))
                return true;
            ::CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
    }
    if (exc) exc("Could not connect to Discord. Is Discord running?");
    return false;
}


bool DiscordRichPresence::sendDiscordMessageSync(uint32_t opcode, const std::string& json, Exception exc) const {
    if (m_pipe == INVALID_HANDLE_VALUE) {
		if (exc) exc("Pipe is not valid");
        return false;
    }
    
    DWORD bytesAvailable;
    if (!::PeekNamedPipe(m_pipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
		if (exc) exc("Lost connection to Discord");
        return false;
    }

    if (json.size() > MAX_MESSAGE_SIZE) {
		if (exc) exc("Message size exceeds maximum limit");
        return false;
    }
    
    DiscordIPCHeader header{};
    header.opcode = opcode;
    header.length = static_cast<uint32_t>(json.size());
    
    DWORD written;
    if (!::WriteFile(m_pipe, &header, sizeof(header), &written, NULL) || 
        written != sizeof(header)) {
		if (exc) exc("Failed to write message header to pipe");
        return false;
    }
    
    if (!::WriteFile(m_pipe, json.c_str(), static_cast<DWORD>(json.size()), &written, NULL) || 
        written != json.size()) {
		if (exc) exc("Failed to write message body to pipe");
        return false;
    }
    
    DiscordIPCHeader responseHeader{};
    DWORD bytesRead;
    
    if (!::ReadFile(m_pipe, &responseHeader, sizeof(responseHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(responseHeader)) {
		if (exc) exc("Failed to read response header from pipe");
        return false;
    }
    
    if (responseHeader.length > MAX_MESSAGE_SIZE) {
		if (exc) exc("Response size exceeds maximum limit");
        return false;
    }
    
    if (responseHeader.length > 0) {
        std::string response;
        response.resize(responseHeader.length);
        
        if (!::ReadFile(m_pipe, response.data(), responseHeader.length, &bytesRead, NULL) ||
            bytesRead != responseHeader.length) {
			if (exc) exc("Failed to read response body from pipe");
            return false;
        }

        std::string_view responseView(response);
        
        if (responseView.find("\"evt\":\"READY\"") != std::string_view::npos || 
            responseView.find("\"cmd\":\"SET_ACTIVITY\"") != std::string_view::npos ||
            responseView.find("\"code\":0") != std::string_view::npos ||
            responseView.find("\"data\"") != std::string_view::npos) {
            return true;
        }
        
        if (responseView.find("\"evt\":\"ERROR\"") != std::string_view::npos || 
            responseView.find("\"code\":") != std::string_view::npos) {
            
            std::string errorCode = "Unknown";
            std::string errorMessage = "Discord Error";
            
            size_t codePos = responseView.find("\"code\":");
            if (codePos != std::string_view::npos) {
                size_t codeStart = codePos + 7;
                size_t codeEnd = responseView.find_first_of(",}", codeStart);
                if (codeEnd != std::string_view::npos) {
                    errorCode = std::string(responseView.substr(codeStart, codeEnd - codeStart));
                }
            }
            
            size_t messagePos = responseView.find("\"message\":");
            if (messagePos != std::string_view::npos) {
                size_t msgStart = messagePos + 10;
                size_t quoteStart = responseView.find("\"", msgStart);
                if (quoteStart != std::string_view::npos) {
                    size_t quoteEnd = responseView.find("\"", quoteStart + 1);
                    if (quoteEnd != std::string_view::npos) {
                        errorMessage = std::string(responseView.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
                    }
                }
            }
            
            if (exc) exc("Discord Error " + errorCode + ": " + errorMessage);
            return false;
        }
        
        return true;
    }
    
    return true;
}

void DiscordRichPresence::Update(Exception exc) noexcept {
    if (!m_connected || m_pipe == INVALID_HANDLE_VALUE)
        return;
    
    m_pingCounter++;
    if (m_pingCounter >= PING_INTERVAL) {
        if (!sendDiscordMessageSync(1, R"({"cmd":"DISPATCH","evt":"READY"})", exc)) {
            m_connected = false;
            return;
        }
        m_pingCounter = 0;
    }
}

bool DiscordRichPresence::Connect(__int64 clientId, Exception exc) noexcept
{
    if (!m_connected) {
        return m_connected = connectToDiscord(clientId, exc);
    }
    return true;
}

void DiscordRichPresence::Close(Exception exc) noexcept {
    if (m_connected && m_pipe != INVALID_HANDLE_VALUE) {
        try {
            std::string clearActivity = R"({"cmd":"SET_ACTIVITY","args":{"pid":)" + std::to_string(::GetCurrentProcessId()) + R"(,"activity":null},"nonce":")" + generateNonce() + R"("})";
            sendDiscordMessageSync(1, clearActivity, exc);
        } catch (...) {
            // Ignore exceptions during cleanup
        }
        disconnect();
    }
}

bool DiscordRichPresence::SetPresence(const Presence& presence, Exception exc) noexcept {
    if (!m_connected || m_pipe == INVALID_HANDLE_VALUE) {
		if (exc) exc("Not connected to Discord");
        return false;
    }
    
    DWORD bytesAvailable;
    if (!::PeekNamedPipe(m_pipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
        m_connected = false;
		if (exc) exc("Lost connection to Discord");
        return false;
    }
    
    size_t estimatedSize = 200 + presence.state.size() + presence.details.size() + 
                          presence.largeImage.size() + presence.largeText.size() + 
                          presence.smallImage.size() + presence.smallText.size();
    
    std::string activityJson;
    activityJson.reserve(estimatedSize);
    
    activityJson = R"({"cmd":"SET_ACTIVITY","args":{"pid":)" + std::to_string(::GetCurrentProcessId()) + R"(,"activity":{)";

    bool needComma = false;
    if (presence.state.size() >= MIN_STRING_LENGTH) {
        activityJson += R"("state":")" + escapeJsonString(presence.state) + R"(")";
        needComma = true;
    }
    if (presence.details.size() >= MIN_STRING_LENGTH) {
        if (needComma) activityJson += ",";
        activityJson += R"("details":")" + escapeJsonString(presence.details) + R"(")";
        needComma = true;
    }
    
    if (presence.startTime > 0) {
        if (needComma) activityJson += ",";
        activityJson += R"("timestamps":{"start":)" + std::to_string(presence.startTime);
        if (presence.endTime > 0) {
            activityJson += R"(,"end":)" + std::to_string(presence.endTime);
        }
        activityJson += "}";
        needComma = true;
    }
    
    if (!presence.largeImage.empty() || !presence.smallImage.empty() || 
        !presence.largeText.empty() || !presence.smallText.empty()) {
        if (needComma) activityJson += ",";
        activityJson += R"("assets":{)";
        
        bool hasAssets = false;
        if (!presence.largeImage.empty()) {
            activityJson += R"("large_image":")" + escapeJsonString(presence.largeImage) + R"(")";
            hasAssets = true;
        }
        if (presence.largeText.size() >= MIN_STRING_LENGTH) {
            if (hasAssets) activityJson += ",";
            activityJson += R"("large_text":")" + escapeJsonString(presence.largeText) + R"(")";
            hasAssets = true;
        }
        if (!presence.smallImage.empty()) {
            if (hasAssets) activityJson += ",";
            activityJson += R"("small_image":")" + escapeJsonString(presence.smallImage) + R"(")";
            hasAssets = true;
        }
        if (presence.smallText.size() >= MIN_STRING_LENGTH) {
            if (hasAssets) activityJson += ",";
            activityJson += R"("small_text":")" + escapeJsonString(presence.smallText) + R"(")";
        }
        
        activityJson += "}";
        needComma = true;
    }
    
    if (presence.enableButtonRepository && !presence.repositoryUrl.empty()) {
        if (needComma) activityJson += ",";
        activityJson += R"("buttons":[{"label":"View Repository","url":")" + escapeJsonString(presence.repositoryUrl) + R"("}])";
        needComma = true;
    }
    
    activityJson += R"(}},"nonce":")" + generateNonce() + R"("})";

	// std::printf(("\n > Presence JSON: " + activityJson).c_str()); // Debug: Uncomment if needed
    
    return sendDiscordMessageSync(1, activityJson, exc);
}

bool DiscordRichPresence::CheckConnection(Exception exc) const noexcept {
    if (m_pipe == INVALID_HANDLE_VALUE || !m_connected) {
		if (exc) exc("Not connected to Discord");
        return false;
    }
    
    DWORD bytesAvailable;
    if (!::PeekNamedPipe(m_pipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
		if (exc) exc("Lost connection to Discord");
        return false;
    }
    
    return sendDiscordMessageSync(1, R"({"cmd":"DISPATCH","evt":"READY"})", exc);
}

void DiscordRichPresence::disconnect() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
    m_connected = false;
}