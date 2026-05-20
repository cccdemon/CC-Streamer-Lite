#include "streaming/FfmpegPublisher.h"

#include <commctrl.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <sstream>
#include <urlmon.h>
#include <vector>

namespace ccstreamer {

namespace {

std::wstring sizeArg(int width, int height)
{
    std::wstringstream stream;
    stream << width << L"x" << height;
    return stream.str();
}

std::wstring quoteArgument(const std::wstring& value)
{
    std::wstring result = L"\"";
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            result += L"\\\"";
        } else {
            result += ch;
        }
    }
    result += L"\"";
    return result;
}

std::wstring ensureSrtBuffer(const std::wstring& url)
{
    if (url.find(L"sndbuf=") != std::wstring::npos) {
        return url;
    }

    const std::wstring bufferOption = L"sndbuf=2000000";
    if (url.find(L'?') == std::wstring::npos) {
        return url + L"?" + bufferOption;
    }

    return url + L"&" + bufferOption;
}

std::wstring hwndInputName(HWND window)
{
    std::wstringstream stream;
    stream << reinterpret_cast<uintptr_t>(window);
    return stream.str();
}

std::wstring gfxCaptureFilter(HWND window, int fps)
{
    std::wstringstream stream;
    stream
        << L"gfxcapture=hwnd=" << hwndInputName(window)
        << L":max_framerate=" << fps
        << L":capture_cursor=false"
        << L":capture_border=false"
        << L":display_border=false"
        << L":resize_mode=scale_aspect"
        << L":output_fmt=bgra";
    return stream.str();
}

// NOTE: ddagrab is a desktop-region capture fallback. For explicit window
// capture of a selected HWND we must use Windows Graphics Capture (gfxcapture).
std::wstring ddaGrabFilter(HWND window, int fps)
{
    RECT rect {};
    GetWindowRect(window, &rect);
    int width = max(2, rect.right - rect.left);
    int height = max(2, rect.bottom - rect.top);
    if ((width % 2) != 0) {
        --width;
    }
    if ((height % 2) != 0) {
        --height;
    }

    std::wstringstream stream;
    stream
        << L"ddagrab=framerate=" << fps
        << L":draw_mouse=false"
        << L":offset_x=" << rect.left
        << L":offset_y=" << rect.top
        << L":video_size=" << width << L"x" << height
        << L":output_fmt=bgra"
        << L",hwdownload,format=bgra";
    return stream.str();
}

class InstallProgressWindow {
public:
    explicit InstallProgressWindow(HWND owner)
        : owner_(owner)
    {
        INITCOMMONCONTROLSEX controls {};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&controls);

        window_ = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            L"STATIC",
            L"CC-Streamer - FFmpeg setup",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            560,
            170,
            owner_,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        if (window_ == nullptr) {
            return;
        }

