#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <string>
#include <vector>

enum class LogLevel { Info, Warn, Alert, Error };

// Appends timestamped lines to a log file, e.g.
// 2026-10-04 18:05:12 [ALERT] Zone CAUTION -> DANGER at 79 cm
class Logger {
public:
    explicit Logger(const std::string& path);

    void log(LogLevel level, const std::string& msg);
    std::vector<std::string> tail(std::size_t lines) const;
    const std::string& path() const { return path_; }

    static std::string levelName(LogLevel level);

private:
    std::string path_;
    std::ofstream out_;
};

#endif // LOGGER_HPP
