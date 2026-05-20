#include "capture/MediaFoundationCamera.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace ccstreamer {

using Microsoft::WRL::ComPtr;

namespace {

std::wstring hrText(const wchar_t* context, HRESULT result)
{
    wchar_t buffer[128] {};
    swprintf_s(buffer, L"%s failed: 0x%08X", context, static_cast<unsigned int>(result));
    return buffer;
}

bool isCameraBusyResult(HRESULT result)
{
    return result == MF_E_HW_MFT_FAILED_START_STREAMING
        || result == HRESULT_FROM_WIN32(ERROR_BUSY)
        || result == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION);
}

std::wstring cameraReadFailureText(HRESULT result)
{
    if (isCameraBusyResult(result)) {
        return L"Camera read failed: camera is likely already in use by another app or driver transform";
    }

    return hrText(L"ReadSample", result);
}

std::wstring getAllocatedString(IMFActivate* activate, const GUID& key)
{
    wchar_t* value = nullptr;
    UINT32 length = 0;
    if (FAILED(activate->GetAllocatedString(key, &value, &length))) {
        return {};
    }

    std::wstring result(value, length);
    CoTaskMemFree(value);
    return result;
}

std::wstring subtypeName(const GUID& subtype)
{
    if (subtype == MFVideoFormat_RGB32) return L"RGB32";
    if (subtype == MFVideoFormat_ARGB32) return L"ARGB32";
    if (subtype == MFVideoFormat_NV12) return L"NV12";
    if (subtype == MFVideoFormat_YUY2) return L"YUY2";
    if (subtype == MFVideoFormat_MJPG) return L"MJPG";
    if (subtype == MFVideoFormat_I420) return L"I420";
    if (subtype == MFVideoFormat_YV12) return L"YV12";
    return L"UNKNOWN";
}

std::wstring formatLabel(UINT32 width, UINT32 height, UINT32 fpsNumerator, UINT32 fpsDenominator, const GUID& subtype)
{
    const UINT32 fps = fpsDenominator == 0 ? 0 : static_cast<UINT32>((static_cast<double>(fpsNumerator) / fpsDenominator) + 0.5);
    wchar_t buffer[160] {};
    swprintf_s(buffer, L"%ux%u @ %u fps - %s", width, height, fps, subtypeName(subtype).c_str());
    return buffer;
}

std::wstring mediaTypeSummary(IMFMediaType* type)
{
    if (type == nullptr) {
        return L"<none>";
    }

    GUID subtype {};
    UINT32 width = 0;
    UINT32 height = 0;
    UINT32 fpsNumerator = 0;
    UINT32 fpsDenominator = 1;
    type->GetGUID(MF_MT_SUBTYPE, &subtype);
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
    return formatLabel(width, height, fpsNumerator, fpsDenominator, subtype);
}

ComPtr<IMFMediaType> createRgb32MediaType()
{
    ComPtr<IMFMediaType> mediaType;
    if (FAILED(MFCreateMediaType(&mediaType))) {
        return nullptr;
    }

    mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    return mediaType;
}

ComPtr<IMFMediaSource> createMediaSourceForDevice(const CameraDevice& device)
{
    ComPtr<IMFAttributes> attributes;
    ComPtr<IMFMediaSource> mediaSource;

    if (FAILED(MFCreateAttributes(&attributes, 2))) {
        return nullptr;
    }

    attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, device.symbolicLink.c_str());
    MFCreateDeviceSource(attributes.Get(), &mediaSource);
    return mediaSource;
}

RECT fitRectToAspect(const RECT& bounds, LONG aspectWidth, LONG aspectHeight)
{
    const LONG boundsWidth = bounds.right - bounds.left;
    const LONG boundsHeight = bounds.bottom - bounds.top;
    if (boundsWidth <= 0 || boundsHeight <= 0 || aspectWidth <= 0 || aspectHeight <= 0) {
        return bounds;
    }

    LONG width = boundsWidth;
    LONG height = (width * aspectHeight) / aspectWidth;

    if (height > boundsHeight) {
        height = boundsHeight;
        width = (height * aspectWidth) / aspectHeight;
    }

    const LONG left = bounds.left + ((boundsWidth - width) / 2);
    const LONG top = bounds.top + ((boundsHeight - height) / 2);
    return { left, top, left + width, top + height };
}

BYTE clampByte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<BYTE>(value);
}

