#include "app/Application.h"

namespace ccstreamer {

Application::Application(HINSTANCE instance, int showCommand)
    : instance_(instance)
    , showCommand_(showCommand == SW_HIDE ? SW_SHOWNORMAL : showCommand)
    , logger_("CCStreamer")
{
}

int Application::run()
{
    logger_.info("Starting CC-Streamer");
    mainWindow_ = std::make_unique<MainWindow>(instance_);

    if (!mainWindow_->create()) {
        logger_.error("Failed to create main window");
        return 1;
    }

    mainWindow_->show(showCommand_);

    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

} // namespace ccstreamer
