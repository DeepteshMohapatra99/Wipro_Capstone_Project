#include "Logger.hpp"

#include <ctime>
#include <deque>
#include <stdexcept>

Logger::Logger(const std::string& path)
    : path_(path), out_(path, std::ios::app)
{
    if (!out_)
        throw std::runtime_error("Cannot open log file " + path);
}

std::string Logger::levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Alert: return "ALERT";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::log(LogLevel level, const std::string& msg)
{
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);

    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);

    out_ << stamp << " [" << levelName(level) << "] " << msg << '\n';
    out_.flush();
}

std::vector<std::string> Logger::tail(std::size_t lines) const
{
    std::ifstream in(path_);
    std::deque<std::string> last;
    std::string line;

    while (std::getline(in, line)) {
        last.push_back(line);
        if (last.size() > lines)
            last.pop_front();
    }
    return {last.begin(), last.end()};
}