        message_ = CreateWindowExW(
            0,
            L"STATIC",
            L"Preparing FFmpeg setup...",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20,
            22,
            500,
            28,
            window_,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        progress_ = CreateWindowExW(
            0,
            PROGRESS_CLASSW,
            nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20,
            62,
            500,
            24,
            window_,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        detail_ = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20,
            98,
            500,
            22,
            window_,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        SendMessageW(progress_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        ShowWindow(window_, SW_SHOWNORMAL);
        UpdateWindow(window_);
        pump();
    }

    ~InstallProgressWindow()
    {
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
    }

    void setStep(const std::wstring& message, const std::wstring& detail, int percent)
    {
        if (window_ == nullptr) {
            return;
        }

        SetWindowTextW(message_, message.c_str());
        SetWindowTextW(detail_, detail.c_str());
        SendMessageW(progress_, PBM_SETPOS, percent, 0);
        UpdateWindow(window_);
        pump();
    }

    void pulse(int percent)
    {
        if (progress_ != nullptr) {
            SendMessageW(progress_, PBM_SETPOS, percent, 0);
        }
        pump();
    }

private:
    void pump()
    {
        MSG message {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (owner_ != nullptr && !IsDialogMessageW(owner_, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }

    HWND owner_ = nullptr;
    HWND window_ = nullptr;
    HWND message_ = nullptr;
    HWND progress_ = nullptr;
    HWND detail_ = nullptr;
};

class DownloadProgress final : public IBindStatusCallback {
public:
    DownloadProgress(InstallProgressWindow& progress, int startPercent, int endPercent)
        : progress_(progress)
        , startPercent_(startPercent)
        , endPercent_(endPercent)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (object == nullptr) {
            return E_POINTER;
        }

        if (iid == IID_IUnknown || iid == IID_IBindStatusCallback) {
            *object = static_cast<IBindStatusCallback*>(this);
            AddRef();
            return S_OK;
        }

        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 2;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD, IBinding*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPriority(LONG*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*, BINDINFO*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID, IUnknown*) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnProgress(ULONG progress, ULONG progressMax, ULONG, LPCWSTR statusText) override
    {
        int percent = startPercent_;
        if (progressMax > 0) {
            const double fraction = static_cast<double>(progress) / static_cast<double>(progressMax);
            percent = startPercent_ + static_cast<int>((endPercent_ - startPercent_) * fraction);
        }

        progress_.setStep(
            L"Downloading FFmpeg essentials",
            statusText != nullptr ? statusText : L"Receiving archive bytes...",
            percent);
        return S_OK;
    }

private:
    InstallProgressWindow& progress_;
    int startPercent_ = 0;
    int endPercent_ = 100;
};

bool downloadFile(const std::wstring& url, const std::wstring& target, InstallProgressWindow& progress, std::wstring& error)
{
    DownloadProgress callback(progress, 12, 65);
    const HRESULT result = URLDownloadToFileW(nullptr, url.c_str(), target.c_str(), 0, &callback);
    if (FAILED(result)) {
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"FFmpeg download failed: 0x%08X", static_cast<unsigned int>(result));
        error = buffer;
        return false;
    }

    return true;
}

bool runTarExtract(const std::wstring& zipPath, const std::wstring& extractDir, InstallProgressWindow& progress, std::wstring& error)
{
    std::wstring commandLine = L"tar.exe -xf " + quoteArgument(zipPath) + L" -C " + quoteArgument(extractDir);
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        const DWORD lastError = GetLastError();
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"Failed to start tar.exe extractor: Win32 error %lu", lastError);
        error = buffer;
        return false;
    }

    int percent = 68;
    while (WaitForSingleObject(process.hProcess, 150) == WAIT_TIMEOUT) {
        percent = percent >= 84 ? 68 : percent + 1;
        progress.pulse(percent);
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    progress.pulse(85);
    if (exitCode != 0) {
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"FFmpeg archive extraction failed: tar.exe exit code %lu", exitCode);
        error = buffer;
        return false;
    }

    return true;
}

}

FfmpegPublisher::FfmpegPublisher()
    : logger_("FfmpegPublisher")
{
}

FfmpegPublisher::~FfmpegPublisher()
{
    stop();
}

bool FfmpegPublisher::gameCaptureUsedFallback() const
{
    return gameCaptureUsedFallback_;
}

bool FfmpegPublisher::start(const StreamSelection& selection, const StreamEndpoints& endpoints, std::wstring& error)
{
    gameCaptureUsedFallback_ = false;
    try {
        if (running()) {
            error = L"Streaming is already running";
            return false;
        }

        ffmpegPath_ = findFfmpeg();
        if (ffmpegPath_.empty()) {
            error = L"ffmpeg.exe not found. Install FFmpeg or copy ffmpeg.exe next to CCStreamer.exe.";
            logger_.error(error);
            return false;
        }

        if (selection.windowHandle == nullptr && selection.windowTitle.empty()) {
            error = L"No window selected for GameSzene stream";
            return false;
        }

        const bool cameraPipeOnly = selection.cameraName == L"__camera_pipe__";
        if (!cameraPipeOnly && !selection.cameraFromPreviewRegion && selection.cameraName.empty()) {
            error = L"No camera selected for CamSzene stream";
            return false;
        }
        if (selection.cameraFromPreviewRegion && (selection.cameraRegionWidth <= 0 || selection.cameraRegionHeight <= 0)) {
            error = L"Camera preview region is not available for CamSzene stream";
            return false;
        }

        const auto startGamePublisher = [&](bool useGfxCapture) {
            std::wstringstream args;
            args
                << L"-hide_banner -nostdin -loglevel warning ";
            if (useGfxCapture) {
                args
                    << L"-f lavfi -i " << quote(gfxCaptureFilter(selection.windowHandle, 30)) << L" "
                    << L"-vf hwdownload,format=bgra ";
            } else {
                args
                    << L"-f gdigrab -framerate 30 -draw_mouse 0 -i " << quote(L"title=" + selection.windowTitle) << L" ";
            }
            args
                << L"-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
                << L"-f mpegts " << quote(ensureSrtBuffer(endpoints.gameSrtUrl));

            return startProcess(L"GameSzene", args.str(), gameProcess_, error);
        };

        bool triedGfxCapture = selection.windowHandle != nullptr;
        if (triedGfxCapture) {
            if (!startGamePublisher(true)) {
                stop();
                return false;
            }

            if (!verifyProcessRunning(L"GameSzene", gameProcess_, error)) {
                if (!selection.windowTitle.empty()) {
                    logger_.warning("GameSzene gfxcapture failed, falling back to gdigrab title capture");
                    gameCaptureUsedFallback_ = true;
                    stopProcess(gameProcess_);
                    if (!startGamePublisher(false)) {
                        stop();
                        return false;
                    }
                    if (!verifyProcessRunning(L"GameSzene", gameProcess_, error)) {
                        stop();
                        return false;
                    }
                } else {
                    stop();
                    return false;
                }
            }
        } else {
            if (!startGamePublisher(false)) {
                stop();
                return false;
            }
            if (!verifyProcessRunning(L"GameSzene", gameProcess_, error)) {
                stop();
                return false;
            }
        }

        if (!cameraPipeOnly) {
            std::wstringstream camArgs;
            camArgs << L"-hide_banner -nostdin -loglevel warning ";
            if (selection.cameraFromPreviewRegion) {
                camArgs
                    << L"-f gdigrab -framerate " << selection.cameraFps
                    << L" -offset_x " << selection.cameraRegionX
                    << L" -offset_y " << selection.cameraRegionY
                    << L" -video_size " << sizeArg(selection.cameraRegionWidth, selection.cameraRegionHeight)
                    << L" -i desktop ";
            } else {
                camArgs
                    << L"-f dshow -video_size " << sizeArg(selection.cameraWidth, selection.cameraHeight)
                    << L" -framerate " << selection.cameraFps
                    << L" -i " << quote(L"video=" + selection.cameraName) << L" ";
            }
            camArgs
                << L"-vf scale=" << selection.cameraWidth << L":" << selection.cameraHeight
                << L" -c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
                << L"-f mpegts " << quote(ensureSrtBuffer(endpoints.camSrtUrl));

            if (!startProcess(L"CamSzene", camArgs.str(), camProcess_, error)) {
                stop();
                return false;
            }

            if (!verifyProcessRunning(L"CamSzene", camProcess_, error)) {
                stop();
                return false;
            }
        }

        logger_.info(L"Started FFmpeg SRT publishers");
        return true;
    } catch (const std::exception& exception) {
        stop();
        error = L"Publisher backend failed before startup";
        logger_.error(std::string("Publisher exception: ") + exception.what());
        return false;
    }
}

void FfmpegPublisher::stop()
{
    stopProcess(gameProcess_);
    stopProcess(camProcess_);
    if (camInputWrite_ != nullptr) {
        CloseHandle(camInputWrite_);
        camInputWrite_ = nullptr;
    }
}

bool FfmpegPublisher::running() const
{
    return processRunning(gameProcess_) || processRunning(camProcess_);
}

bool FfmpegPublisher::startCameraPipe(int width, int height, int fps, const std::wstring& camSrtUrl, std::wstring& error)
{
    ffmpegPath_ = findFfmpeg();
    if (ffmpegPath_.empty()) {
        error = L"ffmpeg.exe not found. Install FFmpeg or copy ffmpeg.exe next to CCStreamer.exe.";
        logger_.error(error);
        return false;
    }

    if (processRunning(camProcess_)) {
        error = L"CamSzene publisher is already running";
        return false;
    }

    SECURITY_ATTRIBUTES security {};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE pipeRead = nullptr;
    HANDLE pipeWrite = nullptr;
    if (!CreatePipe(&pipeRead, &pipeWrite, &security, 0)) {
        const DWORD lastError = GetLastError();
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"Failed to create camera frame pipe: Win32 error %lu", lastError);
        error = buffer;
        logger_.error(error);
        return false;
    }

