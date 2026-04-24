#include "config.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <regex>

int YamlParser::count_indent(const std::string& line) {
    int indent = 0;
    for (char c : line) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += 2;  // tab 算作 2 空格
        else break;
    }
    return indent;
}

std::string YamlParser::strip_comment(const std::string& line) {
    size_t pos = line.find('#');
    if (pos == std::string::npos) {
        return line;
    }
    // 检查 # 是否在引号内
    bool in_quote = false;
    for (size_t i = 0; i < pos; ++i) {
        if (line[i] == '"') {
            in_quote = !in_quote;
        }
    }
    if (!in_quote) {
        return line.substr(0, pos);
    }
    return line;
}

std::string YamlParser::strip_quotes(const std::string& s) {
    std::string trimmed = trim(s);
    if (trimmed.size() >= 2) {
        if ((trimmed.front() == '"' && trimmed.back() == '"') ||
            (trimmed.front() == '\'' && trimmed.back() == '\'')) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
    }
    return trimmed;
}

bool YamlParser::parse_key_value(const std::string& line, std::string& key, std::string& value) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    key = trim(line.substr(0, colon_pos));
    value = trim(line.substr(colon_pos + 1));

    key = strip_quotes(key);
    value = strip_quotes(value);

    return !key.empty();
}

bool YamlParser::parse(const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    State state = State::ROOT;
    std::string current_process;
    int process_indent = 0;
    bool in_match_section = false;
    int match_indent = 0;

    while (std::getline(stream, line)) {
        // 跳过空行
        std::string stripped = strip_comment(line);
        if (trim(stripped).empty()) {
            continue;
        }

        int indent = count_indent(stripped);
        stripped = trim(stripped);

        std::string key, value;
        bool is_key_value = parse_key_value(stripped, key, value);

        // 根据缩进判断当前状态
        if (indent == 0) {
            state = State::ROOT;
            in_match_section = false;
        }

        if (state == State::ROOT) {
            if (key == "processes") {
                state = State::PROCESSES;
                continue;
            }

            // 全局配置
            if (is_key_value) {
                if (key == "download") {
                    config_.download = value;
                } else if (key == "upload") {
                    config_.upload = value;
                } else if (key == "download-minimum") {
                    config_.download_minimum = value;
                } else if (key == "upload-minimum") {
                    config_.upload_minimum = value;
                } else if (key == "download-priority") {
                    config_.download_priority = std::stoi(value);
                } else if (key == "upload-priority") {
                    config_.upload_priority = std::stoi(value);
                }
            }
        } else if (state == State::PROCESSES) {
            // 进程名（如 "Path of Exile":）
            if (indent <= 2 && is_key_value) {
                // 检查是否是新进程（值可能为空）
                if (value.empty() || indent == 0) {
                    // 新进程
                    ProcessConfig proc;
                    proc.name = key;
                    config_.processes.push_back(proc);
                    current_process = key;
                    process_indent = indent;
                    state = State::PROCESS_NAME;
                    in_match_section = false;
                } else {
                    // 可能是 processes 下的直接属性，当作进程名处理
                    ProcessConfig proc;
                    proc.name = key;
                    config_.processes.push_back(proc);
                    current_process = key;
                    process_indent = indent;
                    state = State::PROCESS_NAME;
                    in_match_section = false;
                }
            }
        } else if (state == State::PROCESS_NAME) {
            // 进程属性
            if (!config_.processes.empty()) {
                ProcessConfig& proc = config_.processes.back();

                // 检查是否退回到根级别
                if (indent == 0) {
                    state = State::ROOT;
                    in_match_section = false;
                    // 重新处理这行
                    if (key == "processes") {
                        state = State::PROCESSES;
                        continue;
                    }
                    if (is_key_value) {
                        if (key == "download") config_.download = value;
                        else if (key == "upload") config_.upload = value;
                        else if (key == "download-minimum") config_.download_minimum = value;
                        else if (key == "upload-minimum") config_.upload_minimum = value;
                        else if (key == "download-priority") config_.download_priority = std::stoi(value);
                        else if (key == "upload-priority") config_.upload_priority = std::stoi(value);
                    }
                    continue;
                }

                // 检查是否进入 match 部分
                if (key == "match") {
                    in_match_section = true;
                    match_indent = indent;
                    continue;
                }

                // 检查是否退出 match 部分
                if (in_match_section && indent <= match_indent) {
                    in_match_section = false;
                }

                if (in_match_section) {
                    // 解析 match 条件
                    // 处理 YAML 列表格式: "- key: value"
                    std::string match_key = key;
                    std::string match_value = value;

                    // 检查是否以 "- " 开头（列表项）
                    if (match_key.size() > 2 && match_key[0] == '-' && match_key[1] == ' ') {
                        match_key = trim(match_key.substr(2));
                    }

                    if (!match_key.empty()) {
                        MatchCondition cond;
                        cond.key = match_key;
                        cond.value = match_value;
                        proc.match.push_back(cond);
                    }
                } else {
                    // 进程属性
                    if (is_key_value) {
                        if (key == "download") {
                            proc.download = value;
                        } else if (key == "upload") {
                            proc.upload = value;
                        } else if (key == "download-minimum") {
                            proc.download_minimum = value;
                        } else if (key == "upload-minimum") {
                            proc.upload_minimum = value;
                        } else if (key == "download-priority") {
                            proc.download_priority = std::stoi(value);
                        } else if (key == "upload-priority") {
                            proc.upload_priority = std::stoi(value);
                        } else if (key == "recursive") {
                            std::string v = to_lower(value);
                            proc.recursive = (v == "true" || v == "yes" || v == "1");
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool load_config(const std::string& path, Config& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file: %s", path.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    YamlParser parser;
    if (!parser.parse(content)) {
        LOG_ERROR("Failed to parse config file: %s", path.c_str());
        return false;
    }

    config = parser.config();
    return true;
}
