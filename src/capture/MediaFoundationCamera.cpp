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

}

MediaFoundationCamera::MediaFoundationCamera()
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

bool MediaFoundationCamera::startPreview(const CameraDevice& device, HWND targetWindow, int formatIndex)
{
    if (targetWindow == nullptr || device.symbolicLink.empty()) {
        setError(L"Invalid camera target or device link");
        return false;
    }

    stopPreview();
    setError(L"");
    running_ = true;
    previewThread_ = std::thread(&MediaFoundationCamera::previewLoop, this, device, targetWindow, formatIndex);
    return true;
}

void MediaFoundationCamera::stopPreview()
{
    running_ = false;
    if (previewThread_.joinable()) {
        previewThread_.join();
    }
}

std::wstring MediaFoundationCamera::lastError() const
{
    return lastError_;
}

void MediaFoundationCamera::setError(std::wstring message)
{
    lastError_ = std::move(message);
}

void MediaFoundationCamera::previewLoop(CameraDevice device, HWND targetWindow, int formatIndex)
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

    if (SUCCEEDED(MFCreateAttributes(&readerAttributes, 1))) {
        readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
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
            result = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, nativeType.Get());
            if (FAILED(result)) {
                setError(hrText(L"Set selected camera format", result));
            }
        }
    }

    const auto rgb32Type = createRgb32MediaType();
    if (rgb32Type != nullptr) {
        result = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, rgb32Type.Get());
        if (FAILED(result)) {
            setError(hrText(L"Set RGB32 camera preview format", result));
            running_ = false;
        }
    }

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
                setError(hrText(L"ReadSample", readResult));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
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

        BYTE* data = nullptr;
        LONG stride = 0;
        ComPtr<IMF2DBuffer> buffer2d;
        if (SUCCEEDED(buffer.As(&buffer2d)) && SUCCEEDED(buffer2d->Lock2D(&data, &stride))) {
            if (stride < static_cast<LONG>(width * 4)) {
                buffer2d->Unlock2D();
                continue;
            }

            std::vector<BYTE> packed(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
            const LONG sourceStride = stride;
            const LONG absoluteStride = sourceStride < 0 ? -sourceStride : sourceStride;
            for (UINT32 y = 0; y < height; ++y) {
                const BYTE* source = sourceStride >= 0
                    ? data + (static_cast<size_t>(y) * absoluteStride)
                    : data + (static_cast<size_t>(height - 1 - y) * absoluteStride);
                BYTE* target = packed.data() + (static_cast<size_t>(y) * width * 4);
                std::memcpy(target, source, static_cast<size_t>(width) * 4);
            }
            buffer2d->Unlock2D();
            drawFrame(targetWindow, packed.data(), static_cast<LONG>(width), static_cast<LONG>(height), static_cast<LONG>(width * 4));
        } else {
            DWORD maxLength = 0;
            DWORD currentLength = 0;
            if (SUCCEEDED(buffer->Lock(&data, &maxLength, &currentLength))) {
                drawFrame(targetWindow, data, static_cast<LONG>(width), static_cast<LONG>(height), static_cast<LONG>(width * 4));
                buffer->Unlock();
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

} // namespace ccstreamer
