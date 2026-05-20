#include "app/Application.h"
#include "logging/Logger.h"

#include <exception>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand)
{
    ccstreamer::Logger logger("CCStreamer");
    int result = 1;

    try {
        ccstreamer::Application app(instance, showCommand);
        result = app.run();
    } catch (const std::exception& exception) {
        logger.error(std::string("Unhandled exception: ") + exception.what());
        MessageBoxW(nullptr, L"CC-Streamer crashed. Check CCStreamer.log.", L"CC-Streamer", MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        logger.error("Unhandled non-standard exception");
        MessageBoxW(nullptr, L"CC-Streamer crashed. Check CCStreamer.log.", L"CC-Streamer", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (result != 0) {
        MessageBoxW(
            nullptr,
            L"CC-Streamer failed to start. Check the build output and local debug logs.",
            L"CC-Streamer",
            MB_OK | MB_ICONERROR);
    }

    return result;
}
