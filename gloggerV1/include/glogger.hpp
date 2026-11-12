#pragma once
#include "glogger_export.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <expected>
#include <chrono>
#include <format>
#include <sstream>

#define GLL gLogger::LogLevel


namespace gLogger
{
    /** @brief Severity/category labels written with each log message. */
    enum class LogLevel
    {
        ERR = 0, // Avoid colliding with the ERROR macro from Windows headers.
        DBG,
        WARN,
        INFO,
        CHAT
    };

    static inline const char* LogLevelStr(LogLevel lv)
    {
        switch (lv)
        {
            case LogLevel::ERR:  return "ERROR";
            case LogLevel::DBG:   return "DBG";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::CHAT:  return "CHAT";
            default:              return "OTHER";
        }
    }

    /**
     * @class Logger
     * @brief Synchronous logger that writes to the console and `latest.log`.
     *
     * Call `init()` before `log()`; messages sent before successful
     * initialization are ignored. Calling `init()` again switches the output
     * directory. A Logger instance is not safe for concurrent calls.
     */
    class Logger
    {
    private:
        std::string logDirPath;
        std::string logFileName = "latest.log";
        bool consoleOutputEnabled;
        bool initialized = false;
        std::fstream fhandle;

        std::expected<std::string, bool> getPrevLogName();
        std::string getTimestamp(bool friendly = false);

    public:
        GLL_API Logger(bool consoleOutput = true);
        GLL_API ~Logger();

        GLL_API bool init(const std::string dirPath = "logs");
        GLL_API void log(LogLevel lv, const std::string& msg);

        /**
         * @brief Stream-built log message emitted when this temporary is destroyed.
         * @details Example: `logger(LogLevel::INFO) << "port " << port;`.
         */
        struct LogStreamer {
            Logger& parent;
            LogLevel level;
            std::stringstream ss;

            LogStreamer(Logger& p, LogLevel l) : parent(p), level(l) {}
            ~LogStreamer() { parent.log(level, ss.str()); }

            template<typename T>
            LogStreamer& operator<<(const T& val) {
                ss << val;
                return *this;
            }
        };

        LogStreamer operator()(LogLevel lv) {
            return LogStreamer(*this, lv);
        }
    };
}