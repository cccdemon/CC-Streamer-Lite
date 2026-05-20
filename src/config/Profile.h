#pragma once

#include <QString>
#include <QVector>

namespace ccstreamer {

enum class OutputProtocol {
    Srt,
    Rtsp,
};

struct VideoProfile {
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int bitrateKbps = 6000;
    QString encoder = "auto";
};

struct AudioProfile {
    QString codec = "opus";
    int bitrateKbps = 128;
    int sampleRate = 48000;
    int channels = 2;
};

struct OutputProfile {
    QString name;
    bool enabled = false;
    OutputProtocol protocol = OutputProtocol::Srt;
    QString url;
    QString streamKey;
    QString srtPassphrase;
};

struct StreamProfile {
    VideoProfile video;
    AudioProfile audio;
    QVector<OutputProfile> outputs;
};

QString outputProtocolToString(OutputProtocol protocol);
bool tryParseOutputProtocol(const QString& value, OutputProtocol& protocol);

} // namespace ccstreamer

