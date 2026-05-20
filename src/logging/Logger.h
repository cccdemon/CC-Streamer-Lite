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

    std::string redact(const std::string& message) const;

private:
    std::string category_;
    std::vector<std::string> sensitiveKeys_;
};

} // namespace ccstreamer
