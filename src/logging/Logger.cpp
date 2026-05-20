#include "logging/Logger.h"

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
    const auto line = "[INFO] [" + category_ + "] " + redact(message) + "\n";
    OutputDebugStringA(line.c_str());
    std::clog << line;
}

void Logger::warning(const std::string& message) const
{
    const auto line = "[WARN] [" + category_ + "] " + redact(message) + "\n";
    OutputDebugStringA(line.c_str());
    std::clog << line;
}

void Logger::error(const std::string& message) const
{
    const auto line = "[ERROR] [" + category_ + "] " + redact(message) + "\n";
    OutputDebugStringA(line.c_str());
    std::cerr << line;
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

} // namespace ccstreamer
