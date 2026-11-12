#include "glogger_c.h"
#include "glogger.hpp"

#include <cstddef>
#include <new>
#include <string>

struct glogger_opaque {
    glogger::Logger logger;
};

namespace {

glogger_status to_status(glogger::ErrorCode code) {
    switch (code) {
        case glogger::ErrorCode::InvalidArgument: return GLOGGER_STATUS_INVALID_ARGUMENT;
        case glogger::ErrorCode::InvalidUtf8: return GLOGGER_STATUS_INVALID_UTF8;
        case glogger::ErrorCode::Filesystem: return GLOGGER_STATUS_FILESYSTEM;
        case glogger::ErrorCode::Archival: return GLOGGER_STATUS_ARCHIVAL;
        case glogger::ErrorCode::Io: return GLOGGER_STATUS_IO;
    }
    return GLOGGER_STATUS_INTERNAL;
}

glogger_status from_result(const glogger::Result& result) {
    return result ? GLOGGER_STATUS_OK : to_status(result.error().code);
}

glogger::LoggerOptions options_from_config(const glogger_config& config) {
    glogger::LoggerOptions options;
    if (config.directory != nullptr) options.directory = config.directory;
    if (config.active_filename != nullptr) options.active_filename = config.active_filename;
    if (config.archive_directory != nullptr) options.archive_directory = config.archive_directory;
    options.archive_policy = static_cast<glogger::ArchivePolicy>(config.archive_policy);
    options.console_output = config.console_output != 0;
    options.text_encoding = static_cast<glogger::TextEncoding>(config.text_encoding);
    return options;
}

template <typename Function>
glogger_status protect_boundary(Function&& function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc&) {
        return GLOGGER_STATUS_INTERNAL;
    } catch (...) {
        return GLOGGER_STATUS_INTERNAL;
    }
}

} // namespace

extern "C" {

glogger_t* glogger_create(void) {
    try {
        return new glogger_t{};
    } catch (...) {
        return nullptr;
    }
}

void glogger_destroy(glogger_t* logger) {
    try {
        delete logger;
    } catch (...) {
    }
}

glogger_status glogger_init(glogger_t* logger, const glogger_config* config) {
    if (logger == nullptr || config == nullptr) return GLOGGER_STATUS_INVALID_ARGUMENT;
    return protect_boundary([&] {
        constexpr auto version_size = offsetof(glogger_config, version) + sizeof(uint32_t);
        if (config->struct_size < version_size) return GLOGGER_STATUS_INVALID_ARGUMENT;
        constexpr auto required_size = sizeof(glogger_config);
        if (config->version != GLOGGER_CONFIG_VERSION_1 ||
            config->struct_size < required_size) {
            return GLOGGER_STATUS_INVALID_ARGUMENT;
        }
        return from_result(logger->logger.initialize(options_from_config(*config)));
    });
}

glogger_status glogger_register_level(glogger_t* logger,
                                      uint32_t level_id,
                                      const char* name) {
    if (logger == nullptr || name == nullptr) return GLOGGER_STATUS_INVALID_ARGUMENT;
    return protect_boundary([&] {
        return from_result(logger->logger.register_level(level_id, name));
    });
}

glogger_status glogger_log(glogger_t* logger, uint32_t level_id, const char* message) {
    if (logger == nullptr || message == nullptr) return GLOGGER_STATUS_INVALID_ARGUMENT;
    return protect_boundary([&] {
        return from_result(logger->logger.log(level_id, message));
    });
}

glogger_status glogger_flush(glogger_t* logger) {
    if (logger == nullptr) return GLOGGER_STATUS_INVALID_ARGUMENT;
    return protect_boundary([&] {
        return from_result(logger->logger.flush());
    });
}

} // extern "C"
