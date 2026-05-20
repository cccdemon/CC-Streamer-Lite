#include "app/Application.h"

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand)
{
    ccstreamer::Application app(instance, showCommand);
    const int result = app.run();

    if (result != 0) {
        MessageBoxW(
            nullptr,
            L"CC-Streamer failed to start. Check the build output and local debug logs.",
            L"CC-Streamer",
            MB_OK | MB_ICONERROR);
    }

    return result;
}
