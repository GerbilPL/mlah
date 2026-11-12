#pragma once

#include "glogger_export.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glogger_opaque glogger_t;

typedef enum glogger_status {
    GLOGGER_STATUS_OK = 0,
    GLOGGER_STATUS_INVALID_ARGUMENT = 1,
    GLOGGER_STATUS_INVALID_UTF8 = 2,
    GLOGGER_STATUS_FILESYSTEM = 3,
    GLOGGER_STATUS_ARCHIVAL = 4,
    GLOGGER_STATUS_IO = 5,
    GLOGGER_STATUS_INTERNAL = 6
} glogger_status;

typedef enum glogger_archive_policy {
    GLOGGER_ARCHIVE_OVERWRITE = 0,
    GLOGGER_ARCHIVE_HEADER_TIMESTAMP = 1,
    GLOGGER_ARCHIVE_FILE_METADATA_TIMESTAMP = 2,
    GLOGGER_ARCHIVE_INCREMENTING_NUMBER = 3
} glogger_archive_policy;

typedef enum glogger_text_encoding {
    GLOGGER_ENCODING_UTF8 = 0,
    GLOGGER_ENCODING_ASCII_ESCAPED = 1
} glogger_text_encoding;

enum {
    GLOGGER_CONFIG_VERSION_1 = 1,
    GLOGGER_LEVEL_ERROR = 0,
    GLOGGER_LEVEL_DEBUG = 1,
    GLOGGER_LEVEL_WARN = 2,
    GLOGGER_LEVEL_INFO = 3,
    GLOGGER_LEVEL_CHAT = 4
};

typedef struct glogger_config {
    uint32_t struct_size;
    uint32_t version;
    const char* directory;
    const char* active_filename;
    const char* archive_directory;
    uint32_t archive_policy;
    uint32_t console_output;
    uint32_t text_encoding;
} glogger_config;

GLOGGER_API glogger_t* glogger_create(void);
GLOGGER_API void glogger_destroy(glogger_t* logger);
GLOGGER_API glogger_status glogger_init(glogger_t* logger,
                                         const glogger_config* config);
GLOGGER_API glogger_status glogger_register_level(glogger_t* logger,
                                                   uint32_t level_id,
                                                   const char* name);
GLOGGER_API glogger_status glogger_log(glogger_t* logger,
                                       uint32_t level_id,
                                       const char* message);
GLOGGER_API glogger_status glogger_flush(glogger_t* logger);

#ifdef __cplusplus
}
#endif
