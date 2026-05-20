#pragma once

#include "logging/Logger.h"

#include <atomic>
#include <functional>
#include <mfobjects.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace ccstreamer {

enum class YuvMatrix {
    Bt709,
    Bt601,
};

struct CameraDevice {
    int index = -1;
    std::wstring name;
    std::wstring symbolicLink;
};

struct CameraFormat {
    int index = -1;
    std::wstring label;
    GUID subtype {};
    UINT32 width = 0;
    UINT32 height = 0;
    UINT32 fpsNumerator = 0;
    UINT32 fpsDenominator = 1;
};

class MediaFoundationCamera {
public:
    MediaFoundationCamera();
    ~MediaFoundationCamera();

    MediaFoundationCamera(const MediaFoundationCamera&) = delete;
    MediaFoundationCamera& operator=(const MediaFoundationCamera&) = delete;

    static std::vector<CameraDevice> enumerateDevices();
    static std::vector<CameraFormat> enumerateFormats(const CameraDevice& device);

    bool startPreview(const CameraDevice& device, HWND targetWindow, int formatIndex = -1, bool fullRange = false, YuvMatrix matrix = YuvMatrix::Bt709);
    void stopPreview();
    void setFrameCallback(std::function<void(const BYTE*, LONG, LONG)> callback);
    std::wstring lastError() const;

private:
    void previewLoop(CameraDevice device, HWND targetWindow, int formatIndex, bool fullRange, YuvMatrix matrix);
    static void drawFrame(HWND targetWindow, const BYTE* data, LONG width, LONG height, LONG stride);
    static bool convertFrameToBgra(IMFMediaBuffer* buffer, const GUID& subtype, LONG width, LONG height, bool fullRange, YuvMatrix matrix, std::vector<BYTE>& output);
    void setError(std::wstring message);

    std::atomic_bool running_ = false;
    std::thread previewThread_;
    std::wstring lastError_;
    std::mutex callbackMutex_;
    std::function<void(const BYTE*, LONG, LONG)> frameCallback_;
    Logger logger_;
};

} // namespace ccstreamer
