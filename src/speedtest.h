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
