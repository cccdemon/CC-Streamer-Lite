#include "config/Profile.h"

namespace ccstreamer {

QString outputProtocolToString(OutputProtocol protocol)
{
    switch (protocol) {
    case OutputProtocol::Srt:
        return "srt";
    case OutputProtocol::Rtsp:
        return "rtsp";
    }

    return "unknown";
}

bool tryParseOutputProtocol(const QString& value, OutputProtocol& protocol)
{
    const auto normalized = value.trimmed().toLower();

    if (normalized == "srt") {
        protocol = OutputProtocol::Srt;
        return true;
    }

    if (normalized == "rtsp") {
        protocol = OutputProtocol::Rtsp;
        return true;
    }

    return false;
}

} // namespace ccstreamer