void yuvToBgraLimitedBt709(int yValue, int uValue, int vValue, BYTE* target)
{
    const int c = yValue - 16;
    const int d = uValue - 128;
    const int e = vValue - 128;

    const int r = (298 * c + 459 * e + 128) >> 8;
    const int g = (298 * c - 55 * d - 136 * e + 128) >> 8;
    const int b = (298 * c + 541 * d + 128) >> 8;

    target[0] = clampByte(b);
    target[1] = clampByte(g);
    target[2] = clampByte(r);
    target[3] = 0;
}

void yuvToBgraLimitedBt601(int yValue, int uValue, int vValue, BYTE* target)
{
    const int c = yValue - 16;
    const int d = uValue - 128;
    const int e = vValue - 128;

    const int r = (298 * c + 409 * e + 128) >> 8;
    const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    const int b = (298 * c + 516 * d + 128) >> 8;

    target[0] = clampByte(b);
    target[1] = clampByte(g);
    target[2] = clampByte(r);
    target[3] = 0;
}

void yuvToBgraFullBt709(int yValue, int uValue, int vValue, BYTE* target)
{
    const int d = uValue - 128;
    const int e = vValue - 128;

    const int r = yValue + ((403 * e + 128) >> 8);
    const int g = yValue - ((48 * d + 120 * e + 128) >> 8);
    const int b = yValue + ((475 * d + 128) >> 8);

    target[0] = clampByte(b);
    target[1] = clampByte(g);
    target[2] = clampByte(r);
    target[3] = 0;
}

void yuvToBgraFullBt601(int yValue, int uValue, int vValue, BYTE* target)
{
    const int d = uValue - 128;
    const int e = vValue - 128;

    const int r = yValue + ((359 * e + 128) >> 8);
    const int g = yValue - ((88 * d + 183 * e + 128) >> 8);
    const int b = yValue + ((454 * d + 128) >> 8);

    target[0] = clampByte(b);
    target[1] = clampByte(g);
    target[2] = clampByte(r);
    target[3] = 0;
}

void yuvToBgra(int yValue, int uValue, int vValue, bool fullRange, YuvMatrix matrix, BYTE* target)
{
    if (matrix == YuvMatrix::Bt601) {
        if (fullRange) {
            yuvToBgraFullBt601(yValue, uValue, vValue, target);
        } else {
            yuvToBgraLimitedBt601(yValue, uValue, vValue, target);
        }
        return;
    }

    if (fullRange) {
        yuvToBgraFullBt709(yValue, uValue, vValue, target);
    } else {
        yuvToBgraLimitedBt709(yValue, uValue, vValue, target);
    }
}

bool copyBgraWithStride(IMFMediaBuffer* buffer, LONG width, LONG height, std::vector<BYTE>& output)
{
    BYTE* data = nullptr;
    LONG stride = 0;
    ComPtr<IMF2DBuffer> buffer2d;

    output.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);

    if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buffer2d))) && SUCCEEDED(buffer2d->Lock2D(&data, &stride))) {
        const LONG absoluteStride = stride < 0 ? -stride : stride;
        if (absoluteStride < width * 4) {
            buffer2d->Unlock2D();
            return false;
        }

        for (LONG y = 0; y < height; ++y) {
            const BYTE* source = stride >= 0
                ? data + (static_cast<size_t>(y) * absoluteStride)
                : data + (static_cast<size_t>(height - 1 - y) * absoluteStride);
            BYTE* target = output.data() + (static_cast<size_t>(y) * width * 4);
            std::memcpy(target, source, static_cast<size_t>(width) * 4);
        }

        buffer2d->Unlock2D();
        return true;
    }

    DWORD maxLength = 0;
    DWORD currentLength = 0;
    if (FAILED(buffer->Lock(&data, &maxLength, &currentLength))) {
        return false;
    }

    const size_t required = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (currentLength >= required) {
        std::memcpy(output.data(), data, required);
    }
    buffer->Unlock();
    return currentLength >= required;
}

