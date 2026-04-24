#ifndef SPEEDTEST_HPP
#define SPEEDTEST_HPP

#include <cstdint>
#include <optional>
#include <string>

struct SpeedTestResult {
    uint64_t download_rate;  // bps
    uint64_t upload_rate;    // bps
};

// 测速功能
class SpeedTest {
public:
    static std::optional<SpeedTestResult> run();

private:
    enum class Provider { Ookla, Sivel, Unknown };

    static Provider detect_provider();
    static std::optional<SpeedTestResult> ookla_test();
    static std::optional<SpeedTestResult> sivel_test();
};

#endif // SPEEDTEST_HPP
