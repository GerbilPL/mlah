#include "gLogger.hpp"

namespace gLogger
{
    Logger::Logger(bool consoleOutput) 
        : consoleOutputEnabled(consoleOutput), initialized(false) {}

    Logger::~Logger() {
        if (fhandle.is_open()) fhandle.close();
    }

    std::string Logger::getTimestamp(bool friendly) {
        const auto now = std::chrono::system_clock::now();
        const auto now_sec = std::chrono::floor<std::chrono::seconds>(now);
        if (friendly) return std::format("{:%Y.%m.%d %H:%M:%S}", now_sec);
        return std::format("{:%Y%m%d_%H%M%S}", now_sec);
    }

    std::expected<std::string, bool> Logger::getPrevLogName() {
        std::ifstream tmp(logDirPath + logFileName);
        if (!tmp.is_open()) return std::unexpected(false);
        
        std::string filename;
        if (!std::getline(tmp, filename)) return std::unexpected(false);
        
        if (!filename.empty() && filename.back() == '\r') filename.pop_back(); 
        return filename;
    }

    bool Logger::init(const std::string dirPath) {
        logDirPath = dirPath;
        if (!logDirPath.empty() && logDirPath.back() != '/' && logDirPath.back() != '\\') {
            logDirPath += "/";
        }

        if (!std::filesystem::exists(logDirPath) && !std::filesystem::create_directories(logDirPath)) {
            std::cerr << "ERR: Logger directory creation failed: " << logDirPath << std::endl;
            return false;
        }

        std::string fullPath = logDirPath + logFileName;
        if (std::filesystem::exists(fullPath)) {
            auto result = getPrevLogName();
            if (result) {
                std::string newName = logDirPath + result.value() + ".log";
                try {
                    std::filesystem::rename(fullPath, newName);
                } catch (...) {
                    std::cerr << "ERR: Could not rename old log file." << std::endl;
                }
            }
        }

        fhandle.open(fullPath, std::ios::out | std::ios::trunc);
        if (!fhandle.is_open()) return false;

        initialized = true;
        fhandle << getTimestamp() << "\n";
        fhandle.flush();
        return true;
    }

    void Logger::log(LogLevel lv, const std::string& msg) {
        if (!initialized) return;

        std::string logmess = std::format("[{}][{}]: {}", getTimestamp(true), LogLevelStr(lv), msg);
        
        if (consoleOutputEnabled) std::cout << logmess << std::endl;
        fhandle << logmess << "\n";
        fhandle.flush(); // Zapewnia zapis przy crashu
    }
}