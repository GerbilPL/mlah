#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef GLOGGER_EXPORTS
        #define GLL_API __declspec(dllexport)
    #else
        #define GLL_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define GLL_API __attribute__ ((visibility ("default")))
    #else
        #define GLL_API
    #endif
#endif