#pragma once

#include "glogger_export.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace glogger {

enum class Level : std::uint32_t {
    Error = 0,
    Debug = 1,
    Warn = 2,
    Info = 3,
    Chat = 4
};

enum class ArchivePolicy : std::uint32_t {
    Overwrite = 0,
    HeaderTimestamp = 1,
    FileMetadataTimestamp = 2,
    IncrementingNumber = 3
};

enum class TextEncoding : std::uint32_t {
    Utf8 = 0,
    AsciiEscaped = 1
};

enum class ErrorCode : std::uint32_t {
    InvalidArgument,
    InvalidUtf8,
    Filesystem,
    Archival,
    Io
};

struct Error {
    ErrorCode code;
    std::string message;
};

using Result = std::expected<void, Error>;

struct LoggerOptions {
    std::filesystem::path directory = "logs";
    std::string active_filename = "latest.log";
    std::filesystem::path archive_directory;
    ArchivePolicy archive_policy = ArchivePolicy::Overwrite;
    bool console_output = true;
    TextEncoding text_encoding = TextEncoding::Utf8;
};

class GLOGGER_API Logger {
public:
    explicit Logger(LoggerOptions options = {});
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    Result initialize();
    Result initialize(LoggerOptions options);
    Result log(std::uint32_t level_id, std::string_view message);
    Result log(Level level, std::string_view message) {
        return log(static_cast<std::uint32_t>(level), message);
    }
    Result register_level(std::uint32_t level_id, std::string_view name);
    Result flush();

    template <typename... Args>
    Result logf(std::uint32_t level_id,
                std::format_string<Args...> format_string,
                Args&&... args) {
        return log(level_id, std::format(format_string, std::forward<Args>(args)...));
    }

    template <typename... Args>
    Result logf(Level level, std::format_string<Args...> format_string, Args&&... args) {
        return logf(static_cast<std::uint32_t>(level), format_string,
                    std::forward<Args>(args)...);
    }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glogger