bool convertNv12ToBgra(IMFMediaBuffer* buffer, LONG width, LONG height, bool fullRange, YuvMatrix matrix, std::vector<BYTE>& output)
{
    BYTE* data = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    if (FAILED(buffer->Lock(&data, &maxLength, &currentLength))) {
        return false;
    }

    const size_t yPlaneSize = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t uvPlaneSize = yPlaneSize / 2;
    if (currentLength < yPlaneSize + uvPlaneSize) {
        buffer->Unlock();
        return false;
    }

    const BYTE* yPlane = data;
    const BYTE* uvPlane = data + yPlaneSize;
    output.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);

    for (LONG y = 0; y < height; ++y) {
        for (LONG x = 0; x < width; ++x) {
            const int yValue = yPlane[static_cast<size_t>(y) * width + x];
            const size_t uvIndex = static_cast<size_t>(y / 2) * width + static_cast<size_t>(x & ~1);
            const int uValue = uvPlane[uvIndex + 0];
            const int vValue = uvPlane[uvIndex + 1];
            BYTE* target = output.data() + ((static_cast<size_t>(y) * width + x) * 4);
            yuvToBgra(yValue, uValue, vValue, fullRange, matrix, target);
        }
    }

    buffer->Unlock();
    return true;
}

bool convertYuy2ToBgra(IMFMediaBuffer* buffer, LONG width, LONG height, bool fullRange, YuvMatrix matrix, std::vector<BYTE>& output)
{
    BYTE* data = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    if (FAILED(buffer->Lock(&data, &maxLength, &currentLength))) {
        return false;
    }

    const size_t required = static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
    if (currentLength < required) {
        buffer->Unlock();
        return false;
    }

    output.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);

    for (LONG y = 0; y < height; ++y) {
        const BYTE* source = data + (static_cast<size_t>(y) * width * 2);
        for (LONG x = 0; x < width; x += 2) {
            const int y0 = source[0];
            const int u = source[1];
            const int y1 = source[2];
            const int v = source[3];
            BYTE* target0 = output.data() + ((static_cast<size_t>(y) * width + x) * 4);
            BYTE* target1 = output.data() + ((static_cast<size_t>(y) * width + x + 1) * 4);
            yuvToBgra(y0, u, v, fullRange, matrix, target0);
            if (x + 1 < width) {
                yuvToBgra(y1, u, v, fullRange, matrix, target1);
            }
            source += 4;
        }
    }

    buffer->Unlock();
    return true;
}

}

MediaFoundationCamera::MediaFoundationCamera()
    : logger_("MediaFoundationCamera")
{
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

MediaFoundationCamera::~MediaFoundationCamera()
{
    stopPreview();
    MFShutdown();
}

std::vector<CameraDevice> MediaFoundationCamera::enumerateDevices()
{
    std::vector<CameraDevice> devices;

    ComPtr<IMFAttributes> attributes;
    if (FAILED(MFCreateAttributes(&attributes, 1))) {
        return devices;
    }

    if (FAILED(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
        return devices;
    }

    IMFActivate** activateArray = nullptr;
    UINT32 count = 0;
    if (FAILED(MFEnumDeviceSources(attributes.Get(), &activateArray, &count))) {
        return devices;
    }

    for (UINT32 i = 0; i < count; ++i) {
        CameraDevice device;
        device.index = static_cast<int>(i);
        device.name = getAllocatedString(activateArray[i], MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
        device.symbolicLink = getAllocatedString(activateArray[i], MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        devices.push_back(std::move(device));
        activateArray[i]->Release();
    }

    CoTaskMemFree(activateArray);
    return devices;
}

std::vector<CameraFormat> MediaFoundationCamera::enumerateFormats(const CameraDevice& device)
{
    std::vector<CameraFormat> formats;

    const auto mediaSource = createMediaSourceForDevice(device);
    if (mediaSource == nullptr) {
        return formats;
    }

    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromMediaSource(mediaSource.Get(), nullptr, &reader))) {
        mediaSource->Shutdown();
        return formats;
    }

    for (DWORD index = 0;; ++index) {
        ComPtr<IMFMediaType> type;
        const HRESULT result = reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), index, &type);
        if (result == MF_E_NO_MORE_TYPES) {
            break;
        }
        if (FAILED(result) || type == nullptr) {
            continue;
        }

        GUID subtype {};
        UINT32 width = 0;
        UINT32 height = 0;
        UINT32 fpsNumerator = 0;
        UINT32 fpsDenominator = 1;

        type->GetGUID(MF_MT_SUBTYPE, &subtype);
        MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
        MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);

        if (width == 0 || height == 0) {
            continue;
        }

        CameraFormat format;
        format.index = static_cast<int>(index);
        format.subtype = subtype;
        format.width = width;
        format.height = height;
        format.fpsNumerator = fpsNumerator;
        format.fpsDenominator = fpsDenominator == 0 ? 1 : fpsDenominator;
        format.label = formatLabel(width, height, format.fpsNumerator, format.fpsDenominator, subtype);
        formats.push_back(std::move(format));
    }

    mediaSource->Shutdown();
    return formats;
}

