#include "logging/Logger.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <utility>
#include <windows.h>

namespace ccstreamer {

Logger::Logger(std::string category)
    : category_(std::move(category))
    , sensitiveKeys_({
          "stream_key",
          "srt_passphrase",
          "password",
          "passwd",
          "token",
          "access_token",
          "refresh_token",
          "secret",
      })
{
}

void Logger::info(const std::string& message) const
{
    write("INFO", redact(message));
}

void Logger::warning(const std::string& message) const
{
    write("WARN", redact(message));
}

void Logger::error(const std::string& message) const
{
    write("ERROR", redact(message));
}

void Logger::info(const std::wstring& message) const
{
    info(narrow(message));
}

void Logger::warning(const std::wstring& message) const
{
    warning(narrow(message));
}

void Logger::error(const std::wstring& message) const
{
    error(narrow(message));
}

std::string Logger::redact(const std::string& message) const
{
    std::string result = message;

    for (const auto& key : sensitiveKeys_) {
        const std::regex quotedPattern(
            "(" + key + R"(\s*=\s*["'])([^"']+)(["']))",
            std::regex_constants::icase);
        result = std::regex_replace(result, quotedPattern, "$1[REDACTED]$3");

        const std::regex queryPattern(
            "([?&]" + key + R"(=)([^&\s]+))",
            std::regex_constants::icase);
        result = std::regex_replace(result, queryPattern, "$1[REDACTED]");
    }

    const std::regex userInfoPattern(R"((://)([^:/@\s]+):([^@/\s]+)@)");
    result = std::regex_replace(result, userInfoPattern, "$1[REDACTED]:[REDACTED]@");

    return result;
}

std::wstring Logger::redact(const std::wstring& message) const
{
    const auto redacted = redact(narrow(message));
    const int size = MultiByteToWideChar(CP_UTF8, 0, redacted.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return L"[redaction failed]";
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, redacted.c_str(), -1, wide.data(), size);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

void Logger::write(const std::string& level, const std::string& message) const
{
    const auto line = "[" + level + "] [" + category_ + "] " + message + "\n";
    OutputDebugStringA(line.c_str());

    if (level == "ERROR") {
        std::cerr << line;
    } else {
        std::clog << line;
    }

    std::ofstream file(logPath(), std::ios::app);
    if (file.is_open()) {
        file << line;
    }
}

std::string Logger::narrow(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

std::string Logger::logPath()
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path path = length > 0 ? std::filesystem::path(buffer).parent_path() : std::filesystem::current_path();
    path /= "CCStreamer.log";
    return path.string();
}

} // namespace ccstreamer
