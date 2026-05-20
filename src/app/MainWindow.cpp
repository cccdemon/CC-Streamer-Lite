#include "app/MainWindow.h"

#include <QLabel>
#include <QStatusBar>

namespace ccstreamer {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("CC-Streamer");
    resize(1280, 720);

    auto* placeholder = new QLabel("CC-Streamer preview shell", this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

#if CCSTREAMER_HAS_VULKAN
    statusBar()->showMessage("Ready - Vulkan SDK detected");
#else
    statusBar()->showMessage("Ready - Vulkan SDK not detected, fallback path required");
#endif
}

} // namespace ccstreamer

