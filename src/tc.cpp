#include "tc.h"
#include "utils.h"
#include "logger.h"
#include <sstream>

// 常量
static const uint64_t MAX_RATE = 4294967295ULL;  // tc 最大速率
static const std::string INGRESS_QDISC_PARENT_ID = "ffff:fff1";

// 辅助函数：检查是否是 ifb 设备名
static bool is_ifb_device(const std::string& name) {
    if (name.size() < 4) return false;
    if (name.substr(0, 3) != "ifb") return false;
    for (size_t i = 3; i < name.size(); ++i) {
        if (!isdigit(name[i])) return false;
    }
    return true;
}

// 辅助函数：从行中提取 qdisc id
static bool parse_qdisc_id(const std::string& line, int& id) {
    // 格式: qdisc htb 1: root ...
    size_t pos = line.find("qdisc ");
    if (pos == std::string::npos) return false;

    pos = line.find(':', pos);
    if (pos == std::string::npos) return false;

    // 向前找空格后的数字
    size_t start = pos;
    while (start > 0 && line[start - 1] != ' ') start--;

    std::string id_str = line.substr(start, pos - start);
    try {
        id = std::stoi(id_str);
        return true;
    } catch (...) {
        return false;
    }
}

// 辅助函数：从行中提取 class id
static bool parse_class_id(const std::string& line, int& qdisc_id, int& class_id) {
    // 格式: class htb 1:2 parent 1:1 ...
    size_t pos = line.find(':');
    if (pos == std::string::npos) return false;

    // 找 qdisc id
    size_t start = pos;
    while (start > 0 && line[start - 1] != ' ') start--;

    try {
        qdisc_id = std::stoi(line.substr(start, pos - start));
    } catch (...) {
        return false;
    }

    // 找 class id (冒号后面的数字)
    size_t end = pos + 1;
    while (end < line.size() && isdigit(line[end])) end++;

    try {
        class_id = std::stoi(line.substr(pos + 1, end - pos - 1));
        return true;
    } catch (...) {
        return false;
    }
}

// 辅助函数：从行中提取 filter id
static std::string parse_filter_id(const std::string& line) {
    // 格式: filter parent 1: protocol ip pref 1 u32 ... fh 801::800 ...
    size_t pos = line.find(" fh ");
    if (pos == std::string::npos) return "";

    pos += 4;  // 跳过 " fh "
    size_t end = pos;
    while (end < line.size() && line[end] != ' ' && line[end] != '\n') end++;

    return line.substr(pos, end - pos);
}

void TrafficControl::activate_device(const std::string& name) {
    run_command("ip link set dev " + name + " up");
    LOG_DEBUG("Activated device: %s", name.c_str());
}

std::string TrafficControl::create_ifb_device() {
    // 获取当前设备列表
    auto result = run_command("ls /sys/class/net");
    std::set<std::string> before_devices;
    std::istringstream iss(result.stdout_output);
    std::string dev;
    while (iss >> dev) {
        before_devices.insert(dev);
    }

    // 创建 IFB 设备
    run_command("modprobe ifb numifbs=1");

    // 获取新设备
    result = run_command("ls /sys/class/net");
    std::istringstream iss2(result.stdout_output);
    while (iss2 >> dev) {
        if (before_devices.find(dev) == before_devices.end() && is_ifb_device(dev)) {
            activate_device(dev);
            LOG_INFO("Created IFB device: %s", dev.c_str());
            return dev;
        }
    }

    // 如果没找到新设备，尝试使用 ifb0
    if (file_exists("/sys/class/net/ifb0")) {
        activate_device("ifb0");
        return "ifb0";
    }

    return "";
}

