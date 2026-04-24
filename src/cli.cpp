#include "cli.hpp"
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
        } else if (strcmp(argv[i], "--speed-test") == 0 || strcmp(argv[i], "-s") == 0) {
            options_.speed_test = true;
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
    printf("  -s, --speed-test           Automatically determine upload/download speed\n");
    printf("  -h, --help                 Show this help message\n");
}
