#include "config/Profile.h"

#include <algorithm>
#include <cctype>

namespace ccstreamer {

std::string outputProtocolToString(OutputProtocol protocol)
{
    switch (protocol) {
    case OutputProtocol::Srt:
        return "srt";
    case OutputProtocol::Rtsp:
        return "rtsp";
    case OutputProtocol::Whip:
        return "whip";
    }

    return "unknown";
}

bool tryParseOutputProtocol(const std::string& value, OutputProtocol& protocol)
{
    auto normalized = value;
    normalized.erase(normalized.begin(), std::find_if(normalized.begin(), normalized.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), normalized.end());
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "srt") {
        protocol = OutputProtocol::Srt;
        return true;
    }

    if (normalized == "rtsp") {
        protocol = OutputProtocol::Rtsp;
        return true;
    }

    if (normalized == "whip") {
        protocol = OutputProtocol::Whip;
        return true;
    }

    return false;
}

} // namespace ccstreamer