bool MediaFoundationCamera::startPreview(const CameraDevice& device, HWND targetWindow, int formatIndex, bool fullRange, YuvMatrix matrix)
{
    if (targetWindow == nullptr || device.symbolicLink.empty()) {
        setError(L"Invalid camera target or device link");
        return false;
    }

    stopPreview();
    setError(L"");
    running_ = true;
    previewThread_ = std::thread(&MediaFoundationCamera::previewLoop, this, device, targetWindow, formatIndex, fullRange, matrix);
    return true;
}

void MediaFoundationCamera::stopPreview()
{
    running_ = false;
    if (previewThread_.joinable()) {
        previewThread_.join();
    }
}

void MediaFoundationCamera::setFrameCallback(std::function<void(const BYTE*, LONG, LONG)> callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

std::wstring MediaFoundationCamera::lastError() const
{
    return lastError_;
}

void MediaFoundationCamera::setError(std::wstring message)
{
    lastError_ = std::move(message);
    if (!lastError_.empty()) {
        logger_.error(lastError_);
    }
}

void MediaFoundationCamera::previewLoop(CameraDevice device, HWND targetWindow, int formatIndex, bool fullRange, YuvMatrix matrix)
{
    const HRESULT coResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(coResult);

    ComPtr<IMFAttributes> readerAttributes;
    ComPtr<IMFMediaSource> mediaSource;
    ComPtr<IMFSourceReader> reader;
    HRESULT result = S_OK;

    mediaSource = createMediaSourceForDevice(device);
    if (mediaSource == nullptr) {
        setError(L"MFCreateDeviceSource failed");
    }

    if (SUCCEEDED(MFCreateAttributes(&readerAttributes, 2))) {
        readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE);
        readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
    }

    result = mediaSource != nullptr ? MFCreateSourceReaderFromMediaSource(mediaSource.Get(), readerAttributes.Get(), &reader) : E_FAIL;
    if (mediaSource == nullptr || FAILED(result)) {
        if (lastError_.empty()) {
            setError(hrText(L"MFCreateSourceReaderFromMediaSource", result));
        }
        running_ = false;
        if (shouldUninitializeCom) {
            CoUninitialize();
        }
        return;
    }

    if (formatIndex >= 0) {
        ComPtr<IMFMediaType> nativeType;
        result = reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), static_cast<DWORD>(formatIndex), &nativeType);
        if (SUCCEEDED(result) && nativeType != nullptr) {
            logger_.info(L"Requested camera format: " + mediaTypeSummary(nativeType.Get()));
            result = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, nativeType.Get());
            if (FAILED(result)) {
                setError(hrText(L"Set selected camera format", result));
            }
        }
    }

    bool explicitYuvConversion = false;
    GUID selectedSubtype = {};
    UINT32 selectedWidth = 0;
    UINT32 selectedHeight = 0;
    {
        ComPtr<IMFMediaType> selectedType;
        if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &selectedType)) && selectedType != nullptr) {
            logger_.info(L"Active camera format before preview conversion: " + mediaTypeSummary(selectedType.Get()));
            selectedType->GetGUID(MF_MT_SUBTYPE, &selectedSubtype);
            MFGetAttributeSize(selectedType.Get(), MF_MT_FRAME_SIZE, &selectedWidth, &selectedHeight);
            explicitYuvConversion = selectedSubtype == MFVideoFormat_NV12 || selectedSubtype == MFVideoFormat_YUY2;
        }
    }

    const bool requiresSourceReaderDecode = selectedSubtype == MFVideoFormat_MJPG;
    const auto rgb32Type = createRgb32MediaType();
    if (rgb32Type != nullptr && requiresSourceReaderDecode) {
        logger_.info(L"MJPG camera format selected; requesting software decode to RGB32 for preview");
        result = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, rgb32Type.Get());
        if (FAILED(result)) {
            setError(hrText(L"Set RGB32 camera preview format", result));
        } else {
            ComPtr<IMFMediaType> previewType;
            if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &previewType)) && previewType != nullptr) {
                logger_.info(L"Active camera preview format: " + mediaTypeSummary(previewType.Get()));
            }
        }
    }

    int failedConversionCount = 0;
    bool loggedConversionFailure = false;
    while (running_) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;

        const HRESULT readResult = reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &sample);

        if (FAILED(readResult) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            if (FAILED(readResult)) {
                setError(cameraReadFailureText(readResult));
                SetWindowTextW(targetWindow, isCameraBusyResult(readResult)
                    ? L"Camera is already in use by another app"
                    : L"Camera preview failed");
            }
            running_ = false;
            break;
        }

        if (sample == nullptr) {
            continue;
        }

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
            continue;
        }

        ComPtr<IMFMediaType> currentType;
        if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &currentType))) {
            continue;
        }

        UINT32 width = 0;
        UINT32 height = 0;
        if (FAILED(MFGetAttributeSize(currentType.Get(), MF_MT_FRAME_SIZE, &width, &height)) || width == 0 || height == 0) {
            continue;
        }

        GUID currentSubtype = {};
        currentType->GetGUID(MF_MT_SUBTYPE, &currentSubtype);

        std::vector<BYTE> bgra;
        if (convertFrameToBgra(buffer.Get(), currentSubtype, static_cast<LONG>(width), static_cast<LONG>(height), fullRange, matrix, bgra)) {
            failedConversionCount = 0;
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (frameCallback_) {
                    frameCallback_(bgra.data(), static_cast<LONG>(width), static_cast<LONG>(height));
                }
            }
            drawFrame(targetWindow, bgra.data(), static_cast<LONG>(width), static_cast<LONG>(height), static_cast<LONG>(width * 4));
        } else {
            ++failedConversionCount;
            if (!loggedConversionFailure) {
                loggedConversionFailure = true;
                logger_.error(L"Unsupported camera buffer for selected native format: " + subtypeName(currentSubtype));
                SetWindowTextW(targetWindow, L"Unsupported camera format for preview");
            }
            if (failedConversionCount >= 10) {
                running_ = false;
            }
        }
    }

    reader.Reset();
    if (mediaSource != nullptr) {
        mediaSource->Shutdown();
    }

    if (shouldUninitializeCom) {
        CoUninitialize();
    }
}

