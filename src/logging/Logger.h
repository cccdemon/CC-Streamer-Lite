#pragma once

#include <QString>
#include <QStringList>

namespace ccstreamer {

class Logger {
public:
    explicit Logger(QString category);

    void info(const QString& message) const;
    void warning(const QString& message) const;
    void error(const QString& message) const;

    QString redact(const QString& message) const;

private:
    QString category_;
    QStringList sensitiveKeys_;
};

} // namespace ccstreamer

