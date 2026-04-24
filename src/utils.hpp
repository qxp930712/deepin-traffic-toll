#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// 速率单位解析 (支持: bit, kbit, mbit, gbit, bps, kbps, mbps, gbps)
uint64_t parse_rate(const std::string& rate_str);
std::string format_rate(uint64_t rate_bps);

// 执行 shell 命令
struct CommandResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
};

CommandResult run_command(const std::string& command);
bool command_exists(const std::string& command);

// 字符串工具
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delimiter);
std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
std::string to_lower(const std::string& s);

// 文件工具
bool file_exists(const std::string& path);
std::string read_file(const std::string& path);
std::optional<std::string> read_link(const std::string& path);

#endif // UTILS_HPP
