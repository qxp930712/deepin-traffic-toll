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
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <map>
#include <set>
#include <algorithm>

#include "cli.h"
#include "config.h"
#include "process.h"
#include "tc.h"
#include "speedtest.h"
#include "logger.h"
#include "utils.h"

// 全局变量用于信号处理
static volatile bool running = true;
static TrafficControl* g_tc = nullptr;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = false;
        LOG_INFO("Aborted");
    }
}

LogLevel string_to_log_level(const std::string& level) {
    std::string l = to_lower(level);
    if (l == "trace") return LogLevel::TRACE;
    if (l == "debug") return LogLevel::DEBUG;
    if (l == "info") return LogLevel::INFO;
    if (l == "success") return LogLevel::SUCCESS;
    if (l == "warning") return LogLevel::WARNING;
    if (l == "error") return LogLevel::ERROR;
    if (l == "critical") return LogLevel::CRITICAL;
    return LogLevel::INFO;
}

// 常量
static const uint64_t MAX_RATE = 4294967295ULL;
static const std::string GLOBAL_MINIMUM_DOWNLOAD_RATE = "100kbps";
static const std::string GLOBAL_MINIMUM_UPLOAD_RATE = "10kbps";
static const std::string MINIMUM_DOWNLOAD_RATE = "10kbps";
static const std::string MINIMUM_UPLOAD_RATE = "1kbps";

