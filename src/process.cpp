#include "process.h"
#include "config.h"
#include "utils.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>

// 全局缓存
std::unordered_map<int, int> g_inode_to_pid;
bool g_inode_cache_built = false;

void NetParser::refresh_cache() {
    g_inode_cache_built = false;
}

// 直接从进程的网络表获取端口（不依赖全局inode映射）
static std::set<int> get_ports_from_proc_net(int pid, const std::string& filename) {
    std::set<int> ports;

    // 直接读取 /proc/[pid]/net/tcp 或 udp
    std::string path = "/proc/" + std::to_string(pid) + "/net/" + filename;
    std::ifstream file(path);
    if (!file.is_open()) return ports;

    std::string line;
    std::getline(file, line);  // 跳过标题行

    while (std::getline(file, line)) {
        try {
            // 格式: sl local_address rem_address st tx_queue:rx_queue ...
            std::istringstream iss(line);
            std::string sl, local_addr;
            iss >> sl >> local_addr;

            // local_addr 格式: "0100007F:0035" 或 IPv6 格式
            // 提取端口（最后一个冒号后面）
            size_t colon_pos = local_addr.rfind(':');
            if (colon_pos != std::string::npos) {
                std::string port_str = local_addr.substr(colon_pos + 1);
                int port = std::stoi(port_str, nullptr, 16);
                ports.insert(port);
            }
        } catch (...) {}
    }

    return ports;
}

std::set<int> NetParser::get_process_ports(int pid) {
    std::set<int> ports;

    // 直接从进程的网络表读取（这是 psutil 的做法）
    auto tcp_ports = get_ports_from_proc_net(pid, "tcp");
    auto tcp6_ports = get_ports_from_proc_net(pid, "tcp6");
    auto udp_ports = get_ports_from_proc_net(pid, "udp");
    auto udp6_ports = get_ports_from_proc_net(pid, "udp6");

    ports.insert(tcp_ports.begin(), tcp_ports.end());
    ports.insert(tcp6_ports.begin(), tcp6_ports.end());
    ports.insert(udp_ports.begin(), udp_ports.end());
    ports.insert(udp6_ports.begin(), udp6_ports.end());

    return ports;
}

std::unordered_map<int, int> NetParser::get_port_to_pid() {
    return {};
}

bool ProcessFilter::match(const Process& proc, const std::string& key, const std::regex& pattern) {
    std::string value;

    if (key == "name") {
        value = proc.name;
    } else if (key == "exe") {
        value = proc.exe;
    } else if (key == "cmdline") {
        value = proc.cmdline;
    } else if (key == "pid") {
        value = std::to_string(proc.pid);
    } else {
        return false;
    }

    return std::regex_search(value, pattern);
}

std::vector<Process> ProcessFilter::get_all_processes() {
    std::vector<Process> processes;

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return processes;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.empty() || !isdigit(name[0])) continue;

        Process proc;
        proc.pid = std::stoi(name);

        std::string comm_path = "/proc/" + name + "/comm";
        proc.name = trim(read_file(comm_path));

        std::string exe_path = "/proc/" + name + "/exe";
        auto exe_link = read_link(exe_path);
        proc.exe = exe_link.value_or("");

        std::string cmdline_path = "/proc/" + name + "/cmdline";
        std::string cmdline = read_file(cmdline_path);
        for (char& c : cmdline) {
            if (c == '\0') c = ' ';
        }
        proc.cmdline = trim(cmdline);

        processes.push_back(proc);
    }
    closedir(proc_dir);

    return processes;
}

static std::vector<int> get_child_pids(int parent_pid, const std::vector<Process>& all_processes) {
    std::vector<int> children;

    for (const auto& proc : all_processes) {
        std::string stat_path = "/proc/" + std::to_string(proc.pid) + "/stat";
        std::string stat_content = read_file(stat_path);
        if (stat_content.empty()) continue;

        size_t paren_end = stat_content.rfind(')');
        if (paren_end == std::string::npos) continue;

        std::string after_comm = stat_content.substr(paren_end + 1);
        std::istringstream iss(after_comm);
        std::string state;
        int ppid;
        iss >> state >> ppid;

        if (ppid == parent_pid) {
            children.push_back(proc.pid);
        }
    }

    return children;
}

std::unordered_map<std::string, std::set<int>> ProcessFilter::filter_connections(
    const std::vector<ProcessConfig>& configs) {

    std::unordered_map<std::string, std::set<int>> result;

    auto all_processes = get_all_processes();

    // 为每个配置预编译正则
    std::vector<std::vector<std::pair<std::string, std::regex>>> compiled_patterns;
    for (const auto& config : configs) {
        std::vector<std::pair<std::string, std::regex>> patterns;
        for (const auto& cond : config.match) {
            try {
                patterns.emplace_back(cond.key, std::regex(cond.value));
            } catch (const std::regex_error& e) {
                LOG_WARNING("Invalid regex '%s': %s", cond.value.c_str(), e.what());
            }
        }
        compiled_patterns.push_back(std::move(patterns));
    }

    // 匹配进程并获取端口
    for (size_t i = 0; i < configs.size(); ++i) {
        const auto& config = configs[i];
        const auto& patterns = compiled_patterns[i];
        int match_count = 0;
        int total_ports = 0;

        for (const auto& proc : all_processes) {
            bool matched = true;
            for (const auto& [key, regex] : patterns) {
                if (!match(proc, key, regex)) {
                    matched = false;
                    break;
                }
            }

            if (matched) {
                match_count++;
                auto ports = NetParser::get_process_ports(proc.pid);
                if (!ports.empty()) {
                    total_ports += ports.size();
                }
                result[config.name].insert(ports.begin(), ports.end());

                // 递归获取子进程端口
                if (config.recursive) {
                    std::function<void(int)> get_recursive_ports = [&](int parent_pid) {
                        auto child_pids = get_child_pids(parent_pid, all_processes);
                        for (int child_pid : child_pids) {
                            auto child_ports = NetParser::get_process_ports(child_pid);
                            if (!child_ports.empty()) {
                                total_ports += child_ports.size();
                            }
                            result[config.name].insert(child_ports.begin(), child_ports.end());
                            get_recursive_ports(child_pid);
                        }
                    };
                    get_recursive_ports(proc.pid);
                }
            }
        }

        LOG_DEBUG("Config '%s': matched %d processes, total %d ports",
                  config.name.c_str(), match_count, total_ports);
    }

    return result;
}
