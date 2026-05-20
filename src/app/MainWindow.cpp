#include "app/MainWindow.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ccstreamer {

namespace {

constexpr int ControlIdPreview = 100;
constexpr int ControlIdSelectWindow = 101;
constexpr int ControlIdSelectCamera = 102;
constexpr int ControlIdAudio = 103;
constexpr int ControlIdPrimaryOutput = 104;
constexpr int ControlIdBackupOutput = 105;
constexpr int ControlIdStream = 106;
constexpr int ControlIdPrimaryEndpoint = 107;
constexpr int ControlIdBackupEndpoint = 108;
constexpr int ControlIdVideoCodec = 109;
constexpr int ControlIdHardwareAccel = 110;
constexpr int ControlIdColorMode = 111;
constexpr int ControlIdColorRange = 112;
constexpr int ControlIdColorMatrix = 113;
constexpr UINT MessageStartupChecks = WM_APP + 1;
constexpr int PickerIdList = 201;
constexpr int PickerIdPreview = 202;
constexpr int PickerIdOk = 203;
constexpr int PickerIdCancel = 204;
constexpr int PickerIdFormatList = 205;

constexpr COLORREF ColorBg = RGB(4, 6, 10);
constexpr COLORREF ColorBg2 = RGB(8, 14, 20);
constexpr COLORREF ColorBg3 = RGB(12, 21, 32);
constexpr COLORREF ColorText = RGB(200, 220, 232);
constexpr COLORREF ColorDim = RGB(92, 113, 126);
constexpr COLORREF ColorBorder = RGB(0, 70, 86);
constexpr COLORREF ColorCyan = RGB(0, 212, 255);
constexpr COLORREF ColorGold = RGB(240, 165, 0);
constexpr COLORREF ColorGreen = RGB(0, 255, 136);

HBRUSH pickerBrush()
{
    static HBRUSH brush = CreateSolidBrush(ColorBg2);
    return brush;
}

struct WindowCandidate {
    HWND window = nullptr;
    std::wstring title;
};

enum class PickerKind {
    Window,
    Camera,
};

struct PickerState {
    PickerKind kind = PickerKind::Window;
    bool done = false;
    bool accepted = false;
    HWND picker = nullptr;
    HWND list = nullptr;
    HWND formatList = nullptr;
    HWND preview = nullptr;
    HTHUMBNAIL thumbnail = nullptr;
    MediaFoundationCamera* cameraPreviewEngine = nullptr;
    bool fullRange = false;
    YuvMatrix matrix = YuvMatrix::Bt709;
    std::vector<WindowCandidate> windows;
    std::vector<CameraDevice> cameras;
    std::vector<CameraFormat> cameraFormats;
};

BOOL CALLBACK collectVisibleWindows(HWND window, LPARAM userData)
{
    auto* windows = reinterpret_cast<std::vector<WindowCandidate>*>(userData);

    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) {
        return TRUE;
    }

    wchar_t title[256] {};
    GetWindowTextW(window, title, 256);
    if (title[0] == L'\0') {
        return TRUE;
    }

    RECT rect {};
    GetWindowRect(window, &rect);
    if ((rect.right - rect.left) < 80 || (rect.bottom - rect.top) < 60) {
        return TRUE;
    }

    windows->push_back({ window, title });
    return TRUE;
}

std::vector<WindowCandidate> enumerateWindows(HWND excludeWindow)
{
    std::vector<WindowCandidate> windows;
    EnumWindows(collectVisibleWindows, reinterpret_cast<LPARAM>(&windows));
    windows.erase(
        std::remove_if(windows.begin(), windows.end(), [excludeWindow](const WindowCandidate& candidate) {
            return candidate.window == excludeWindow;
        }),
        windows.end());
    return windows;
}

void updateWindowPickerPreview(PickerState& state)
{
    if (state.thumbnail != nullptr) {
        DwmUnregisterThumbnail(state.thumbnail);
        state.thumbnail = nullptr;
    }

    const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
    if (selected < 0 || selected >= static_cast<int>(state.windows.size())) {
        return;
    }

    if (SUCCEEDED(DwmRegisterThumbnail(state.picker, state.windows[selected].window, &state.thumbnail))) {
        RECT rect {};
        GetWindowRect(state.preview, &rect);
        MapWindowPoints(HWND_DESKTOP, state.picker, reinterpret_cast<POINT*>(&rect), 2);
        ShowWindow(state.preview, SW_HIDE);

        DWM_THUMBNAIL_PROPERTIES properties {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
        properties.rcDestination = rect;
        properties.fVisible = TRUE;
        properties.opacity = 255;
        DwmUpdateThumbnailProperties(state.thumbnail, &properties);
    }
}

void updateCameraPickerPreview(PickerState& state)
{
    const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
    if (selected < 0 || selected >= static_cast<int>(state.cameras.size())) {
        return;
    }

    state.cameraFormats = MediaFoundationCamera::enumerateFormats(state.cameras[selected]);
    if (state.formatList != nullptr) {
        SendMessageW(state.formatList, LB_RESETCONTENT, 0, 0);
        int preferredIndex = 0;
        for (int i = 0; i < static_cast<int>(state.cameraFormats.size()); ++i) {
            const auto& format = state.cameraFormats[i];
            SendMessageW(state.formatList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format.label.c_str()));
            const UINT32 fps = format.fpsDenominator == 0 ? 0 : static_cast<UINT32>((static_cast<double>(format.fpsNumerator) / format.fpsDenominator) + 0.5);
            if (format.width == 1920 && format.height == 1080 && fps == 24) {
                preferredIndex = i;
            }
        }
        SendMessageW(state.formatList, LB_SETCURSEL, preferredIndex, 0);
    }

    if (state.cameraPreviewEngine != nullptr) {
        state.cameraPreviewEngine->stopPreview();
        const int selectedFormat = state.formatList != nullptr ? static_cast<int>(SendMessageW(state.formatList, LB_GETCURSEL, 0, 0)) : -1;
        const int nativeFormatIndex = selectedFormat >= 0 && selectedFormat < static_cast<int>(state.cameraFormats.size())
            ? state.cameraFormats[selectedFormat].index
            : -1;
        state.cameraPreviewEngine->startPreview(state.cameras[selected], state.preview, nativeFormatIndex, state.fullRange, state.matrix);
    }
}

