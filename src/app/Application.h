#pragma once

#include "app/MainWindow.h"
#include "logging/Logger.h"

#include <windows.h>

#include <memory>

namespace ccstreamer {

class Application {
public:
    Application(HINSTANCE instance, int showCommand);

    int run();

private:
    HINSTANCE instance_;
    int showCommand_;
    Logger logger_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace ccstreamer
