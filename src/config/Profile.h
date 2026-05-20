#pragma once

#include <string>
#include <vector>

namespace ccstreamer {

enum class OutputProtocol {
    Srt,
    Rtsp,
    Whip,
};

struct VideoProfile {
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int bitrateKbps = 6000;
    std::string codec = "h264";
    std::string encoder = "hardware-auto";
    std::string colorMode = "yuv";
    std::string colorMatrix = "bt709";
    std::string colorRange = "limited";
};

struct AudioProfile {
    std::string codec = "opus";
    int bitrateKbps = 128;
    int sampleRate = 48000;
    int channels = 2;
};

struct OutputProfile {
    std::string name;
    bool enabled = false;
    OutputProtocol protocol = OutputProtocol::Srt;
    std::string url;
    std::string streamKey;
    std::string srtPassphrase;
};

struct StreamProfile {
    VideoProfile video;
    AudioProfile audio;
    std::vector<OutputProfile> outputs;
};

std::string outputProtocolToString(OutputProtocol protocol);
bool tryParseOutputProtocol(const std::string& value, OutputProtocol& protocol);

} // namespace ccstreamer
