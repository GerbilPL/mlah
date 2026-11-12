#include "glogger.hpp"

#include <iostream>

int main()
{
    gLogger::Logger logger;
    if (!logger.init("logs")) {
        std::cerr << "Could not initialize logger\n";
        return 1;
    }

    logger.log(gLogger::LogLevel::INFO, "Application started");
    logger(gLogger::LogLevel::DBG) << "Loaded " << 3 << " configuration entries";
    logger.log(gLogger::LogLevel::WARN, "Using default timeout");
}
