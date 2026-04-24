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
#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>
#include <vector>
#include <set>
#include <optional>
#include <regex>
#include <unordered_map>

// 全局缓存（用于刷新）
extern bool g_inode_cache_built;

// 网络连接
struct Connection {
    int local_port;
    int remote_port;
    std::string remote_ip;
    enum class Type { TCP, UDP } type;
};

// 进程信息
struct Process {
    int pid;
    std::string name;      // /proc/[pid]/comm
    std::string exe;       // /proc/[pid]/exe
    std::string cmdline;   // /proc/[pid]/cmdline

    std::vector<Connection> connections();
    std::vector<Process> children();
};

// 进程过滤器
class ProcessFilter {
public:
    // 匹配单个进程
    static bool match(const Process& proc, const std::string& key, const std::regex& pattern);

    // 获取所有进程
    static std::vector<Process> get_all_processes();

    // 根据条件过滤进程并获取连接
    static std::unordered_map<std::string, std::set<int>> filter_connections(
        const std::vector<struct ProcessConfig>& configs);
};

// 解析 /proc/net/tcp 和 /proc/net/tcp6
class NetParser {
public:
    // 获取端口到 PID 的映射
    static std::unordered_map<int, int> get_port_to_pid();

    // 获取进程的所有连接端口
    static std::set<int> get_process_ports(int pid);

    // 刷新 inode 缓存
    static void refresh_cache();

private:
    static std::vector<std::tuple<int, int, std::string>> parse_tcp_file(const std::string& path);
};

// 全局缓存（用于刷新）
extern bool g_inode_cache_built;

#endif // PROCESS_HPP
