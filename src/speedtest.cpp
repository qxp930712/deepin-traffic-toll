#include "speedtest.h"
#include "utils.h"
#include "logger.h"
#include <regex>
#include <sstream>

SpeedTest::Provider SpeedTest::detect_provider() {
    if (!command_exists("speedtest")) {
        return Provider::Unknown;
    }

    auto result = run_command("speedtest --version");

    if (result.stdout_output.find("Speedtest by Ookla") != std::string::npos) {
        return Provider::Ookla;
    } else if (result.stdout_output.find("speedtest-cli") != std::string::npos) {
        return Provider::Sivel;
    }

    return Provider::Unknown;
}

std::optional<SpeedTestResult> SpeedTest::ookla_test() {
    auto result = run_command("speedtest --format=json");

    if (result.exit_code != 0) {
        LOG_ERROR("Ookla speedtest failed: %s", result.stderr_output.c_str());
        return std::nullopt;
    }

    // 简单 JSON 解析
    try {
        std::string json = result.stdout_output;

        // 查找 download bandwidth
        auto find_bandwidth = [&json](const std::string& section) -> uint64_t {
            std::string pattern = "\"" + section + "\":{";
            size_t pos = json.find(pattern);
            if (pos == std::string::npos) return 0;

            pos = json.find("\"bandwidth\":", pos);
            if (pos == std::string::npos) return 0;

            pos += 13;  // 跳过 "bandwidth":
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;

            size_t end = pos;
            while (end < json.size() && isdigit(json[end])) end++;

            if (end > pos) {
                return std::stoull(json.substr(pos, end - pos));
            }
            return 0;
        };

        SpeedTestResult res;
        res.download_rate = find_bandwidth("download");
        res.upload_rate = find_bandwidth("upload");

        if (res.download_rate == 0 || res.upload_rate == 0) {
            LOG_ERROR("Failed to parse Ookla speedtest output");
            return std::nullopt;
        }

        return res;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Ookla speedtest output: %s", e.what());
        return std::nullopt;
    }
}

std::optional<SpeedTestResult> SpeedTest::sivel_test() {
    auto result = run_command("speedtest --json");

    if (result.exit_code != 0) {
        LOG_ERROR("Sivel speedtest failed: %s", result.stderr_output.c_str());
        return std::nullopt;
    }

    try {
        std::string json = result.stdout_output;

        auto find_value = [&json](const std::string& key) -> double {
            std::string pattern = "\"" + key + "\":";
            size_t pos = json.find(pattern);
            if (pos == std::string::npos) return 0;

            pos += pattern.length();
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;

            size_t end = pos;
            while (end < json.size() && (isdigit(json[end]) || json[end] == '.')) end++;

            if (end > pos) {
                return std::stod(json.substr(pos, end - pos));
            }
            return 0;
        };

        SpeedTestResult res;
        res.download_rate = static_cast<uint64_t>(find_value("download"));
        res.upload_rate = static_cast<uint64_t>(find_value("upload"));

        if (res.download_rate == 0 || res.upload_rate == 0) {
            LOG_ERROR("Failed to parse Sivel speedtest output");
            return std::nullopt;
        }

        return res;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Sivel speedtest output: %s", e.what());
        return std::nullopt;
    }
}

std::optional<SpeedTestResult> SpeedTest::run() {
    Provider provider = detect_provider();

    switch (provider) {
        case Provider::Ookla:
            LOG_INFO("Using Ookla speedtest provider");
            return ookla_test();

        case Provider::Sivel:
            LOG_INFO("Using Sivel speedtest-cli provider");
            return sivel_test();

        default:
            LOG_ERROR("No speedtest provider found. Install speedtest-cli or Ookla speedtest.");
            return std::nullopt;
    }
}