std::string TrafficControl::acquire_ifb_device() {
    // 查找已有的 IFB 设备
    auto result = run_command("ls /sys/class/net");
    std::istringstream iss(result.stdout_output);
    std::string dev;

    while (iss >> dev) {
        if (is_ifb_device(dev)) {
            // 检查是否已激活
            std::string operstate = trim(read_file("/sys/class/net/" + dev + "/operstate"));
            if (operstate != "up" && operstate != "unknown") {
                activate_device(dev);
            }
            LOG_INFO("Using existing IFB device: %s", dev.c_str());
            return dev;
        }
    }

    // 没有找到，创建新的
    return create_ifb_device();
}

int TrafficControl::get_free_qdisc_id(const std::string& device) {
    auto result = run_command("tc qdisc show dev " + device);

    std::set<int> ids;
    std::string line;
    std::istringstream iss(result.stdout_output);

    while (std::getline(iss, line)) {
        int id;
        if (parse_qdisc_id(line, id)) {
            ids.insert(id);
        }
    }

    int id = 1;
    while (ids.count(id)) {
        id++;
    }
    return id;
}

int TrafficControl::get_free_class_id(const std::string& device, int qdisc_id) {
    auto result = run_command("tc class show dev " + device);

    std::set<int> ids;
    std::string line;
    std::istringstream iss(result.stdout_output);

    while (std::getline(iss, line)) {
        int qid, cid;
        if (parse_class_id(line, qid, cid) && qid == qdisc_id) {
            ids.insert(cid);
        }
    }

    int id = 1;
    while (ids.count(id)) {
        id++;
    }
    return id;
}

std::set<std::string> TrafficControl::get_filter_ids(const std::string& device) {
    auto result = run_command("tc filter show dev " + device);

    std::set<std::string> ids;
    std::string line;
    std::istringstream iss(result.stdout_output);

    while (std::getline(iss, line)) {
        std::string id = parse_filter_id(line);
        if (!id.empty()) {
            ids.insert(id);
        }
    }

    return ids;
}

std::pair<QDisc, QDisc> TrafficControl::setup(
    const std::string& device,
    uint64_t download_rate,
    uint64_t download_min,
    uint64_t upload_rate,
    uint64_t upload_min,
    int download_priority,
    int upload_priority) {

    // 1. 设置入口 QDisc (ingress)
    run_command("tc qdisc add dev " + device + " handle ffff: ingress");

    // 2. 获取或创建 IFB 设备
    ifb_device_ = acquire_ifb_device();

    // 3. 将入口流量重定向到 IFB
    run_command("tc filter add dev " + device +
                " parent ffff: protocol ip u32 match u32 0 0 action "
                "mirred egress redirect dev " + ifb_device_);

    // 4. 在 IFB 上创建 HTB QDisc
    int ifb_qdisc_id = get_free_qdisc_id(ifb_device_);
    run_command("tc qdisc add dev " + ifb_device_ +
                " root handle " + std::to_string(ifb_qdisc_id) + ": htb");

    int ifb_root_class_id = get_free_class_id(ifb_device_, ifb_qdisc_id);
    run_command("tc class add dev " + ifb_device_ +
                " parent " + std::to_string(ifb_qdisc_id) + ": classid " +
                std::to_string(ifb_qdisc_id) + ":" + std::to_string(ifb_root_class_id) +
                " htb rate " + std::to_string(download_rate));

    ingress_qdisc_ = QDisc{ifb_device_, ifb_qdisc_id, ifb_root_class_id};

    // 5. 创建默认类
    int ifb_default_class_id = add_htb_class(ingress_qdisc_, download_rate, download_min, download_priority);
    run_command("tc filter add dev " + ifb_device_ +
                " parent " + std::to_string(ifb_qdisc_id) +
                ": prio 2 protocol ip u32 match u32 0 0 flowid " +
                std::to_string(ifb_qdisc_id) + ":" + std::to_string(ifb_default_class_id));

    // 6. 在主设备上创建出口 HTB QDisc
    int device_qdisc_id = get_free_qdisc_id(device);
    run_command("tc qdisc add dev " + device +
                " root handle " + std::to_string(device_qdisc_id) + ": htb");

    int device_root_class_id = get_free_class_id(device, device_qdisc_id);
    run_command("tc class add dev " + device +
                " parent " + std::to_string(device_qdisc_id) + ": classid " +
                std::to_string(device_qdisc_id) + ":" + std::to_string(device_root_class_id) +
                " htb rate " + std::to_string(upload_rate));

    egress_qdisc_ = QDisc{device, device_qdisc_id, device_root_class_id};

    // 7. 创建默认出口类
    int device_default_class_id = add_htb_class(egress_qdisc_, upload_rate, upload_min, upload_priority);
    run_command("tc filter add dev " + device +
                " parent " + std::to_string(device_qdisc_id) +
                ": prio 2 protocol ip u32 match u32 0 0 flowid " +
                std::to_string(device_qdisc_id) + ":" + std::to_string(device_default_class_id));

    return {ingress_qdisc_, egress_qdisc_};
}

