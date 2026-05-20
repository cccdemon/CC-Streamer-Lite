#pragma once

#include "capture/MediaFoundationCamera.h"

#include <windows.h>
#include <dwmapi.h>

namespace ccstreamer {

class MainWindow final {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();

    bool create();
    void show(int showCommand);

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void createControls(HWND window);
    void layoutControls(int width, int height);
    void createThemeResources();
    void destroyThemeResources();
    void paintBackground(HWND window);
    void drawPanel(HDC deviceContext, const RECT& rect, const wchar_t* title, COLORREF accent);
    void drawOwnerButton(const DRAWITEMSTRUCT& item);
    RECT fitRectToAspect(const RECT& bounds, int aspectWidth, int aspectHeight) const;
    void layoutCameraPreview();
    void showWindowPicker();
    void showCameraPicker();
    void setWindowPreview(HWND sourceWindow);
    void setCameraPreview(const CameraDevice& camera, int formatIndex);
    void clearWindowPreview();
    void clearCameraPreview();
    HWND createLabel(HWND parent, const wchar_t* text, int id);
    HWND createButton(HWND parent, const wchar_t* text, int id);

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND preview_ = nullptr;
    HWND titleLabel_ = nullptr;
    HWND sourceLabel_ = nullptr;
    HWND selectWindowButton_ = nullptr;
    HWND selectCameraButton_ = nullptr;
    HWND audioButton_ = nullptr;
    HWND configureLabel_ = nullptr;
    HWND primaryEndpointLabel_ = nullptr;
    HWND primaryEndpointEdit_ = nullptr;
    HWND backupEndpointLabel_ = nullptr;
    HWND backupEndpointEdit_ = nullptr;
    HWND encoderLabel_ = nullptr;
    HWND videoCodecLabel_ = nullptr;
    HWND videoCodecCombo_ = nullptr;
    HWND hardwareAccelLabel_ = nullptr;
    HWND hardwareAccelCombo_ = nullptr;
    HWND colorModeLabel_ = nullptr;
    HWND colorModeCombo_ = nullptr;
    HWND colorRangeLabel_ = nullptr;
    HWND colorRangeCombo_ = nullptr;
    HWND outputLabel_ = nullptr;
    HWND primaryOutputButton_ = nullptr;
    HWND backupOutputButton_ = nullptr;
    HWND streamButton_ = nullptr;
    HWND statusLabel_ = nullptr;
    HWND windowPreviewLabel_ = nullptr;
    HWND camPreviewLabel_ = nullptr;
    HWND cameraPreview_ = nullptr;
    HTHUMBNAIL windowThumbnail_ = nullptr;
    MediaFoundationCamera pickerCameraPreview_;
    RECT previewRect_ {};
    RECT windowPreviewRect_ {};
    RECT camPreviewRect_ {};
    bool pickerOpen_ = false;
    MediaFoundationCamera mediaFoundationCamera_;
    HFONT titleFont_ = nullptr;
    HFONT uiFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    HBRUSH previewBrush_ = nullptr;
};

} // namespace ccstreamer
