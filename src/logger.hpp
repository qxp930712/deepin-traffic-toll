#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <cstdio>
#include <ctime>
#include <mutex>

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    SUCCESS,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel get_level() const;

    void trace(const char* format, ...);
    void debug(const char* format, ...);
    void info(const char* format, ...);
    void success(const char* format, ...);
    void warning(const char* format, ...);
    void error(const char* format, ...);
    void critical(const char* format, ...);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const char* level_str, const char* format, va_list args);
    const char* level_to_string(LogLevel level);
    const char* level_to_color(LogLevel level);

    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
};

// 便捷宏
#define LOG_TRACE(...) Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::instance().info(__VA_ARGS__)
#define LOG_SUCCESS(...) Logger::instance().success(__VA_ARGS__)
#define LOG_WARNING(...) Logger::instance().warning(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::instance().critical(__VA_ARGS__)

#endif // LOGGER_HPP