LRESULT CALLBACK pickerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    PickerState* state = nullptr;

    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<PickerState*>(createStruct->lpCreateParams);
        state->picker = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    } else {
        state = reinterpret_cast<PickerState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (state == nullptr) {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    switch (message) {
    case WM_CTLCOLORSTATIC: {
        const auto deviceContext = reinterpret_cast<HDC>(wParam);
        SetBkMode(deviceContext, TRANSPARENT);
        SetTextColor(deviceContext, ColorText);
        return reinterpret_cast<LRESULT>(pickerBrush());
    }
    case WM_CTLCOLORLISTBOX: {
        const auto deviceContext = reinterpret_cast<HDC>(wParam);
        SetBkColor(deviceContext, ColorBg3);
        SetTextColor(deviceContext, ColorText);
        return reinterpret_cast<LRESULT>(pickerBrush());
    }
    case WM_ERASEBKGND: {
        RECT rect {};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wParam), &rect, pickerBrush());
        return TRUE;
    }
    case WM_COMMAND: {
        const int command = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        const HWND sender = reinterpret_cast<HWND>(lParam);

        if (command == PickerIdOk) {
            state->accepted = true;
            state->done = true;
            return 0;
        }

        if (command == PickerIdCancel) {
            state->accepted = false;
            state->done = true;
            return 0;
        }

        if (sender == state->list && notification == LBN_SELCHANGE) {
            if (state->kind == PickerKind::Window) {
                updateWindowPickerPreview(*state);
            } else {
                updateCameraPickerPreview(*state);
            }
            return 0;
        }
        if (sender == state->formatList && notification == LBN_SELCHANGE && state->kind == PickerKind::Camera) {
            const int selectedCamera = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
            if (selectedCamera >= 0 && selectedCamera < static_cast<int>(state->cameras.size()) && state->cameraPreviewEngine != nullptr) {
                state->cameraPreviewEngine->stopPreview();
                const int selectedFormat = static_cast<int>(SendMessageW(state->formatList, LB_GETCURSEL, 0, 0));
                const int nativeFormatIndex = selectedFormat >= 0 && selectedFormat < static_cast<int>(state->cameraFormats.size())
                    ? state->cameraFormats[selectedFormat].index
                    : -1;
                state->cameraPreviewEngine->startPreview(state->cameras[selectedCamera], state->preview, nativeFormatIndex, state->fullRange, state->matrix);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        state->accepted = false;
        state->done = true;
        return 0;
    case WM_DESTROY:
        state->done = true;
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void registerPickerWindowClass(HINSTANCE instance)
{
    constexpr wchar_t className[] = L"CCStreamerPickerWindow";

    WNDCLASSEXW windowClass {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = pickerWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = pickerBrush();
    windowClass.lpszClassName = className;

    RegisterClassExW(&windowClass);
}

}

MainWindow::MainWindow(HINSTANCE instance)
    : instance_(instance)
    , logger_("MainWindow")
{
}

MainWindow::~MainWindow()
{
    clearWindowPreview();
    clearCameraPreview();
    destroyThemeResources();
}

bool MainWindow::create()
{
    createThemeResources();

    constexpr wchar_t className[] = L"CCStreamerMainWindow";

    WNDCLASSEXW windowClass {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = MainWindow::windowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = bgBrush_;
    windowClass.lpszClassName = className;

    if (!RegisterClassExW(&windowClass)) {
        const auto error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    window_ = CreateWindowExW(
        0,
        className,
        L"CC-Streamer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        nullptr,
        nullptr,
        instance_,
        this);

    return window_ != nullptr;
}

void MainWindow::show(int showCommand)
{
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
}

LRESULT CALLBACK MainWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(createStruct->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->handleMessage(window, message, wParam, lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT MainWindow::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        createControls(window);
        PostMessageW(window, MessageStartupChecks, 0, 0);
        return 0;
    case MessageStartupChecks:
        runStartupChecks();
        return 0;
    case WM_SIZE:
        layoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 1120;
        info->ptMinTrackSize.y = 820;
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        const auto deviceContext = reinterpret_cast<HDC>(wParam);
        SetBkMode(deviceContext, TRANSPARENT);
        SetTextColor(deviceContext, ColorText);
        return reinterpret_cast<LRESULT>(panelBrush_);
    }
    case WM_CTLCOLOREDIT: {
        const auto deviceContext = reinterpret_cast<HDC>(wParam);
        SetBkColor(deviceContext, ColorBg3);
        SetTextColor(deviceContext, ColorText);
        return reinterpret_cast<LRESULT>(inputBrush_);
    }
    case WM_DRAWITEM:
        drawOwnerButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_PAINT: {
        paintBackground(window);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ControlIdStream) {
            if (streamingActive_) {
                stopStreaming();
            } else {
                startStreaming();
            }
            return 0;
        }
        if (LOWORD(wParam) == ControlIdVideoCodec || LOWORD(wParam) == ControlIdHardwareAccel || LOWORD(wParam) == ControlIdColorMode || LOWORD(wParam) == ControlIdColorRange || LOWORD(wParam) == ControlIdColorMatrix) {
            updateBandwidthPrediction();
            return 0;
        }
        if (LOWORD(wParam) == ControlIdSelectWindow) {
            showWindowPicker();
            return 0;
        }
        if (LOWORD(wParam) == ControlIdSelectCamera) {
            showCameraPicker();
            return 0;
        }
        if (LOWORD(wParam) == ControlIdAudio) {
            SetWindowTextW(statusLabel_, L"Status: WASAPI audio capture not implemented yet");
            return 0;
        }
        if (LOWORD(wParam) == ControlIdPrimaryOutput || LOWORD(wParam) == ControlIdBackupOutput) {
            SetWindowTextW(statusLabel_, L"Status: output configuration not implemented yet");
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

void MainWindow::createControls(HWND window)
{
    preview_ = CreateWindowExW(
        0,
        L"STATIC",
        L"PREVIEW CANVAS - VULKAN COMPOSITOR",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        0,
        0,
        100,
        100,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdPreview)),
        instance_,
        nullptr);

    titleLabel_ = createLabel(window, L"CC-STREAMER <LITE>", 0);
    sourceLabel_ = createLabel(window, L"Sources", 0);
    selectWindowButton_ = createButton(window, L"Window Stream", ControlIdSelectWindow);
    selectCameraButton_ = createButton(window, L"Cam Stream", ControlIdSelectCamera);
    audioButton_ = createButton(window, L"System Audio", ControlIdAudio);

    configureLabel_ = createLabel(window, L"Configure", 0);
    primaryEndpointLabel_ = createLabel(window, L"GameSzene SRT endpoint", 0);
    primaryEndpointEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"srt://85.215.253.135:8893?streamid=publish:jericho_game&pkt_size=1316",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,
        0,
        100,
        24,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdPrimaryEndpoint)),
        instance_,
        nullptr);
    backupEndpointLabel_ = createLabel(window, L"CamSzene SRT endpoint", 0);
    backupEndpointEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"srt://85.215.253.135:8892?streamid=publish:headwig_game&pkt_size=1316",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,
        0,
        100,
        24,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdBackupEndpoint)),
        instance_,
        nullptr);

    encoderLabel_ = createLabel(window, L"Encoder", 0);
    videoCodecLabel_ = createLabel(window, L"Video codec", 0);
    videoCodecCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 100, 120, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdVideoCodec)), instance_, nullptr);
    SendMessageW(videoCodecCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"H.264"));
    SendMessageW(videoCodecCombo_, CB_SETCURSEL, 0, 0);

    hardwareAccelLabel_ = createLabel(window, L"Hardware acceleration", 0);
    hardwareAccelCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 100, 120, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdHardwareAccel)), instance_, nullptr);
