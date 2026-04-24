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
#include "cli.h"
#include <cstring>
#include <cstdlib>

bool CliParser::parse(int argc, char* argv[]) {
    if (argc < 3) {
        print_help(argv[0]);
        return false;
    }

    int i = 1;

    // 解析必需参数
    while (i < argc && argv[i][0] != '-') {
        if (options_.device.empty()) {
            options_.device = argv[i];
        } else if (options_.config.empty()) {
            options_.config = argv[i];
        }
        i++;
    }

    if (options_.device.empty() || options_.config.empty()) {
        print_help(argv[0]);
        return false;
    }

    // 解析可选参数
    while (i < argc) {
        if (strcmp(argv[i], "--delay") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            options_.delay = std::stod(argv[++i]);
        } else if (strcmp(argv[i], "--logging-level") == 0 || strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            options_.log_level = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return false;
        } else {
            return false;
        }
        i++;
    }

    return true;
}

void CliParser::print_help(const char* program_name) {
    printf("Usage: %s device config [options]\n\n", program_name);
    printf("Arguments:\n");
    printf("  device              The network device to be traffic shaped\n");
    printf("  config              The configuration file\n\n");
    printf("Options:\n");
    printf("  -d, --delay <seconds>      Delay between checks (default: 1.0)\n");
    printf("  -l, --logging-level <level> Logging level: TRACE, DEBUG, INFO,\n");
    printf("                             SUCCESS, WARNING, ERROR, CRITICAL (default: INFO)\n");
    printf("  -h, --help                 Show this help message\n");
}