    SetHandleInformation(pipeWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = pipeRead;

    const std::wstring logPath = ffmpegLogPath(L"CamSzene");
    HANDLE logFile = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE) {
        CloseHandle(pipeRead);
        CloseHandle(pipeWrite);
        error = L"Failed to open CamSzene FFmpeg log";
        logger_.error(error);
        return false;
    }
    startup.hStdOutput = logFile;
    startup.hStdError = logFile;

    std::wstringstream args;
    args
        << quote(ffmpegPath_)
        << L" -hide_banner -nostdin -loglevel warning"
        << L" -f rawvideo -pix_fmt bgra -video_size " << sizeArg(width, height)
        << L" -framerate " << fps
        << L" -i pipe:0"
        << L" -vf scale=" << width << L":" << height
        << L" -c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p"
        << L" -f mpegts " << quote(ensureSrtBuffer(camSrtUrl));

    std::wstring commandLine = args.str();
    logger_.info(L"Starting CamSzene pipe publisher: " + commandLine);
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &camProcess_)) {
        const DWORD lastError = GetLastError();
        CloseHandle(pipeRead);
        CloseHandle(pipeWrite);
        CloseHandle(logFile);
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"Failed to start CamSzene FFmpeg pipe publisher: Win32 error %lu", lastError);
        error = buffer;
        logger_.error(error);
        return false;
    }

    CloseHandle(pipeRead);
    CloseHandle(logFile);
    camInputWrite_ = pipeWrite;

    if (!verifyProcessRunning(L"CamSzene", camProcess_, error)) {
        stopProcess(camProcess_);
        CloseHandle(camInputWrite_);
        camInputWrite_ = nullptr;
        return false;
    }

    return true;
}

