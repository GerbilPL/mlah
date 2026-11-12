#include "glogger_c.h"
#include "glogger.hpp"

extern "C" {

// The opaque C handle stores a Logger; keep the representation cast inside this ABI boundary.
glogger_t* glogger_create(bool console_output) {
    return reinterpret_cast<glogger_t*>(new gLogger::Logger(console_output));
}

void glogger_destroy(glogger_t* logger) {
    if (logger != nullptr) {
        delete reinterpret_cast<gLogger::Logger*>(logger);
    }
}

bool glogger_init(glogger_t* logger, const char* dir_path) {
    if (logger == nullptr) return false;
    
    std::string path = (dir_path != nullptr) ? std::string(dir_path) : "logs";
    
    return reinterpret_cast<gLogger::Logger*>(logger)->init(path);
}

void glogger_log(glogger_t* logger, int level, const char* msg) {
    if (logger == nullptr || msg == nullptr) return;
    
    gLogger::LogLevel cpp_level = static_cast<gLogger::LogLevel>(level);
    
    reinterpret_cast<gLogger::Logger*>(logger)->log(cpp_level, std::string(msg));
}

}