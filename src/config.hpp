#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <optional>
#include <map>

// 匹配条件
struct MatchCondition {
    std::string key;    // name, exe, cmdline, pid 等
    std::string value;  // 正则表达式
};

// 进程配置
struct ProcessConfig {
    std::string name;
    std::optional<std::string> download;
    std::optional<std::string> upload;
    std::optional<std::string> download_minimum;
    std::optional<std::string> upload_minimum;
    std::optional<int> download_priority;
    std::optional<int> upload_priority;
    bool recursive = false;
    std::vector<MatchCondition> match;
};

// 全局配置
struct Config {
    std::optional<std::string> download;
    std::optional<std::string> upload;
    std::optional<std::string> download_minimum;
    std::optional<std::string> upload_minimum;
    std::optional<int> download_priority;
    std::optional<int> upload_priority;
    std::vector<ProcessConfig> processes;
};

// YAML 解析器
class YamlParser {
public:
    bool parse(const std::string& content);
    const Config& config() const { return config_; }

private:
    Config config_;

    // 简单的 YAML 解析状态机
    enum class State {
        ROOT,
        PROCESSES,
        PROCESS_NAME,
        PROCESS_FIELD
    };

    void parse_line(const std::string& line, State& state,
                    std::string& current_process, int indent);
    int count_indent(const std::string& line);
    std::string strip_comment(const std::string& line);
    bool parse_key_value(const std::string& line, std::string& key, std::string& value);
    std::string strip_quotes(const std::string& s);
};

// 加载配置文件
bool load_config(const std::string& path, Config& config);

#endif // CONFIG_HPP