void FfmpegPublisher::submitCameraFrame(const BYTE* bgra, LONG width, LONG height)
{
    if (camInputWrite_ == nullptr || bgra == nullptr || width <= 0 || height <= 0 || !processRunning(camProcess_)) {
        return;
    }

    const DWORD bytesToWrite = static_cast<DWORD>(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    DWORD bytesWritten = 0;
    if (!WriteFile(camInputWrite_, bgra, bytesToWrite, &bytesWritten, nullptr) || bytesWritten != bytesToWrite) {
        logger_.error("Failed to write complete camera frame to FFmpeg pipe");
        CloseHandle(camInputWrite_);
        camInputWrite_ = nullptr;
    }
}

bool FfmpegPublisher::available() const
{
    const std::wstring path = findFfmpeg();
    if (!path.empty()) {
        logger_.info(L"FFmpeg available: " + path);
        return true;
    }

    logger_.warning("FFmpeg not found by local path or PATH lookup");
    return false;
}

bool FfmpegPublisher::installWithPrompt(HWND owner, std::wstring& error)
{
    if (available()) {
        return true;
    }

    const int choice = MessageBoxW(
        owner,
        L"FFmpeg is required for SRT streaming and was not found.\n\nDownload FFmpeg essentials from gyan.dev and install ffmpeg.exe beside CCStreamer.exe?",
        L"CC-Streamer - Download FFmpeg",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);

    if (choice != IDYES) {
        error = L"FFmpeg download skipped";
        return false;
    }

    if (available()) {
        logger_.info("FFmpeg became available before installer started");
        return true;
    }

    const std::wstring appDir = appDirectory();
    if (appDir.empty()) {
        error = L"Could not resolve application directory";
        return false;
    }

    InstallProgressWindow progress(owner);
    logger_.info("Starting FFmpeg download/install sequence");

    const std::filesystem::path workDir = std::filesystem::path(appDir) / L"ffmpeg-download";
    const std::filesystem::path extractDir = workDir / L"extract";
    const std::filesystem::path zipPath = workDir / L"ffmpeg-release-essentials.zip";
    const std::filesystem::path targetPath = std::filesystem::path(appDir) / L"ffmpeg.exe";

    progress.setStep(L"Preparing FFmpeg install folder", L"Creating local download and extraction folders...", 5);
    std::error_code fsError;
    std::filesystem::create_directories(workDir, fsError);
    std::filesystem::remove_all(extractDir, fsError);
    std::filesystem::create_directories(extractDir, fsError);
    if (fsError) {
        error = L"Failed to prepare FFmpeg install folders";
        logger_.error(error);
        return false;
    }
    progress.pulse(10);

    progress.setStep(L"Downloading FFmpeg essentials", L"Source: https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip", 12);
    if (!downloadFile(L"https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip", zipPath.wstring(), progress, error)) {
        logger_.error(error);
        return false;
    }

    progress.setStep(L"Extracting FFmpeg archive", L"Unpacking the downloaded ZIP archive...", 68);
    if (!runTarExtract(zipPath.wstring(), extractDir.wstring(), progress, error)) {
        logger_.error(error);
        return false;
    }

    progress.setStep(L"Installing FFmpeg", L"Copying ffmpeg.exe beside CCStreamer.exe...", 88);
    std::filesystem::path sourceExe;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(extractDir, fsError)) {
        if (!entry.is_regular_file(fsError)) {
            continue;
        }
        if (entry.path().filename() == L"ffmpeg.exe") {
            sourceExe = entry.path();
            break;
        }
    }

    if (sourceExe.empty()) {
        error = L"ffmpeg.exe missing in downloaded archive";
        logger_.error(error);
        return false;
    }

    std::filesystem::copy_file(sourceExe, targetPath, std::filesystem::copy_options::overwrite_existing, fsError);
    if (fsError) {
        error = L"Failed to copy ffmpeg.exe beside CCStreamer.exe";
        logger_.error(error);
        return false;
    }
    progress.pulse(98);

    if (!available()) {
        error = L"FFmpeg install finished, but ffmpeg.exe was not found beside CCStreamer.exe";
        logger_.error(error);
        return false;
    }

    progress.setStep(L"FFmpeg installed", L"Streaming backend is ready.", 100);
    Sleep(600);
    logger_.info("FFmpeg installed beside CCStreamer.exe");
    return true;
}

