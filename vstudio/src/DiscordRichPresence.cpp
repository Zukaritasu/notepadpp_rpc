// Copyright (C) 2025 - 2026 Zukaritasu
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

#include "DiscordRichPresence.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

DiscordRichPresence::DiscordRichPresence()  noexcept
    : m_pipe(INVALID_HANDLE_VALUE), m_connected(false), m_pingCounter(0)
{
    m_lastUpdateTime.store(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

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
    
    for (unsigned char c : str) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 32) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    escaped += buf;
                } else {
                    escaped += (char)c;
                }
                break;
        }
    }
    return escaped;
}

bool DiscordRichPresence::connectToDiscord(__int64 clientId, Exception exc) {
    for (int i = 0; i < MAX_PIPE_ATTEMPTS; i++) {
        std::string pipeName = R"(\\.\pipe\discord-ipc-)" + std::to_string(i);
        if (WaitNamedPipeA(pipeName.c_str(), 100)) {
            m_pipe = ::CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
            if (m_pipe != INVALID_HANDLE_VALUE) {
                std::string handshake = R"({"v":1,"client_id":")" + std::to_string(clientId) + R"("})";
                if (sendDiscordMessageSync(0, handshake, exc))
                    return true;
                ::CloseHandle(m_pipe);
                m_pipe = INVALID_HANDLE_VALUE;
            }
		}
    }
    if (exc) exc("Could not connect to Discord. Is Discord running?");
    return false;
}


namespace {
    static constexpr DWORD PIPE_WRITE_TIMEOUT_MS = 2000;   // 2s per write op
    static constexpr DWORD PIPE_READ_TIMEOUT_MS  = 3000;   // 3s per read op

    struct HandleDeleter {
        void operator()(HANDLE h) const {
            if (h != NULL && h != INVALID_HANDLE_VALUE) ::CloseHandle(h);
        }
    };

    using UniqueHandle = std::unique_ptr<void, HandleDeleter>;

    bool writeWithTimeout(HANDLE pipe, const void* buffer, DWORD size, DWORD timeoutMs) {
        if (pipe == INVALID_HANDLE_VALUE)
            return false;

        OVERLAPPED ov{};
        ov.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!ov.hEvent)
            return false;

        UniqueHandle ovHandle(ov.hEvent);
        
        DWORD written = 0;
        BOOL ok = ::WriteFile(pipe, buffer, size, &written, &ov);
        if (!ok) {
            DWORD err = ::GetLastError();
            if (err != ERROR_IO_PENDING) {
                return false;
            }

            DWORD waitRes = ::WaitForSingleObject(ov.hEvent, timeoutMs);
            if (waitRes != WAIT_OBJECT_0) {
                ::CancelIo(pipe);
                return false;
            }

            if (!::GetOverlappedResult(pipe, &ov, &written, FALSE) || written != size) {
                return false;
            }
        } else if (written != size) {
            return false;
        }

        return true;
    }

    bool readWithTimeout(HANDLE pipe, void* buffer, DWORD size, DWORD timeoutMs, DWORD& bytesReadOut) {
        bytesReadOut = 0;
        if (pipe == INVALID_HANDLE_VALUE)
            return false;

        OVERLAPPED ov{};
        ov.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!ov.hEvent)
            return false;

        UniqueHandle ovHandle(ov.hEvent);

        BOOL ok = ::ReadFile(pipe, buffer, size, &bytesReadOut, &ov);
        if (!ok) {
            DWORD err = ::GetLastError();
            if (err != ERROR_IO_PENDING) {
                return false;
            }

            DWORD waitRes = ::WaitForSingleObject(ov.hEvent, timeoutMs);
            if (waitRes != WAIT_OBJECT_0) {
                ::CancelIo(pipe);
                return false;
            }

            if (!::GetOverlappedResult(pipe, &ov, &bytesReadOut, FALSE)) {
                return false;
            }
        }

        return true;
    }
}

