#pragma once

#include <QMainWindow>

namespace ccstreamer {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace ccstreamer