bool FfmpegPublisher::startProcess(const std::wstring& label, const std::wstring& arguments, PROCESS_INFORMATION& process, std::wstring& error)
{
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;

    SECURITY_ATTRIBUTES security {};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    const std::wstring logPath = ffmpegLogPath(label);
    HANDLE logFile = CreateFileW(
        logPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &security,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (logFile == INVALID_HANDLE_VALUE) {
        const DWORD lastError = GetLastError();
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"Failed to open FFmpeg log file: Win32 error %lu", lastError);
        error = buffer;
        logger_.error(error);
        return false;
    }

    startup.hStdOutput = logFile;
    startup.hStdError = logFile;
    startup.hStdInput = nullptr;

    std::wstring commandLine = quote(ffmpegPath_) + L" " + arguments;
    logger_.info(L"Starting " + label + L" publisher: " + commandLine);
    logger_.info(L"FFmpeg " + label + L" log: " + logPath);

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process)) {
        const DWORD lastError = GetLastError();
        CloseHandle(logFile);
        wchar_t buffer[160] {};
        swprintf_s(buffer, L"Failed to start ffmpeg.exe: Win32 error %lu", lastError);
        error = buffer;
        logger_.error(error);
        return false;
    }

    CloseHandle(logFile);
    return true;
}

bool FfmpegPublisher::verifyProcessRunning(const std::wstring& label, const PROCESS_INFORMATION& process, std::wstring& error) const
{
    const DWORD waitResult = WaitForSingleObject(process.hProcess, 1800);
    if (waitResult == WAIT_TIMEOUT) {
        return true;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    const std::wstring logPath = ffmpegLogPath(label);
    const std::wstring tail = readLogTail(logPath);
    std::wstringstream stream;
    stream << label << L" publisher exited immediately with code " << exitCode;
    if (!tail.empty()) {
        stream << L": " << tail;
    }
    error = stream.str();
    logger_.error(error);
    return false;
}

std::wstring FfmpegPublisher::findFfmpeg() const
{
    const auto localPath = std::filesystem::path(appDirectory()) / L"ffmpeg.exe";
    std::error_code error;
    if (std::filesystem::exists(localPath, error)) {
        logger_.info(L"Found local ffmpeg.exe: " + localPath.wstring());
        return localPath.wstring();
    }
    if (error) {
        logger_.warning(std::string("Local ffmpeg.exe check failed: ") + error.message());
    }

    wchar_t found[MAX_PATH] {};
    const DWORD length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr);
    if (length > 0 && length < MAX_PATH) {
        logger_.info(L"Found ffmpeg.exe on PATH: " + std::wstring(found));
        return found;
    }

    return {};
}