bool DiscordRichPresence::sendDiscordMessageSync(uint32_t opcode, const std::string& json, Exception exc) {
    if (m_pipe == INVALID_HANDLE_VALUE) {
		if (exc) exc("Pipe is not valid");
        return false;
    }
    
    DWORD bytesAvailable;
    if (!::PeekNamedPipe(m_pipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
		if (exc) exc("Lost connection to Discord");
        disconnect();
        return false;
    }

    if (json.size() > MAX_MESSAGE_SIZE) {
		if (exc) exc("Message size exceeds maximum limit");
        return false;
    }
    
    DiscordIPCHeader header{};
    header.opcode = opcode;
    header.length = static_cast<uint32_t>(json.size());

    if (!writeWithTimeout(m_pipe, &header, sizeof(header), PIPE_WRITE_TIMEOUT_MS))
        return false;
    if (!writeWithTimeout(m_pipe, json.c_str(), static_cast<DWORD>(json.size()), PIPE_WRITE_TIMEOUT_MS))
        return false;
    
    DiscordIPCHeader responseHeader{};
    DWORD bytesRead;

    if (!readWithTimeout(m_pipe, &responseHeader, sizeof(responseHeader), PIPE_READ_TIMEOUT_MS, bytesRead) ||
        bytesRead != sizeof(responseHeader)) {
        if (exc) exc("Failed to read response header from pipe");
        disconnect();
        return false;
    }
    
    if (responseHeader.length > MAX_MESSAGE_SIZE) {
		if (exc) exc("Response size exceeds maximum limit");
        return false;
    }
    
    if (responseHeader.length > 0) {
        std::string response;
        response.resize(responseHeader.length);
        if (!readWithTimeout(m_pipe, response.data(), responseHeader.length, PIPE_READ_TIMEOUT_MS, bytesRead) ||
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
    AutoUnlock lock(m_mutex);
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
    AutoUnlock lock(m_mutex);
    if (!m_connected) {
        return m_connected = connectToDiscord(clientId, exc);
    }
    return true;
}

void DiscordRichPresence::Close(Exception exc) noexcept {
    AutoUnlock lock(m_mutex);
    if (m_connected && m_pipe != INVALID_HANDLE_VALUE) {
        try {
            std::string clearActivity = R"({"cmd":"SET_ACTIVITY","args":{"pid":)" + std::to_string(::GetCurrentProcessId()) + R"(,"activity":null},"nonce":")" + generateNonce() + R"("})";
            sendDiscordMessageSync(1, clearActivity, exc);
        } catch (...) {}
        disconnect();
    }
}

bool DiscordRichPresence::SetPresence(const Presence& presence, Exception exc) noexcept {
    AutoUnlock lock(m_mutex);

	if (m_presence.compare(presence)) return true; // No changes, skip update
	m_presence = presence;

    m_lastUpdateTime.store(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

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

    return sendDiscordMessageSync(1, activityJson, exc);
}

bool DiscordRichPresence::SetIdleStatus(const Presence& presence, Exception exc) noexcept
{
    AutoUnlock lock(m_mutex);

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

    std::string idleJson;

    idleJson = R"({"cmd":"SET_ACTIVITY","args":{"pid":)" + std::to_string(::GetCurrentProcessId()) + R"(,"activity":{)";

    bool needComma = false;

    if (presence.details.size() >= MIN_STRING_LENGTH) {
        if (needComma) 
            idleJson += ",";
        idleJson += R"("details":")" + escapeJsonString(presence.details) + R"(")";
        needComma = true;
    }

    if (presence.startTime > 0) {
        if (needComma) 
            idleJson += ",";
        idleJson += R"("timestamps":{"start":)" + std::to_string(presence.startTime);

        if (presence.endTime > 0)
            idleJson += R"(,"end":)" + std::to_string(presence.endTime);
        idleJson += "}";
        needComma = true;
    }

    if (needComma) idleJson += ",";
    idleJson += R"("assets":{)";

    bool hasAssets = false;
    if (!presence.largeImage.empty()) {
        idleJson += R"("large_image":")" + escapeJsonString(presence.largeImage) + R"(")";
        hasAssets = true;
    }

    if (presence.largeText.size() >= MIN_STRING_LENGTH) {
        if (hasAssets) 
            idleJson += ",";
        idleJson += R"("large_text":")" + escapeJsonString(presence.largeText) + R"(")";
        hasAssets = true;
    }

    idleJson += R"(}}},"nonce":")" + generateNonce() + R"("})";

    return sendDiscordMessageSync(1, idleJson, exc);
}

bool DiscordRichPresence::CheckConnection(Exception exc) noexcept {
    AutoUnlock lock(m_mutex);
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