#include "glogger.hpp"

#include <iostream>

int main() {
    glogger::LoggerOptions options;
    options.directory = "logs";
    options.active_filename = "server.log";
    options.archive_policy = glogger::ArchivePolicy::IncrementingNumber;

    glogger::Logger logger(options);
    if (auto result = logger.initialize(); !result) {
        std::cerr << "Could not initialize logger: " << result.error().message << '\n';
        return 1;
    }
    if (auto result = logger.log(glogger::Level::Info, "Server started"); !result) {
        std::cerr << "Could not write log record: " << result.error().message << '\n';
        return 1;
    }
    if (auto result = logger.logf(glogger::Level::Info, "Listening on port {}", 8080); !result) {
        std::cerr << "Could not write log record: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
