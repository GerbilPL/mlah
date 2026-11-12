#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(GLOGGER_BUILD_SHARED)
        #if defined(GLOGGER_EXPORTS)
            #define GLOGGER_API __declspec(dllexport)
        #else
            #define GLOGGER_API __declspec(dllimport)
        #endif
    #else
        #define GLOGGER_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define GLOGGER_API __attribute__((visibility("default")))
#else
    #define GLOGGER_API
#endif