void MediaFoundationCamera::drawFrame(HWND targetWindow, const BYTE* data, LONG width, LONG height, LONG stride)
{
    if (targetWindow == nullptr || data == nullptr || width <= 0 || height <= 0 || stride <= 0) {
        return;
    }

    RECT client {};
    GetClientRect(targetWindow, &client);

    HDC dc = GetDC(targetWindow);
    if (dc == nullptr) {
        return;
    }

    const int targetWidth = client.right - client.left;
    const int targetHeight = client.bottom - client.top;
    if (targetWidth <= 0 || targetHeight <= 0) {
        ReleaseDC(targetWindow, dc);
        return;
    }

    HDC memoryDc = CreateCompatibleDC(dc);
    HBITMAP memoryBitmap = CreateCompatibleBitmap(dc, targetWidth, targetHeight);
    const auto oldBitmap = SelectObject(memoryDc, memoryBitmap);

    HBRUSH bg = CreateSolidBrush(RGB(2, 8, 12));
    FillRect(memoryDc, &client, bg);
    DeleteObject(bg);

    const RECT fitted = fitRectToAspect(client, width, height);

    BITMAPINFO info {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(memoryDc, HALFTONE);
    SetBrushOrgEx(memoryDc, 0, 0, nullptr);

    StretchDIBits(
        memoryDc,
        fitted.left,
        fitted.top,
        fitted.right - fitted.left,
        fitted.bottom - fitted.top,
        0,
        0,
        width,
        height,
        data,
        &info,
        DIB_RGB_COLORS,
        SRCCOPY);

    BitBlt(dc, 0, 0, targetWidth, targetHeight, memoryDc, 0, 0, SRCCOPY);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(memoryBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(targetWindow, dc);
}

bool MediaFoundationCamera::convertFrameToBgra(IMFMediaBuffer* buffer, const GUID& subtype, LONG width, LONG height, bool fullRange, YuvMatrix matrix, std::vector<BYTE>& output)
{
    if (buffer == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    if (subtype == MFVideoFormat_NV12) {
        return convertNv12ToBgra(buffer, width, height, fullRange, matrix, output);
    }

    if (subtype == MFVideoFormat_YUY2) {
        return convertYuy2ToBgra(buffer, width, height, fullRange, matrix, output);
    }

    return copyBgraWithStride(buffer, width, height, output);
}

} // namespace ccstreamer
