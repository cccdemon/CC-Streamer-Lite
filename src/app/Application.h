#pragma once

#include "app/MainWindow.h"
#include "logging/Logger.h"

#include <memory>

namespace ccstreamer {

class Application {
public:
    Application();

    void start();

private:
    Logger logger_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace ccstreamer