int TrafficControl::add_htb_class(const QDisc& qdisc,
                                   uint64_t ceil,
                                   uint64_t rate,
                                   int priority) {
    int class_id = get_free_class_id(qdisc.device, qdisc.id);

    std::string cmd = "tc class add dev " + qdisc.device +
                      " parent " + std::to_string(qdisc.id) + ":" + std::to_string(qdisc.root_class_id) +
                      " classid " + std::to_string(qdisc.id) + ":" + std::to_string(class_id) +
                      " htb rate " + std::to_string(rate) +
                      " ceil " + std::to_string(ceil) +
                      " prio " + std::to_string(priority);

    run_command(cmd);
    LOG_DEBUG("Added HTB class %d on %s (rate: %lu, ceil: %lu, prio: %d)",
              class_id, qdisc.device.c_str(), rate, ceil, priority);

    return class_id;
}

std::string TrafficControl::add_u32_filter(const QDisc& qdisc,
                                            const std::string& predicate,
                                            int class_id) {
    auto before = get_filter_ids(qdisc.device);

    std::string cmd = "tc filter add dev " + qdisc.device +
                      " protocol ip parent " + std::to_string(qdisc.id) +
                      ": prio 1 u32 " + predicate +
                      " flowid " + std::to_string(qdisc.id) + ":" + std::to_string(class_id);

    run_command(cmd);
    LOG_DEBUG("Added u32 filter on %s: %s -> class %d",
              qdisc.device.c_str(), predicate.c_str(), class_id);

    auto after = get_filter_ids(qdisc.device);

    // 找到新增的 filter id
    for (const auto& id : after) {
        if (before.find(id) == before.end()) {
            return id;
        }
    }

    return "";
}

void TrafficControl::remove_u32_filter(const QDisc& qdisc,
                                        const std::string& filter_id) {
    std::string cmd = "tc filter del dev " + qdisc.device +
                      " parent " + std::to_string(qdisc.id) +
                      ": handle " + filter_id +
                      " prio 1 protocol ip u32";

    run_command(cmd);
    LOG_DEBUG("Removed u32 filter %s on %s", filter_id.c_str(), qdisc.device.c_str());
}

void TrafficControl::remove_qdisc(const std::string& device,
                                   const std::string& parent) {
    std::string cmd = "tc qdisc del dev " + device + " parent " + parent;
    run_command(cmd);
    LOG_DEBUG("Removed qdisc on %s parent %s", device.c_str(), parent.c_str());
}

void TrafficControl::cleanup() {
    LOG_INFO("Cleaning up QDiscs");

    if (!ingress_qdisc_.device.empty()) {
        remove_qdisc(ingress_qdisc_.device, "root");
    }

    if (!egress_qdisc_.device.empty()) {
        remove_qdisc(egress_qdisc_.device, "root");
        remove_qdisc(egress_qdisc_.device, INGRESS_QDISC_PARENT_ID);
    }
}
