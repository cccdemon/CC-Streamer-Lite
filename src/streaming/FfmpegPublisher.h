#pragma once

#include "logging/Logger.h"

#include <string>
#include <windows.h>

namespace ccstreamer {

struct StreamSelection {
    std::wstring windowTitle;
    HWND windowHandle = nullptr;
    std::wstring cameraName;
    int cameraWidth = 1920;
    int cameraHeight = 1080;
    int cameraFps = 24;
    bool cameraFromPreviewRegion = false;
    int cameraRegionX = 0;
    int cameraRegionY = 0;
    int cameraRegionWidth = 0;
    int cameraRegionHeight = 0;
};

struct StreamEndpoints {
    std::wstring gameSrtUrl;
    std::wstring camSrtUrl;
};

class FfmpegPublisher final {
public:
    FfmpegPublisher();
    ~FfmpegPublisher();

    FfmpegPublisher(const FfmpegPublisher&) = delete;
    FfmpegPublisher& operator=(const FfmpegPublisher&) = delete;

    bool start(const StreamSelection& selection, const StreamEndpoints& endpoints, std::wstring& error);
    bool startCameraPipe(int width, int height, int fps, const std::wstring& camSrtUrl, std::wstring& error);
    void submitCameraFrame(const BYTE* bgra, LONG width, LONG height);
    void stop();
    bool running() const;
    bool available() const;
    bool installWithPrompt(HWND owner, std::wstring& error);
    bool gameCaptureUsedFallback() const;

private:
    bool startProcess(const std::wstring& label, const std::wstring& arguments, PROCESS_INFORMATION& process, std::wstring& error);
    bool verifyProcessRunning(const std::wstring& label, const PROCESS_INFORMATION& process, std::wstring& error) const;
    std::wstring findFfmpeg() const;
    std::wstring appDirectory() const;
    std::wstring ffmpegLogPath(const std::wstring& label) const;
    std::wstring readLogTail(const std::wstring& path) const;
    static std::wstring quote(const std::wstring& value);
    static bool processRunning(const PROCESS_INFORMATION& process);
    static void stopProcess(PROCESS_INFORMATION& process);

    std::wstring ffmpegPath_;
    PROCESS_INFORMATION gameProcess_ {};
    PROCESS_INFORMATION camProcess_ {};
    HANDLE camInputWrite_ = nullptr;
    bool gameCaptureUsedFallback_ = false;
    Logger logger_;
};

} // namespace ccstreamer