std::wstring FfmpegPublisher::appDirectory() const
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) {
        return {};
    }

    return std::filesystem::path(buffer).parent_path().wstring();
}

std::wstring FfmpegPublisher::ffmpegLogPath(const std::wstring& label) const
{
    return (std::filesystem::path(appDirectory()) / (L"CCStreamer-" + label + L"-ffmpeg.log")).wstring();
}

std::wstring FfmpegPublisher::readLogTail(const std::wstring& path) const
{
    std::ifstream file { std::filesystem::path(path) };
    if (!file.is_open()) {
        return {};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
            if (lines.size() > 4) {
                lines.erase(lines.begin());
            }
        }
    }

    std::string combined;
    for (const auto& item : lines) {
        if (!combined.empty()) {
            combined += " | ";
        }
        combined += item;
    }

    if (combined.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, combined.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return L"see FFmpeg log file";
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, combined.c_str(), -1, wide.data(), size);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::wstring FfmpegPublisher::quote(const std::wstring& value)
{
    std::wstring result = L"\"";
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            result += L"\\\"";
        } else {
            result += ch;
        }
    }
    result += L"\"";
    return result;
}

bool FfmpegPublisher::processRunning(const PROCESS_INFORMATION& process)
{
    if (process.hProcess == nullptr) {
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 0);
    return waitResult == WAIT_TIMEOUT;
}

void FfmpegPublisher::stopProcess(PROCESS_INFORMATION& process)
{
    if (process.hProcess != nullptr) {
        if (processRunning(process)) {
            TerminateProcess(process.hProcess, 0);
            WaitForSingleObject(process.hProcess, 2000);
        }
        CloseHandle(process.hProcess);
        process.hProcess = nullptr;
    }

    if (process.hThread != nullptr) {
        CloseHandle(process.hThread);
        process.hThread = nullptr;
    }
}

} // namespace ccstreamer
