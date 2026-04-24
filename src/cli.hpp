#ifndef CLI_HPP
#define CLI_HPP

#include <string>

struct Options {
    std::string device;
    std::string config;
    double delay = 1.0;
    std::string log_level = "INFO";
    bool speed_test = false;
};

class CliParser {
public:
    bool parse(int argc, char* argv[]);
    void print_help(const char* program_name);
    const Options& options() const { return options_; }

private:
    Options options_;
};

#endif // CLI_HPP
