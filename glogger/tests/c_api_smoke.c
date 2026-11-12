#include "glogger_c.h"

#include <stddef.h>

int glogger_c_api_smoke(const char* directory) {
    glogger_t* logger = glogger_create();
    if (logger == NULL) return 1;

    glogger_config config = {0};
    config.struct_size = (uint32_t)sizeof(config);
    config.version = GLOGGER_CONFIG_VERSION_1;
    config.directory = directory;
    config.active_filename = "c-api.log";
    config.archive_policy = GLOGGER_ARCHIVE_OVERWRITE;
    config.console_output = 0;
    config.text_encoding = GLOGGER_ENCODING_UTF8;

    glogger_status status = glogger_init(logger, &config);
    if (status == GLOGGER_STATUS_OK) {
        status = glogger_register_level(logger, 10, "CUSTOM");
    }
    if (status == GLOGGER_STATUS_OK) {
        status = glogger_log(logger, 10, "C caller");
    }
    if (status == GLOGGER_STATUS_OK) {
        status = glogger_flush(logger);
    }
    glogger_destroy(logger);
    return status == GLOGGER_STATUS_OK ? 0 : 1;
}