int main(int argc, char* argv[]) {
    // 解析命令行参数
    CliParser parser;
    if (!parser.parse(argc, argv)) {
        return 1;
    }

    const Options& opts = parser.options();

    // 设置日志级别
    Logger::instance().set_level(string_to_log_level(opts.log_level));

    // 加载配置文件
    Config config;
    if (!load_config(opts.config, config)) {
        return 1;
    }

    // 全局速率
    uint64_t global_download_rate = MAX_RATE;
    uint64_t global_upload_rate = MAX_RATE;

    // 自动测速
    if (opts.speed_test) {
        LOG_INFO("Running speed test...");
        auto result = SpeedTest::run();
        if (result) {
            LOG_INFO("Determined download speed: %lu bps, upload speed: %lu bps",
                     result->download_rate, result->upload_rate);
            global_download_rate = result->download_rate;
            global_upload_rate = result->upload_rate;
        } else {
            LOG_ERROR("Failed to automatically determine speed, falling back to configuration values");
        }
    }

    // 使用配置文件中的速率
    if (config.download) {
        global_download_rate = parse_rate(*config.download);
    }
    if (config.upload) {
        global_upload_rate = parse_rate(*config.upload);
    }

    if (!config.download) {
        LOG_INFO("No global download rate specified, download traffic prioritization won't work");
    }
    if (!config.upload) {
        LOG_INFO("No global upload rate specified, upload traffic prioritization won't work");
    }

    // 计算最低优先级（最大的整数）
    int lowest_priority = -1;
    for (const auto& proc : config.processes) {
        if (proc.download_priority && *proc.download_priority > lowest_priority) {
            lowest_priority = *proc.download_priority;
        }
        if (proc.upload_priority && *proc.upload_priority > lowest_priority) {
            lowest_priority = *proc.upload_priority;
        }
    }
    lowest_priority++;

    // 全局优先级
    int global_download_priority = config.download_priority.value_or(lowest_priority);
    int global_upload_priority = config.upload_priority.value_or(lowest_priority);

    // 全局最小速率
    uint64_t global_download_min = parse_rate(config.download_minimum.value_or(GLOBAL_MINIMUM_DOWNLOAD_RATE));
    uint64_t global_upload_min = parse_rate(config.upload_minimum.value_or(GLOBAL_MINIMUM_UPLOAD_RATE));

    if (config.download) {
        LOG_INFO("Setting up global class with max download rate: %s (minimum: %s) and priority: %d",
                 format_rate(global_download_rate).c_str(),
                 format_rate(global_download_min).c_str(),
                 global_download_priority);
    } else {
        LOG_INFO("Setting up global class with unlimited download rate (minimum: %s) and priority: %d",
                 format_rate(global_download_min).c_str(),
                 global_download_priority);
    }

    if (config.upload) {
        LOG_INFO("Setting up global class with max upload rate: %s (minimum: %s) and priority: %d",
                 format_rate(global_upload_rate).c_str(),
                 format_rate(global_upload_min).c_str(),
                 global_upload_priority);
    } else {
        LOG_INFO("Setting up global class with unlimited upload rate (minimum: %s) and priority: %d",
                 format_rate(global_upload_min).c_str(),
                 global_upload_priority);
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 初始化 TrafficControl
    TrafficControl tc;
    g_tc = &tc;

    // 设置 QDisc
    auto [ingress_qdisc, egress_qdisc] = tc.setup(
        opts.device,
        global_download_rate,
        global_download_min,
        global_upload_rate,
        global_upload_min,
        global_download_priority,
        global_upload_priority);

    // 为每个进程创建 class
    std::map<std::string, int> ingress_class_ids;
    std::map<std::string, int> egress_class_ids;

    for (const auto& proc_config : config.processes) {
        if (proc_config.match.empty()) {
            LOG_WARNING("No conditions for: '%s' specified, it will never be matched",
                        proc_config.name.c_str());
            continue;
        }

        LOG_DEBUG("Process config '%s' has %zu match conditions, recursive=%d",
                  proc_config.name.c_str(), proc_config.match.size(), proc_config.recursive);
        for (const auto& m : proc_config.match) {
            LOG_DEBUG("  match: %s = %s", m.key.c_str(), m.value.c_str());
        }

        // 下载配置
        uint64_t download_rate = proc_config.download
            ? parse_rate(*proc_config.download)
            : global_download_rate;
        uint64_t download_min = proc_config.download_minimum
            ? parse_rate(*proc_config.download_minimum)
            : parse_rate(MINIMUM_DOWNLOAD_RATE);
        int download_priority = proc_config.download_priority.value_or(lowest_priority);

        // 上传配置
        uint64_t upload_rate = proc_config.upload
            ? parse_rate(*proc_config.upload)
            : global_upload_rate;
        uint64_t upload_min = proc_config.upload_minimum
            ? parse_rate(*proc_config.upload_minimum)
            : parse_rate(MINIMUM_UPLOAD_RATE);
        int upload_priority = proc_config.upload_priority.value_or(lowest_priority);

        // 创建下载 class
        if (proc_config.download || proc_config.download_priority) {
            if (proc_config.download) {
                LOG_INFO("Setting up class for: '%s' with max download rate: %s (minimum: %s) and priority: %d",
                         proc_config.name.c_str(),
                         format_rate(download_rate).c_str(),
                         format_rate(download_min).c_str(),
                         download_priority);
            } else {
                LOG_INFO("Setting up class for: '%s' with unlimited download rate (minimum: %s) and priority: %d",
                         proc_config.name.c_str(),
                         format_rate(download_min).c_str(),
                         download_priority);
            }

            int class_id = tc.add_htb_class(ingress_qdisc, download_rate, download_min, download_priority);
            ingress_class_ids[proc_config.name] = class_id;
        }

        // 创建上传 class
        if (proc_config.upload || proc_config.upload_priority) {
            if (proc_config.upload) {
                LOG_INFO("Setting up class for: '%s' with max upload rate: %s (minimum: %s) and priority: %d",
                         proc_config.name.c_str(),
                         format_rate(upload_rate).c_str(),
                         format_rate(upload_min).c_str(),
                         upload_priority);
            } else {
                LOG_INFO("Setting up class for: '%s' with unlimited upload rate (minimum: %s) and priority: %d",
                         proc_config.name.c_str(),
                         format_rate(upload_min).c_str(),
                         upload_priority);
            }

            int class_id = tc.add_htb_class(egress_qdisc, upload_rate, upload_min, upload_priority);
            egress_class_ids[proc_config.name] = class_id;
        }
    }

    // 端口过滤器映射
    std::map<int, std::string> port_to_ingress_filter_id;
    std::map<int, std::string> port_to_egress_filter_id;
    std::map<std::string, std::set<int>> filtered_ports;

    // 添加过滤器函数
    auto add_ingress_filter = [&](int port, int class_id) {
        std::string predicate = "match ip dport " + std::to_string(port) + " 0xffff";
        std::string filter_id = tc.add_u32_filter(ingress_qdisc, predicate, class_id);
        port_to_ingress_filter_id[port] = filter_id;
    };

    auto add_egress_filter = [&](int port, int class_id) {
        std::string predicate = "match ip sport " + std::to_string(port) + " 0xffff";
        std::string filter_id = tc.add_u32_filter(egress_qdisc, predicate, class_id);
        port_to_egress_filter_id[port] = filter_id;
    };

    // 删除过滤器函数
    auto remove_filters = [&](int port) {
        if (port_to_ingress_filter_id.count(port)) {
            tc.remove_u32_filter(ingress_qdisc, port_to_ingress_filter_id[port]);
            port_to_ingress_filter_id.erase(port);
        }
        if (port_to_egress_filter_id.count(port)) {
            tc.remove_u32_filter(egress_qdisc, port_to_egress_filter_id[port]);
            port_to_egress_filter_id.erase(port);
        }
    };

    // 主循环
    int loop_count = 0;
    while (running) {
        loop_count++;
        if (loop_count <= 3) {
            LOG_DEBUG("Main loop iteration %d, filtering connections...", loop_count);
        }

        // 过滤进程连接
        auto filtered_connections = ProcessFilter::filter_connections(config.processes);

        if (loop_count <= 3) {
            LOG_DEBUG("Found %zu process groups with connections", filtered_connections.size());
        }

        for (const auto& [name, ports] : filtered_connections) {
            int ingress_class_id = ingress_class_ids.count(name) ? ingress_class_ids[name] : 0;
            int egress_class_id = egress_class_ids.count(name) ? egress_class_ids[name] : 0;

            // 新端口
            std::vector<int> new_ports;
            for (int port : ports) {
                if (!filtered_ports[name].count(port)) {
                    new_ports.push_back(port);
                }
            }

            if (!new_ports.empty()) {
                std::sort(new_ports.begin(), new_ports.end());
                std::string ports_str;
                for (int p : new_ports) {
                    if (!ports_str.empty()) ports_str += ", ";
                    ports_str += std::to_string(p);
                }
                LOG_INFO("Shaping traffic for '%s' on local ports %s",
                         name.c_str(), ports_str.c_str());

                for (int port : new_ports) {
                    if (ingress_class_id) add_ingress_filter(port, ingress_class_id);
                    if (egress_class_id) add_egress_filter(port, egress_class_id);
                }
            }

            // 旧端口
            std::vector<int> freed_ports;
            for (int port : filtered_ports[name]) {
                if (!ports.count(port)) {
                    freed_ports.push_back(port);
                }
            }

            if (!freed_ports.empty()) {
                std::sort(freed_ports.begin(), freed_ports.end());
                std::string ports_str;
                for (int p : freed_ports) {
                    if (!ports_str.empty()) ports_str += ", ";
                    ports_str += std::to_string(p);
                }
                LOG_INFO("Removing filters for '%s' on local ports %s",
                         name.c_str(), ports_str.c_str());

                for (int port : freed_ports) {
                    remove_filters(port);
                }
            }

            filtered_ports[name] = ports;
        }

        // 清理不存在的进程端口
        std::vector<std::string> to_remove;
        for (const auto& [name, ports] : filtered_ports) {
            if (filtered_connections.find(name) == filtered_connections.end()) {
                if (!ports.empty()) {
                    std::vector<int> port_list(ports.begin(), ports.end());
                    std::sort(port_list.begin(), port_list.end());
                    std::string ports_str;
                    for (int p : port_list) {
                        if (!ports_str.empty()) ports_str += ", ";
                        ports_str += std::to_string(p);
                    }
                    LOG_INFO("Removing filters for '%s' on local ports %s",
                             name.c_str(), ports_str.c_str());

                    for (int port : ports) {
                        remove_filters(port);
                    }
                }
                to_remove.push_back(name);
            }
        }

        for (const auto& name : to_remove) {
            filtered_ports.erase(name);
        }

        // 刷新 inode 缓存
        NetParser::refresh_cache();

        // 等待
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(opts.delay * 1000)));
    }

    // 清理
    tc.cleanup();
    LOG_INFO("Cleanup completed");

    return 0;
}
