#pragma once

#include "glogger_export.h"
#include <stdbool.h>

/** @brief Opaque handle to a logger instance; create and destroy it with the C API. */
typedef struct glogger_opaque glogger_t;

#ifdef __cplusplus
extern "C" {
#endif

GLL_API glogger_t* glogger_create(bool console_output);
GLL_API void glogger_destroy(glogger_t* logger);

GLL_API bool glogger_init(glogger_t* logger, const char* dir_path);

/**
 * @brief Write a message using a numeric `gLogger::LogLevel` value.
 * @details Values are 0=ERR, 1=DBG, 2=WARN, 3=INFO, and 4=CHAT.
 */
GLL_API void glogger_log(glogger_t* logger, int level, const char* msg);

#ifdef __cplusplus
}
#endif