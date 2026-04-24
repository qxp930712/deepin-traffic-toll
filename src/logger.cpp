/*
* Copyright (C) 2026 Uniontech Technology Co., Ltd.
*
* Author:      caimengci <caimengci@uniontech.com>
*
* Maintainer:  caimengci <caimengci@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "logger.h"
#include <cstdarg>
#include <cstring>
#include <sys/time.h>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::get_level() const {
    return level_;
}

const char* Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::SUCCESS:  return "SUCCESS";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* Logger::level_to_color(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "\033[90m";      // 灰色
        case LogLevel::DEBUG:    return "\033[36m";      // 青色
        case LogLevel::INFO:     return "\033[37m";      // 白色
        case LogLevel::SUCCESS:  return "\033[32m";      // 绿色
        case LogLevel::WARNING:  return "\033[33m";      // 黄色
        case LogLevel::ERROR:    return "\033[31m";      // 红色
        case LogLevel::CRITICAL: return "\033[35m";      // 紫色
        default: return "\033[0m";
    }
}

void Logger::log(LogLevel level, const char* level_str, const char* format, va_list args) {
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }

    // 获取当前时间
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm* tm_info = localtime(&tv.tv_sec);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // 格式化用户消息
    char msg_buf[4096];
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);

    // 输出带颜色和时间的日志
    std::lock_guard<std::mutex> lock(mutex_);
    const char* color = level_to_color(level);
    printf("%s%s.%03ld | %-8s | %s\033[0m\n",
           color, time_buf, tv.tv_usec / 1000, level_str, msg_buf);
}

void Logger::trace(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::TRACE, "TRACE", format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::DEBUG, "DEBUG", format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::INFO, "INFO", format, args);
    va_end(args);
}

void Logger::success(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::SUCCESS, "SUCCESS", format, args);
    va_end(args);
}

void Logger::warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::WARNING, "WARNING", format, args);
    va_end(args);
}

void Logger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::ERROR, "ERROR", format, args);
    va_end(args);
}

void Logger::critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::CRITICAL, "CRITICAL", format, args);
    va_end(args);
}
