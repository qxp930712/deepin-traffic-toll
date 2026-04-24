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
#ifndef TC_HPP
#define TC_HPP

#include <string>
#include <utility>
#include <map>
#include <set>

// QDisc 结构
struct QDisc {
    std::string device;
    int id;
    int root_class_id;
};

// TC 流量控制封装
class TrafficControl {
public:
    // 初始化设置
    std::pair<QDisc, QDisc> setup(
        const std::string& device,
        uint64_t download_rate,
        uint64_t download_min,
        uint64_t upload_rate,
        uint64_t upload_min,
        int download_priority,
        int upload_priority);

    // 添加 HTB 类
    int add_htb_class(const QDisc& qdisc,
                      uint64_t ceil,
                      uint64_t rate,
                      int priority);

    // 添加/删除 U32 过滤器
    std::string add_u32_filter(const QDisc& qdisc,
                               const std::string& predicate,
                               int class_id);
    void remove_u32_filter(const QDisc& qdisc,
                           const std::string& filter_id);

    // 清理
    void remove_qdisc(const std::string& device,
                      const std::string& parent = "root");
    void cleanup();

    // 获取 QDisc
    const QDisc& ingress_qdisc() const { return ingress_qdisc_; }
    const QDisc& egress_qdisc() const { return egress_qdisc_; }

private:
    QDisc ingress_qdisc_;
    QDisc egress_qdisc_;
    std::string ifb_device_;
    int next_qdisc_id_ = 1;
    int next_class_id_ = 1;

    int get_free_qdisc_id(const std::string& device);
    int get_free_class_id(const std::string& device, int qdisc_id);
    std::set<std::string> get_filter_ids(const std::string& device);
    std::string create_ifb_device();
    std::string acquire_ifb_device();
    void activate_device(const std::string& name);
};

#endif // TC_HPP