#if CCSTREAMER_HAS_VULKAN
    SendMessageW(hardwareAccelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Auto - Vulkan present"));
    SendMessageW(hardwareAccelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Software fallback"));
#else
    SendMessageW(hardwareAccelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Software fallback"));
#endif
    SendMessageW(hardwareAccelCombo_, CB_SETCURSEL, 0, 0);

    colorModeLabel_ = createLabel(window, L"Color mode", 0);
    colorModeCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 100, 120, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdColorMode)), instance_, nullptr);
    SendMessageW(colorModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"YUV"));
    SendMessageW(colorModeCombo_, CB_SETCURSEL, 0, 0);

    colorMatrixLabel_ = createLabel(window, L"YUV matrix", 0);
    colorMatrixCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 100, 120, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdColorMatrix)), instance_, nullptr);
    SendMessageW(colorMatrixCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"BT.709"));
    SendMessageW(colorMatrixCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"BT.601"));
    SendMessageW(colorMatrixCombo_, CB_SETCURSEL, 0, 0);

    colorRangeLabel_ = createLabel(window, L"YUV range", 0);
    colorRangeCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 100, 120, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlIdColorRange)), instance_, nullptr);
    SendMessageW(colorRangeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Begrenzt / Limited"));
    SendMessageW(colorRangeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Voll / Full"));
    SendMessageW(colorRangeCombo_, CB_SETCURSEL, 0, 0);

    bandwidthLabel_ = createLabel(window, L"Bandwidth prediction", 0);
    bandwidthValueLabel_ = createLabel(window, L"Estimated: 0 Mbps", 0);

    outputLabel_ = createLabel(window, L"Outputs", 0);
    primaryOutputButton_ = createButton(window, L"Use Window WebRTC", ControlIdPrimaryOutput);
    backupOutputButton_ = createButton(window, L"Use Cam WebRTC", ControlIdBackupOutput);
    streamButton_ = createButton(window, L"Start Stream", ControlIdStream);

#if CCSTREAMER_HAS_VULKAN
    statusLabel_ = createLabel(window, L"Status: ready - Vulkan SDK detected", 0);
#else
    statusLabel_ = createLabel(window, L"Status: ready - Vulkan SDK not detected, fallback required", 0);
#endif

    windowPreviewLabel_ = createLabel(window, L"WINDOW WEBRTC", 0);
    camPreviewLabel_ = createLabel(window, L"CAM WEBRTC", 0);

    RECT clientRect {};
    GetClientRect(window, &clientRect);
    layoutControls(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

    SendMessageW(titleLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
    SendMessageW(preview_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(statusLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(primaryEndpointEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(backupEndpointEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(videoCodecCombo_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(hardwareAccelCombo_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(colorModeCombo_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(colorMatrixCombo_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(colorRangeCombo_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(bandwidthValueLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(windowPreviewLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(camPreviewLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
}

void MainWindow::layoutControls(int width, int height)
{
    if (preview_ == nullptr) {
        return;
    }

    const int margin = 18;
    const int headerHeight = 56;
    const int statusHeight = 34;
    const int sideWidth = 330;
    const int buttonHeight = 34;
    const int contentTop = headerHeight + margin;
    const int previewWidth = width - sideWidth - (margin * 3);
    const int previewHeight = height - headerHeight - statusHeight - (margin * 3);
    const int sideX = margin + previewWidth + margin;
    previewRect_ = { margin + 12, contentTop + 38, margin + 12 + previewWidth - 24, contentTop + 38 + previewHeight - 50 };
    const int gap = 12;
    const int paneWidth = previewRect_.right - previewRect_.left;
    const int paneHeight = paneWidth * 9 / 16;
    const int maxPaneHeight = ((previewRect_.bottom - previewRect_.top) - gap) / 2;
    const int fittedPaneHeight = min(paneHeight, maxPaneHeight);
    windowPreviewRect_ = { previewRect_.left, previewRect_.top, previewRect_.right, previewRect_.top + fittedPaneHeight };
    camPreviewRect_ = { previewRect_.left, windowPreviewRect_.bottom + gap, previewRect_.right, windowPreviewRect_.bottom + gap + fittedPaneHeight };

    MoveWindow(preview_, windowPreviewRect_.left, windowPreviewRect_.top, windowPreviewRect_.right - windowPreviewRect_.left, windowPreviewRect_.bottom - windowPreviewRect_.top, TRUE);
    MoveWindow(windowPreviewLabel_, windowPreviewRect_.left + 10, windowPreviewRect_.top + 8, 160, 22, TRUE);
    MoveWindow(camPreviewLabel_, camPreviewRect_.left + 10, camPreviewRect_.top + 8, 160, 22, TRUE);
    layoutCameraPreview();
    if (windowThumbnail_ != nullptr) {
        SIZE sourceSize {};
        RECT destination = windowPreviewRect_;
        if (SUCCEEDED(DwmQueryThumbnailSourceSize(windowThumbnail_, &sourceSize))) {
            destination = fitRectToAspect(windowPreviewRect_, sourceSize.cx, sourceSize.cy);
        }

        DWM_THUMBNAIL_PROPERTIES properties {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
        properties.rcDestination = destination;
        properties.fVisible = TRUE;
        properties.opacity = 255;
        DwmUpdateThumbnailProperties(windowThumbnail_, &properties);
    }
    MoveWindow(titleLabel_, margin, 12, 420, 32, TRUE);
    MoveWindow(sourceLabel_, sideX + 12, contentTop + 10, sideWidth - 24, 24, TRUE);
    MoveWindow(selectWindowButton_, sideX + 12, contentTop + 44, sideWidth - 24, buttonHeight, TRUE);
    MoveWindow(selectCameraButton_, sideX + 12, contentTop + 86, sideWidth - 24, buttonHeight, TRUE);
    MoveWindow(audioButton_, sideX + 12, contentTop + 128, sideWidth - 24, buttonHeight, TRUE);

    MoveWindow(configureLabel_, sideX + 12, contentTop + 178, sideWidth - 24, 22, TRUE);
    MoveWindow(primaryEndpointLabel_, sideX + 12, contentTop + 204, sideWidth - 24, 20, TRUE);
    MoveWindow(primaryEndpointEdit_, sideX + 12, contentTop + 226, sideWidth - 24, 26, TRUE);
    MoveWindow(backupEndpointLabel_, sideX + 12, contentTop + 258, sideWidth - 24, 20, TRUE);
    MoveWindow(backupEndpointEdit_, sideX + 12, contentTop + 280, sideWidth - 24, 26, TRUE);

    MoveWindow(encoderLabel_, sideX + 12, contentTop + 318, sideWidth - 24, 22, TRUE);
    MoveWindow(videoCodecLabel_, sideX + 12, contentTop + 344, 120, 20, TRUE);
    MoveWindow(videoCodecCombo_, sideX + 150, contentTop + 340, sideWidth - 162, 120, TRUE);
    MoveWindow(hardwareAccelLabel_, sideX + 12, contentTop + 374, 132, 20, TRUE);
    MoveWindow(hardwareAccelCombo_, sideX + 150, contentTop + 370, sideWidth - 162, 120, TRUE);
    MoveWindow(colorModeLabel_, sideX + 12, contentTop + 404, 120, 20, TRUE);
    MoveWindow(colorModeCombo_, sideX + 150, contentTop + 400, sideWidth - 162, 120, TRUE);
    MoveWindow(colorMatrixLabel_, sideX + 12, contentTop + 434, 120, 20, TRUE);
    MoveWindow(colorMatrixCombo_, sideX + 150, contentTop + 430, sideWidth - 162, 120, TRUE);
    MoveWindow(colorRangeLabel_, sideX + 12, contentTop + 464, 120, 20, TRUE);
    MoveWindow(colorRangeCombo_, sideX + 150, contentTop + 460, sideWidth - 162, 120, TRUE);

    MoveWindow(bandwidthLabel_, sideX + 12, contentTop + 500, sideWidth - 24, 20, TRUE);
    MoveWindow(bandwidthValueLabel_, sideX + 12, contentTop + 524, sideWidth - 24, 24, TRUE);

    MoveWindow(outputLabel_, sideX + 12, contentTop + 558, sideWidth - 24, 22, TRUE);
    ShowWindow(primaryOutputButton_, SW_HIDE);
    ShowWindow(backupOutputButton_, SW_HIDE);
    const int streamTop = height - statusHeight - margin - buttonHeight - 10;
    MoveWindow(streamButton_, sideX + 12, streamTop, sideWidth - 24, buttonHeight, TRUE);
    MoveWindow(statusLabel_, margin + 12, height - statusHeight - margin + 7, width - (margin * 2) - 24, 22, TRUE);
    InvalidateRect(window_, nullptr, TRUE);
    updateBandwidthPrediction();
}

void MainWindow::updateBandwidthPrediction()
{
    if (bandwidthValueLabel_ == nullptr) {
        return;
    }

    const double windowPixels = static_cast<double>(selectedWindowWidth_) * selectedWindowHeight_;
    const double cameraPixels = static_cast<double>(selectedCameraWidth_) * selectedCameraHeight_;
    const double windowMbps = (windowPixels * 30.0 * 0.085) / 1000000.0;
    const double cameraMbps = (cameraPixels * selectedCameraFps_ * 0.070) / 1000000.0;
    const double audioMbps = 0.128;
    const double overheadMbps = 0.25;
    const double total = windowMbps + cameraMbps + audioMbps + overheadMbps;

    wchar_t text[192] {};
    swprintf_s(
        text,
        L"Estimated: %.1f Mbps",
        total);
    SetWindowTextW(bandwidthValueLabel_, text);
}

void MainWindow::createThemeResources()
{
    bgBrush_ = CreateSolidBrush(ColorBg);
    panelBrush_ = CreateSolidBrush(ColorBg2);
    inputBrush_ = CreateSolidBrush(ColorBg3);
    previewBrush_ = CreateSolidBrush(RGB(2, 8, 12));

    titleFont_ = CreateFontW(
        -24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    uiFont_ = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    monoFont_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
}

void MainWindow::destroyThemeResources()
{
    if (titleFont_ != nullptr) DeleteObject(titleFont_);
    if (uiFont_ != nullptr) DeleteObject(uiFont_);
    if (monoFont_ != nullptr) DeleteObject(monoFont_);
    if (bgBrush_ != nullptr) DeleteObject(bgBrush_);
    if (panelBrush_ != nullptr) DeleteObject(panelBrush_);
    if (inputBrush_ != nullptr) DeleteObject(inputBrush_);
    if (previewBrush_ != nullptr) DeleteObject(previewBrush_);
}

void MainWindow::paintBackground(HWND window)
{
    PAINTSTRUCT paint {};
    const HDC deviceContext = BeginPaint(window, &paint);
    RECT clientRect {};
    GetClientRect(window, &clientRect);
    FillRect(deviceContext, &clientRect, bgBrush_);

    RECT header { 0, 0, clientRect.right, 56 };
    FillRect(deviceContext, &header, panelBrush_);
    HPEN borderPen = CreatePen(PS_SOLID, 1, ColorBorder);
    HPEN cyanPen = CreatePen(PS_SOLID, 2, ColorCyan);
    const auto oldPen = SelectObject(deviceContext, borderPen);
    MoveToEx(deviceContext, 0, 55, nullptr);
    LineTo(deviceContext, clientRect.right, 55);
    SelectObject(deviceContext, cyanPen);
    MoveToEx(deviceContext, 0, 55, nullptr);
    LineTo(deviceContext, min(140, clientRect.right), 55);

    const int margin = 18;
    const int sideWidth = 330;
    const int previewWidth = clientRect.right - sideWidth - (margin * 3);
    const int contentTop = 56 + margin;
    const int previewHeight = clientRect.bottom - 56 - 34 - (margin * 3);
    RECT previewPanel { margin, contentTop, margin + previewWidth, contentTop + previewHeight };
    RECT sidePanel { margin + previewWidth + margin, contentTop, clientRect.right - margin, clientRect.bottom - 34 - margin };
    RECT statusPanel { margin, clientRect.bottom - 34 - margin, clientRect.right - margin, clientRect.bottom - margin };

    drawPanel(deviceContext, previewPanel, L"PREVIEW", ColorCyan);
    drawPanel(deviceContext, sidePanel, L"CONTROL", ColorGold);
    drawPanel(deviceContext, statusPanel, L"SYSTEM", ColorGreen);

    SelectObject(deviceContext, oldPen);
    DeleteObject(borderPen);
    DeleteObject(cyanPen);
    EndPaint(window, &paint);
}

void MainWindow::drawPanel(HDC deviceContext, const RECT& rect, const wchar_t* title, COLORREF accent)
{
    FillRect(deviceContext, &rect, panelBrush_);
    HPEN borderPen = CreatePen(PS_SOLID, 1, ColorBorder);
    HPEN accentPen = CreatePen(PS_SOLID, 2, accent);
    const auto oldPen = SelectObject(deviceContext, borderPen);
    const auto oldBrush = SelectObject(deviceContext, GetStockObject(HOLLOW_BRUSH));
    Rectangle(deviceContext, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(deviceContext, accentPen);
    MoveToEx(deviceContext, rect.left + 1, rect.top + 1, nullptr);
    LineTo(deviceContext, min(rect.left + 96, rect.right - 1), rect.top + 1);
    MoveToEx(deviceContext, rect.left + 1, rect.top + 1, nullptr);
    LineTo(deviceContext, rect.left + 1, min(rect.top + 28, rect.bottom - 1));

    SelectObject(deviceContext, monoFont_);
    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, accent);
    RECT titleRect { rect.left + 12, rect.top + 10, rect.right - 12, rect.top + 30 };
    DrawTextW(deviceContext, title, -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(deviceContext, oldBrush);
    SelectObject(deviceContext, oldPen);
    DeleteObject(borderPen);
    DeleteObject(accentPen);
}

void MainWindow::drawOwnerButton(const DRAWITEMSTRUCT& item)
{
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    COLORREF accent = ColorCyan;
    if (item.CtlID == ControlIdStream) {
        accent = ColorGreen;
    } else if (item.CtlID == ControlIdBackupOutput || item.CtlID == ControlIdPrimaryOutput) {
        accent = ColorGold;
    }

    HBRUSH fill = CreateSolidBrush(pressed ? ColorBg3 : ColorBg);
    FillRect(item.hDC, &item.rcItem, fill);
    DeleteObject(fill);

    HPEN borderPen = CreatePen(PS_SOLID, focused ? 2 : 1, accent);
    const auto oldPen = SelectObject(item.hDC, borderPen);
    const auto oldBrush = SelectObject(item.hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom);

    wchar_t text[128] {};
    GetWindowTextW(item.hwndItem, text, 128);
    SelectObject(item.hDC, monoFont_);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, accent);
    RECT textRect = item.rcItem;
    DrawTextW(item.hDC, text, -1, &textRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    SelectObject(item.hDC, oldBrush);
    SelectObject(item.hDC, oldPen);
    DeleteObject(borderPen);
}

RECT MainWindow::fitRectToAspect(const RECT& bounds, int aspectWidth, int aspectHeight) const
{
    const int boundsWidth = bounds.right - bounds.left;
    const int boundsHeight = bounds.bottom - bounds.top;
    if (boundsWidth <= 0 || boundsHeight <= 0 || aspectWidth <= 0 || aspectHeight <= 0) {
        return bounds;
    }

    int width = boundsWidth;
    int height = (width * aspectHeight) / aspectWidth;

    if (height > boundsHeight) {
        height = boundsHeight;
        width = (height * aspectWidth) / aspectHeight;
    }

    const int left = bounds.left + ((boundsWidth - width) / 2);
    const int top = bounds.top + ((boundsHeight - height) / 2);
    return { left, top, left + width, top + height };
}

void MainWindow::layoutCameraPreview()
{
    if (cameraPreview_ == nullptr) {
        return;
    }

    const RECT fitted = fitRectToAspect(camPreviewRect_, 16, 9);
    MoveWindow(cameraPreview_, fitted.left, fitted.top, fitted.right - fitted.left, fitted.bottom - fitted.top, TRUE);
}

bool MainWindow::isFullRangeSelected() const
{
    if (colorRangeCombo_ == nullptr) {
        return false;
    }

    return SendMessageW(colorRangeCombo_, CB_GETCURSEL, 0, 0) == 1;
}

YuvMatrix MainWindow::selectedYuvMatrix() const
{
    if (colorMatrixCombo_ == nullptr) {
        return YuvMatrix::Bt709;
    }

    return SendMessageW(colorMatrixCombo_, CB_GETCURSEL, 0, 0) == 1 ? YuvMatrix::Bt601 : YuvMatrix::Bt709;
}

std::wstring MainWindow::readText(HWND control) const
{
    if (control == nullptr) {
        return {};
    }

    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

bool MainWindow::validateSrtEndpoint(const std::wstring& url) const
{
    if (url.rfind(L"srt://", 0) != 0) {
        return false;
    }

    if (url.find(L"streamid=publish:") == std::wstring::npos) {
        return false;
    }

    if (url.find(L"pkt_size=1316") == std::wstring::npos) {
        return false;
    }

    return true;
}

void MainWindow::startStreaming()
{
    const std::wstring gameUrl = readText(primaryEndpointEdit_);
    const std::wstring camUrl = readText(backupEndpointEdit_);

    if (!validateSrtEndpoint(gameUrl)) {
        SetWindowTextW(statusLabel_, L"Status: invalid GameSzene SRT endpoint");
        logger_.error(L"Invalid GameSzene SRT endpoint: " + gameUrl);
        return;
    }

    if (!validateSrtEndpoint(camUrl)) {
        SetWindowTextW(statusLabel_, L"Status: invalid CamSzene SRT endpoint");
        logger_.error(L"Invalid CamSzene SRT endpoint: " + camUrl);
        return;
    }

    StreamSelection selection;
    selection.windowTitle = selectedWindowTitle_;
    selection.windowHandle = selectedWindowHandle_;
    selection.cameraName = L"__camera_pipe__";
    selection.cameraWidth = selectedCameraWidth_;
    selection.cameraHeight = selectedCameraHeight_;
    selection.cameraFps = selectedCameraFps_;
    selection.cameraFromPreviewRegion = false;

    StreamEndpoints endpoints;
    endpoints.gameSrtUrl = gameUrl;
    endpoints.camSrtUrl = camUrl;

    SetWindowTextW(statusLabel_, L"Status: starting FFmpeg publishers...");

    std::wstring error;
    if (!publisher_.start(selection, endpoints, error)) {
        const std::wstring status = L"Status: " + error;
        SetWindowTextW(statusLabel_, status.c_str());
        logger_.error(status);
        return;
    }

    if (!publisher_.startCameraPipe(selectedCameraWidth_, selectedCameraHeight_, selectedCameraFps_, camUrl, error)) {
        publisher_.stop();
        const std::wstring status = L"Status: " + error;
        SetWindowTextW(statusLabel_, status.c_str());
        logger_.error(status);
        return;
    }

    mediaFoundationCamera_.setFrameCallback([this](const BYTE* bgra, LONG width, LONG height) {
        publisher_.submitCameraFrame(bgra, width, height);
    });

    streamingActive_ = true;
    setStreamingControlsEnabled(false);
    SetWindowTextW(streamButton_, L"Stop Stream");
    const std::wstring status = publisher_.gameCaptureUsedFallback()
        ? L"Status: streaming to GameSzene and CamSzene via FFmpeg/SRT (gfxcapture failed, using gdigrab fallback)"
        : L"Status: streaming to GameSzene and CamSzene via FFmpeg/SRT";
    SetWindowTextW(statusLabel_, status.c_str());
    logger_.info(status);
}

void MainWindow::stopStreaming()
{
    mediaFoundationCamera_.setFrameCallback(nullptr);
    publisher_.stop();
    streamingActive_ = false;
    setStreamingControlsEnabled(true);
    SetWindowTextW(streamButton_, L"Start Stream");
    SetWindowTextW(statusLabel_, L"Status: streaming stopped");
    logger_.info("Streaming stopped");
}

void MainWindow::setStreamingControlsEnabled(bool enabled)
{
    EnableWindow(primaryEndpointEdit_, enabled);
    EnableWindow(backupEndpointEdit_, enabled);
    EnableWindow(videoCodecCombo_, enabled);
    EnableWindow(hardwareAccelCombo_, enabled);
    EnableWindow(colorModeCombo_, enabled);
    EnableWindow(colorMatrixCombo_, enabled);
    EnableWindow(colorRangeCombo_, enabled);
    EnableWindow(selectWindowButton_, enabled);
    EnableWindow(selectCameraButton_, enabled);
}

void MainWindow::runStartupChecks()
{
    if (startupChecksRan_) {
        logger_.warning("Startup checks already ran; skipping duplicate request");
        return;
    }
    startupChecksRan_ = true;

    if (publisher_.available()) {
        logger_.info("FFmpeg available for publisher backend");
        SetWindowTextW(statusLabel_, L"Status: FFmpeg ready - streaming backend available");
        return;
    }

    SetWindowTextW(statusLabel_, L"Status: FFmpeg missing - download required for streaming");
    std::wstring error;
    if (publisher_.installWithPrompt(window_, error)) {
        SetWindowTextW(statusLabel_, L"Status: FFmpeg installed - streaming backend ready");
    } else {
        const std::wstring status = L"Status: " + error;
        SetWindowTextW(statusLabel_, status.c_str());
    }
}

void MainWindow::showWindowPicker()
{
    if (pickerOpen_) {
        SetWindowTextW(statusLabel_, L"Status: picker already open");
        return;
    }
    pickerOpen_ = true;

    PickerState state;
    state.kind = PickerKind::Window;
    state.windows = enumerateWindows(window_);

    if (state.windows.empty()) {
        MessageBoxW(window_, L"No visible windows found.", L"CC-Streamer", MB_OK | MB_ICONINFORMATION);
        logger_.warning("Window picker found no visible windows");
        pickerOpen_ = false;
        return;
    }

    registerPickerWindowClass(instance_);
    const HWND picker = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"CCStreamerPickerWindow",
        L"Select Window",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        760,
        460,
        window_,
        nullptr,
        instance_,
        &state);
    if (picker == nullptr) {
        pickerOpen_ = false;
        return;
    }

    state.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 16, 16, 300, 360, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdList)), instance_, nullptr);
    state.preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Window Preview", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 332, 16, 400, 360, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdPreview)), instance_, nullptr);
    const HWND ok = CreateWindowExW(0, L"BUTTON", L"Use Window", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 532, 392, 96, 32, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdOk)), instance_, nullptr);
    const HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 636, 392, 96, 32, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdCancel)), instance_, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);

    for (const auto& candidate : state.windows) {
        SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(candidate.title.c_str()));
    }
    SendMessageW(state.list, LB_SETCURSEL, 0, 0);
    updateWindowPickerPreview(state);

    EnableWindow(window_, FALSE);
    ShowWindow(picker, SW_SHOWNORMAL);
    UpdateWindow(picker);

    while (!state.done) {
        MSG message {};
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
    if (state.thumbnail != nullptr) {
        DwmUnregisterThumbnail(state.thumbnail);
        state.thumbnail = nullptr;
    }
    DestroyWindow(picker);
    EnableWindow(window_, TRUE);
    SetForegroundWindow(window_);

    if (state.accepted && selected >= 0 && selected < static_cast<int>(state.windows.size())) {
        setWindowPreview(state.windows[selected].window);
        const std::wstring status = L"Window Stream: " + state.windows[selected].title;
        SetWindowTextW(statusLabel_, status.c_str());
    }

    pickerOpen_ = false;
}

void MainWindow::showCameraPicker()
{
    if (pickerOpen_) {
        SetWindowTextW(statusLabel_, L"Status: picker already open");
        return;
    }
    pickerOpen_ = true;

    PickerState state;
    state.kind = PickerKind::Camera;
    state.cameras = MediaFoundationCamera::enumerateDevices();
    state.cameraPreviewEngine = &pickerCameraPreview_;
    state.fullRange = isFullRangeSelected();
    state.matrix = selectedYuvMatrix();

    if (state.cameras.empty()) {
        MessageBoxW(window_, L"No Media Foundation camera devices found.", L"CC-Streamer", MB_OK | MB_ICONINFORMATION);
        logger_.warning("Camera picker found no Media Foundation camera devices");
        pickerOpen_ = false;
        return;
    }

    registerPickerWindowClass(instance_);
    const HWND picker = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CCStreamerPickerWindow", L"Select Camera", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 760, 460, window_, nullptr, instance_, &state);
    if (picker == nullptr) {
        pickerOpen_ = false;
        return;
    }

    state.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 16, 16, 300, 160, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdList)), instance_, nullptr);
    state.formatList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 16, 190, 300, 186, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdFormatList)), instance_, nullptr);
    state.preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Select a Media Foundation camera", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 332, 16, 400, 360, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdPreview)), instance_, nullptr);
    const HWND ok = CreateWindowExW(0, L"BUTTON", L"Use Camera", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 532, 392, 96, 32, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdOk)), instance_, nullptr);
    const HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 636, 392, 96, 32, picker, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PickerIdCancel)), instance_, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);

    for (const auto& candidate : state.cameras) {
        SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(candidate.name.c_str()));
    }
    SendMessageW(state.list, LB_SETCURSEL, 0, 0);
    updateCameraPickerPreview(state);

    EnableWindow(window_, FALSE);
    ShowWindow(picker, SW_SHOWNORMAL);
    UpdateWindow(picker);

    while (!state.done) {
        MSG message {};
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
    const int selectedFormat = state.formatList != nullptr ? static_cast<int>(SendMessageW(state.formatList, LB_GETCURSEL, 0, 0)) : -1;
    const int nativeFormatIndex = selectedFormat >= 0 && selectedFormat < static_cast<int>(state.cameraFormats.size())
        ? state.cameraFormats[selectedFormat].index
        : -1;
    pickerCameraPreview_.stopPreview();
    DestroyWindow(picker);
    EnableWindow(window_, TRUE);
    SetForegroundWindow(window_);

    if (state.accepted && selected >= 0 && selected < static_cast<int>(state.cameras.size())) {
        selectedCameraName_ = state.cameras[selected].name;
        if (selectedFormat >= 0 && selectedFormat < static_cast<int>(state.cameraFormats.size())) {
            selectedCameraWidth_ = static_cast<int>(state.cameraFormats[selectedFormat].width);
            selectedCameraHeight_ = static_cast<int>(state.cameraFormats[selectedFormat].height);
            selectedCameraFps_ = state.cameraFormats[selectedFormat].fpsDenominator == 0
                ? 24
                : static_cast<int>((static_cast<double>(state.cameraFormats[selectedFormat].fpsNumerator) / state.cameraFormats[selectedFormat].fpsDenominator) + 0.5);
        }
        setCameraPreview(state.cameras[selected], nativeFormatIndex);
        const std::wstring matrix = selectedYuvMatrix() == YuvMatrix::Bt601 ? L"BT.601" : L"BT.709";
        const std::wstring status = L"Cam Stream: " + state.cameras[selected].name + (isFullRangeSelected() ? L" - YUV full - " : L" - YUV limited - ") + matrix;
        SetWindowTextW(statusLabel_, status.c_str());
        updateBandwidthPrediction();
    }

    pickerOpen_ = false;
}

