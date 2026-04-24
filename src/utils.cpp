#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <array>
#include <cstdio>
#include <unistd.h>
#include <limits.h>

uint64_t parse_rate(const std::string& rate_str) {
    if (rate_str.empty()) {
        return 0;
    }

    std::string s = to_lower(trim(rate_str));

    // 简单解析：找到数字和单位
    size_t i = 0;
    while (i < s.size() && (isdigit(s[i]) || s[i] == '.')) i++;

    if (i == 0) return 0;

    double value = std::stod(s.substr(0, i));
    std::string unit = s.substr(i);
    unit = trim(unit);

    if (unit.empty()) return 0;

    // 转换为 bps
    double multiplier = 1.0;

    // 确定基础单位和前缀
    bool is_bit = (unit.find("bit") != std::string::npos);
    bool is_iec = (unit[0] == 'k' && unit[1] == 'i') ||
                  (unit[0] == 'm' && unit[1] == 'i') ||
                  (unit[0] == 'g' && unit[1] == 'i') ||
                  (unit[0] == 't' && unit[1] == 'i');

    if (is_bit) {
        // bit 单位
        if (unit.find("kbit") != std::string::npos || unit.find("kibit") != std::string::npos) {
            multiplier = is_iec ? 1024 : 1000;
        } else if (unit.find("mbit") != std::string::npos || unit.find("mibit") != std::string::npos) {
            multiplier = is_iec ? 1024 * 1024 : 1000 * 1000;
        } else if (unit.find("gbit") != std::string::npos || unit.find("gibit") != std::string::npos) {
            multiplier = is_iec ? 1024 * 1024 * 1024 : 1000 * 1000 * 1000;
        } else if (unit.find("tbit") != std::string::npos || unit.find("tibit") != std::string::npos) {
            multiplier = is_iec ? 1024ULL * 1024 * 1024 * 1024 : 1000ULL * 1000 * 1000 * 1000;
        }
    } else {
        // bps 单位 (bytes per second -> bits per second)
        multiplier = 8;  // 基础 bps
        if (unit.find("kbps") != std::string::npos || unit.find("kibps") != std::string::npos) {
            multiplier = is_iec ? 8 * 1024 : 8 * 1000;
        } else if (unit.find("mbps") != std::string::npos || unit.find("mibps") != std::string::npos) {
            multiplier = is_iec ? 8 * 1024 * 1024 : 8 * 1000 * 1000;
        } else if (unit.find("gbps") != std::string::npos || unit.find("gibps") != std::string::npos) {
            multiplier = is_iec ? 8 * 1024 * 1024 * 1024 : 8 * 1000 * 1000 * 1000;
        } else if (unit.find("tbps") != std::string::npos || unit.find("tibps") != std::string::npos) {
            multiplier = is_iec ? 8ULL * 1024 * 1024 * 1024 * 1024 : 8ULL * 1000 * 1000 * 1000 * 1000;
        }
    }

    return static_cast<uint64_t>(value * multiplier);
}

std::string format_rate(uint64_t rate_bps) {
    if (rate_bps >= 8ULL * 1000 * 1000 * 1000) {
        return std::to_string(rate_bps / (8ULL * 1000 * 1000 * 1000)) + "gbps";
    } else if (rate_bps >= 8ULL * 1000 * 1000) {
        return std::to_string(rate_bps / (8ULL * 1000 * 1000)) + "mbps";
    } else if (rate_bps >= 8ULL * 1000) {
        return std::to_string(rate_bps / (8ULL * 1000)) + "kbps";
    } else {
        return std::to_string(rate_bps) + "bps";
    }
}

CommandResult run_command(const std::string& command) {
    CommandResult result;

    // 使用 popen 执行命令
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (!pipe) {
        result.exit_code = -1;
        return result;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result.stdout_output += buffer;
    }

    result.exit_code = pclose(pipe);
    return result;
}

bool command_exists(const std::string& command) {
    std::string path = "/usr/bin/" + command;
    if (access(path.c_str(), X_OK) == 0) {
        return true;
    }

    // 检查 PATH 环境变量
    FILE* pipe = popen(("which " + command + " 2>/dev/null").c_str(), "r");
    if (pipe) {
        char buffer[256];
        bool found = (fgets(buffer, sizeof(buffer), pipe) != nullptr);
        pclose(pipe);
        return found;
    }
    return false;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += delimiter;
        }
        result += parts[i];
    }
    return result;
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool file_exists(const std::string& path) {
    return access(path.c_str(), F_OK) == 0;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<std::string> read_link(const std::string& path) {
    char buffer[PATH_MAX];
    ssize_t len = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
    if (len == -1) {
        return std::nullopt;
    }
    buffer[len] = '\0';
    return std::string(buffer);
}
