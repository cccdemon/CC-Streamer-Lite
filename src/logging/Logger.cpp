#include "logging/Logger.h"

#include <QDebug>
#include <QRegularExpression>

#include <utility>

namespace ccstreamer {

Logger::Logger(QString category)
    : category_(std::move(category))
    , sensitiveKeys_({
          "stream_key",
          "srt_passphrase",
          "password",
          "passwd",
          "token",
          "access_token",
          "refresh_token",
          "secret",
      })
{
}

void Logger::info(const QString& message) const
{
    qInfo().noquote() << "[" + category_ + "]" << redact(message);
}

void Logger::warning(const QString& message) const
{
    qWarning().noquote() << "[" + category_ + "]" << redact(message);
}

void Logger::error(const QString& message) const
{
    qCritical().noquote() << "[" + category_ + "]" << redact(message);
}

QString Logger::redact(const QString& message) const
{
    QString result = message;

    for (const auto& key : sensitiveKeys_) {
        const QRegularExpression quotedPattern(
            "(" + QRegularExpression::escape(key) + R"(\s*=\s*["'])([^"']+)(["'])",
            QRegularExpression::CaseInsensitiveOption);
        result.replace(quotedPattern, R"(\1[REDACTED]\3)");

        const QRegularExpression queryPattern(
            "([?&]" + QRegularExpression::escape(key) + R"(=)([^&\s]+))",
            QRegularExpression::CaseInsensitiveOption);
        result.replace(queryPattern, R"(\1[REDACTED])");
    }

    const QRegularExpression userInfoPattern(R"((://)([^:/@\s]+):([^@/\s]+)@)");
    result.replace(userInfoPattern, R"(\1[REDACTED]:[REDACTED]@)");

    return result;
}

} // namespace ccstreamer