void MainWindow::setWindowPreview(HWND sourceWindow)
{
    clearWindowPreview();
    selectedWindowHandle_ = sourceWindow;
    wchar_t title[256] {};
    GetWindowTextW(sourceWindow, title, 256);
    selectedWindowTitle_ = title;
    RECT sourceRect {};
    if (GetWindowRect(sourceWindow, &sourceRect)) {
        selectedWindowWidth_ = max(1, sourceRect.right - sourceRect.left);
        selectedWindowHeight_ = max(1, sourceRect.bottom - sourceRect.top);
    }
    ShowWindow(preview_, SW_HIDE);
    SetWindowTextW(preview_, L"");

    if (SUCCEEDED(DwmRegisterThumbnail(window_, sourceWindow, &windowThumbnail_))) {
        SIZE sourceSize {};
        RECT destination = windowPreviewRect_;
        if (SUCCEEDED(DwmQueryThumbnailSourceSize(windowThumbnail_, &sourceSize))) {
            destination = fitRectToAspect(windowPreviewRect_, sourceSize.cx, sourceSize.cy);
        }

        DWM_THUMBNAIL_PROPERTIES properties {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
        properties.rcDestination = destination;
        properties.fVisible = TRUE;
        properties.opacity = 255;
        DwmUpdateThumbnailProperties(windowThumbnail_, &properties);
    } else {
        SetWindowTextW(statusLabel_, L"Status: failed to create window preview thumbnail");
        logger_.error("Failed to create DWM window preview thumbnail");
    }

    updateBandwidthPrediction();
}

void MainWindow::setCameraPreview(const CameraDevice& camera, int formatIndex)
{
    clearCameraPreview();

    const RECT fitted = fitRectToAspect(camPreviewRect_, 16, 9);
    cameraPreview_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        fitted.left,
        fitted.top,
        fitted.right - fitted.left,
        fitted.bottom - fitted.top,
        window_,
        reinterpret_cast<HMENU>(300),
        instance_,
        nullptr);

    if (cameraPreview_ == nullptr || camera.symbolicLink.empty() || !mediaFoundationCamera_.startPreview(camera, cameraPreview_, formatIndex, isFullRangeSelected(), selectedYuvMatrix())) {
        if (cameraPreview_ != nullptr) {
            DestroyWindow(cameraPreview_);
            cameraPreview_ = nullptr;
        }
        const std::wstring error = mediaFoundationCamera_.lastError();
        const std::wstring status = error.empty() ? L"Status: failed to start camera preview" : L"Status: " + error;
        SetWindowTextW(statusLabel_, status.c_str());
        logger_.error(status);
    } else {
        SetWindowTextW(cameraPreview_, L"Media Foundation preview starting...");
        logger_.info(L"Camera preview starting: " + camera.name);
    }
}

void MainWindow::clearWindowPreview()
{
    if (windowThumbnail_ != nullptr) {
        DwmUnregisterThumbnail(windowThumbnail_);
        windowThumbnail_ = nullptr;
    }

    if (preview_ != nullptr) {
        ShowWindow(preview_, SW_SHOW);
    }
}

void MainWindow::clearCameraPreview()
{
    mediaFoundationCamera_.stopPreview();

    if (cameraPreview_ != nullptr) {
        DestroyWindow(cameraPreview_);
        cameraPreview_ = nullptr;
    }
}

HWND MainWindow::createLabel(HWND parent, const wchar_t* text, int id)
{
    const HWND label = CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0,
        0,
        100,
        24,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance_,
        nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    return label;
}

HWND MainWindow::createButton(HWND parent, const wchar_t* text, int id)
{
    const HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0,
        0,
        100,
        32,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance_,
        nullptr);
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    return button;
}

} // namespace ccstreamer
