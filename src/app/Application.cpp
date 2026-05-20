#include "app/Application.h"

namespace ccstreamer {

Application::Application()
    : logger_("CCStreamer")
{
}

void Application::start()
{
    logger_.info("Starting CC-Streamer");
    mainWindow_ = std::make_unique<MainWindow>();
    mainWindow_->show();
}

} // namespace ccstreamer

