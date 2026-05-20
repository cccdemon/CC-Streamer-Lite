#pragma once

#include <string>
#include <vector>

namespace ccstreamer {

class Logger {
public:
    explicit Logger(std::string category);

    void info(const std::string& message) const;
    void warning(const std::string& message) const;
    void error(const std::string& message) const;
    void info(const std::wstring& message) const;
    void warning(const std::wstring& message) const;
    void error(const std::wstring& message) const;

    std::string redact(const std::string& message) const;
    std::wstring redact(const std::wstring& message) const;

private:
    void write(const std::string& level, const std::string& message) const;
    static std::string narrow(const std::wstring& value);
    static std::string logPath();

    std::string category_;
    std::vector<std::string> sensitiveKeys_;
};

} // namespace ccstreamer
