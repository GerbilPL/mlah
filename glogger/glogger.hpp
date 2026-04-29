#pragma once
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
    enum class LogLevel
    {
        ERR = 0, // ERROR to często makro w Windows.h, lepiej ERR
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
        Logger(bool consoleOutput = true);
        ~Logger();

        bool init(const std::string dirPath = "logs");
        void log(LogLevel lv, const std::string& msg);

        // Helper do obsługi streamowania: logger(LogLevel::INFO) << "msg";
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