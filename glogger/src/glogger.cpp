#include "glogger.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

#if defined(__linux__)
    #include <fcntl.h>
    #include <sys/stat.h>
#elif defined(__APPLE__)
    #include <sys/stat.h>
#elif defined(_WIN32)
    #define NOMINMAX
    #include <windows.h>
#endif

namespace glogger {
namespace {

constexpr std::uint32_t highest_builtin_level = 4;
std::atomic<std::uint64_t> temporary_directory_counter{0};

Error make_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

std::string filesystem_message(std::string_view operation,
                               const std::filesystem::path& path,
                               const std::error_code& ec) {
    return std::string(operation) + " '" + path.string() + "': " + ec.message();
}

std::string current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const std::time_t time = std::chrono::system_clock::to_time_t(seconds);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream result;
    result << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

std::string format_time(std::chrono::system_clock::time_point value) {
    const std::time_t time = std::chrono::system_clock::to_time_t(value);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream result;
    result << std::put_time(&utc, "%Y%m%d_%H%M%S");
    return result.str();
}

bool valid_utf8(std::string_view input) {
    std::size_t i = 0;
    while (i < input.size()) {
        const auto first = static_cast<unsigned char>(input[i]);
        std::uint32_t scalar = 0;
        std::size_t length = 0;
        if (first <= 0x7f) {
            scalar = first;
            length = 1;
        } else if (first >= 0xc2 && first <= 0xdf) {
            scalar = first & 0x1f;
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            scalar = first & 0x0f;
            length = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            scalar = first & 0x07;
            length = 4;
        } else {
            return false;
        }
        if (i + length > input.size()) return false;
        for (std::size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(input[i + j]);
            if ((next & 0xc0) != 0x80) return false;
            scalar = (scalar << 6) | (next & 0x3f);
        }
        if ((length == 2 && scalar < 0x80) ||
            (length == 3 && scalar < 0x800) ||
            (length == 4 && scalar < 0x10000) ||
            (scalar >= 0xd800 && scalar <= 0xdfff) ||
            scalar > 0x10ffff) {
            return false;
        }
        i += length;
    }
    return true;
}

std::string encode_message(std::string_view input, TextEncoding encoding) {
    if (encoding == TextEncoding::Utf8) return std::string(input);
    std::string result;
    result.reserve(input.size());
    constexpr char hex[] = "0123456789ABCDEF";
    std::size_t i = 0;
    while (i < input.size()) {
        const auto first = static_cast<unsigned char>(input[i]);
        std::uint32_t scalar = 0;
        std::size_t length = 1;
        if (first <= 0x7f) {
            scalar = first;
        } else if (first <= 0xdf) {
            scalar = first & 0x1f;
            length = 2;
        } else if (first <= 0xef) {
            scalar = first & 0x0f;
            length = 3;
        } else {
            scalar = first & 0x07;
            length = 4;
        }
        for (std::size_t j = 1; j < length; ++j) {
            scalar = (scalar << 6) |
                     (static_cast<unsigned char>(input[i + j]) & 0x3f);
        }
        i += length;
        if (scalar < 0x80) {
            result.push_back(static_cast<char>(scalar));
            continue;
        }
        const unsigned digits = scalar <= 0xffff ? 4 : 8;
        result.append(digits == 4 ? "\\u" : "\\U");
        for (unsigned shift = digits * 4; shift > 0; shift -= 4) {
            result.push_back(hex[(scalar >> (shift - 4)) & 0xf]);
        }
    }
    return result;
}

std::string encode_level_name(std::string_view name) {
    std::string escaped = encode_message(name, TextEncoding::AsciiEscaped);
    std::string result;
    result.reserve(escaped.size());
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char value : escaped) {
        if (value < 0x20 || value == 0x7f) {
            result += "\\u00";
            result.push_back(hex[value >> 4]);
            result.push_back(hex[value & 0x0f]);
        } else {
            result.push_back(static_cast<char>(value));
        }
    }
    return result;
}

bool valid_archive_timestamp(std::string_view value) {
    if (value.size() == 15 && value[8] == '_') {
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i != 8 && (value[i] < '0' || value[i] > '9')) return false;
        }
        return true;
    }
    if (value.size() == 20 && value[4] == '-' && value[7] == '-' &&
        value[10] == 'T' && value[13] == ':' && value[16] == ':' &&
        value[19] == 'Z') {
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 &&
                i != 19 && (value[i] < '0' || value[i] > '9')) return false;
        }
        return true;
    }
    return false;
}

std::string archive_stamp(std::string_view value) {
    if (value.size() == 15) return std::string(value);
    return std::string(value.substr(0, 4)) + std::string(value.substr(5, 2)) +
           std::string(value.substr(8, 2)) + "_" + std::string(value.substr(11, 2)) +
           std::string(value.substr(14, 2)) + std::string(value.substr(17, 2));
}

std::string built_in_name(std::uint32_t level) {
    switch (level) {
        case 0: return "ERROR";
        case 1: return "DEBUG";
        case 2: return "WARN";
        case 3: return "INFO";
        case 4: return "CHAT";
        default: return "LEVEL_" + std::to_string(level);
    }
}

std::filesystem::path numbered_archive_path(const std::filesystem::path& directory,
                                             const std::filesystem::path& active,
                                             std::string_view suffix) {
    const auto stem = active.stem().string();
    const auto extension = active.extension().string();
    for (std::uint64_t collision = 0;; ++collision) {
        std::string name = stem + "." + std::string(suffix);
        if (collision != 0) name += "." + std::to_string(collision);
        name += extension;
        const auto candidate = directory / name;
        std::error_code ec;
        const bool exists = std::filesystem::exists(candidate, ec);
        if (ec) return {};
        if (!exists) return candidate;
    }
}

Result move_preserving_source(const std::filesystem::path& source,
                              const std::filesystem::path& destination,
                              ErrorCode failure_code) {
    std::error_code ec;
    std::filesystem::create_hard_link(source, destination, ec);
    if (!ec) {
        std::filesystem::remove(source, ec);
        if (!ec) return {};
        std::error_code cleanup_error;
        std::filesystem::remove(destination, cleanup_error);
        return std::unexpected(make_error(
            failure_code, filesystem_message("Could not remove preserved source", source, ec)));
    }

    ec.clear();
    std::filesystem::copy_file(source, destination,
                               std::filesystem::copy_options::none, ec);
    if (ec) {
        std::error_code cleanup_error;
        std::filesystem::remove(destination, cleanup_error);
        return std::unexpected(make_error(
            failure_code, filesystem_message("Could not preserve log at", source, ec)));
    }
    std::filesystem::remove(source, ec);
    if (ec) {
        std::error_code cleanup_error;
        std::filesystem::remove(destination, cleanup_error);
        return std::unexpected(make_error(
            failure_code, filesystem_message("Could not remove preserved source", source, ec)));
    }
    return {};
}

std::expected<std::chrono::system_clock::time_point, std::error_code>
metadata_timestamp(const std::filesystem::path& path) {
#if defined(__linux__) && defined(STATX_BTIME)
    struct statx metadata {};
    if (::statx(AT_FDCWD, path.c_str(), AT_STATX_SYNC_AS_STAT, STATX_BTIME, &metadata) == 0 &&
        (metadata.stx_mask & STATX_BTIME) != 0) {
        const auto seconds = std::chrono::seconds(metadata.stx_btime.tv_sec);
        const auto nanoseconds = std::chrono::nanoseconds(metadata.stx_btime.tv_nsec);
        return std::chrono::system_clock::time_point(seconds + nanoseconds);
    }
#elif defined(__APPLE__)
    struct stat metadata {};
    if (::stat(path.c_str(), &metadata) == 0) {
        return std::chrono::system_clock::from_time_t(metadata.st_birthtimespec.tv_sec) +
               std::chrono::nanoseconds(metadata.st_birthtimespec.tv_nsec);
    }
#elif defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA metadata{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &metadata)) {
        ULARGE_INTEGER ticks{};
        ticks.LowPart = metadata.ftCreationTime.dwLowDateTime;
        ticks.HighPart = metadata.ftCreationTime.dwHighDateTime;
        constexpr std::uint64_t windows_to_unix_epoch = 116444736000000000ULL;
        if (ticks.QuadPart >= windows_to_unix_epoch) {
            const auto nanoseconds = std::chrono::nanoseconds(
                (ticks.QuadPart - windows_to_unix_epoch) * 100);
            return std::chrono::system_clock::time_point(nanoseconds);
        }
    }
#endif
    std::error_code ec;
    const auto file_time = std::filesystem::last_write_time(path, ec);
    if (ec) return std::unexpected(ec);
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - decltype(file_time)::clock::now() +
        std::chrono::system_clock::now());
}

} // namespace

class Logger::Impl {
public:
    explicit Impl(LoggerOptions value) : options(std::move(value)) {
        levels.emplace(0, "ERROR");
        levels.emplace(1, "DEBUG");
        levels.emplace(2, "WARN");
        levels.emplace(3, "INFO");
        levels.emplace(4, "CHAT");
    }

    Result initialize_current() {
        std::lock_guard lock(mutex);
        return initialize_locked(options);
    }

    Result initialize(LoggerOptions candidate_options) {
        std::lock_guard lock(mutex);
        return initialize_locked(std::move(candidate_options));
    }

    Result initialize_locked(LoggerOptions candidate_options) {
        if (candidate_options.active_filename.empty()) {
            return std::unexpected(make_error(ErrorCode::InvalidArgument,
                                              "active_filename must not be empty"));
        }
        const std::filesystem::path active_name(candidate_options.active_filename);
        if (active_name.has_parent_path() || active_name.filename() != active_name ||
            candidate_options.active_filename == "." || candidate_options.active_filename == "..") {
            return std::unexpected(make_error(
                ErrorCode::InvalidArgument,
                "active_filename must be a filename without directory components"));
        }
        if (candidate_options.archive_policy > ArchivePolicy::IncrementingNumber ||
            candidate_options.text_encoding > TextEncoding::AsciiEscaped) {
            return std::unexpected(make_error(ErrorCode::InvalidArgument,
                                              "unknown archive policy or text encoding"));
        }

        const auto archive_directory = candidate_options.archive_directory.empty()
            ? candidate_options.directory : candidate_options.archive_directory;
        std::error_code ec;
        std::filesystem::create_directories(candidate_options.directory, ec);
        if (ec) {
            return std::unexpected(make_error(
                ErrorCode::Filesystem,
                filesystem_message("Could not create log directory", candidate_options.directory, ec)));
        }
        std::filesystem::create_directories(archive_directory, ec);
        if (ec) {
            return std::unexpected(make_error(
                ErrorCode::Filesystem,
                filesystem_message("Could not create archive directory", archive_directory, ec)));
        }

        const auto active_path = candidate_options.directory / active_name;
        const auto temporary_dir = make_temporary_directory(candidate_options.directory);
        if (temporary_dir.empty()) {
            return std::unexpected(make_error(ErrorCode::Filesystem,
                                              "Could not create a temporary log directory"));
        }
        const auto staged_path = temporary_dir / active_name;
        {
            std::ofstream staged(staged_path, std::ios::binary | std::ios::out);
            if (!staged) {
                std::filesystem::remove_all(temporary_dir, ec);
                return std::unexpected(make_error(
                    ErrorCode::Io, "Could not open staged log file '" + staged_path.string() + "'"));
            }
            staged << "# glogger-start-v1:" << current_timestamp() << '\n';
            staged.flush();
            if (!staged) {
                staged.close();
                std::filesystem::remove_all(temporary_dir, ec);
                return std::unexpected(make_error(ErrorCode::Io,
                                                  "Could not write staged log header"));
            }
        }

        if (stream.is_open()) {
            stream.flush();
            if (!stream) {
                std::filesystem::remove_all(temporary_dir, ec);
                return std::unexpected(make_error(ErrorCode::Io,
                                                  "Could not flush the current log before reinitializing"));
            }
            stream.close();
        }
        const auto previous_path = opened_path;
        const bool had_previous = initialized;
        initialized = false;

        bool had_old_active = std::filesystem::exists(active_path, ec);
        if (ec) {
            std::filesystem::remove_all(temporary_dir, ec);
            restore_previous(previous_path, had_previous);
            return std::unexpected(make_error(ErrorCode::Filesystem,
                                              "Could not inspect the active log file"));
        }

        std::filesystem::path archive_path;
        std::filesystem::path backup_path;
        if (had_old_active) {
            if (candidate_options.archive_policy == ArchivePolicy::Overwrite) {
                backup_path = temporary_dir / "previous.log";
                std::filesystem::rename(active_path, backup_path, ec);
                if (ec) {
                    cleanup_temp(temporary_dir);
                    restore_previous(previous_path, had_previous);
                    return std::unexpected(make_error(
                        ErrorCode::Filesystem,
                        filesystem_message("Could not stage existing active log", active_path, ec)));
                }
            } else {
                auto planned = choose_archive_path(active_path, archive_directory,
                                                   candidate_options.archive_policy);
                if (!planned) {
                    cleanup_temp(temporary_dir);
                    restore_previous(previous_path, had_previous);
                    return std::unexpected(planned.error());
                }
                archive_path = *planned;
                for (;;) {
                    auto preserved = move_preserving_source(active_path, archive_path,
                                                            ErrorCode::Archival);
                    if (preserved) break;
                    std::error_code collision_error;
                    const bool collision = std::filesystem::exists(archive_path, collision_error);
                    if (collision && !collision_error) {
                        planned = choose_archive_path(active_path, archive_directory,
                                                      candidate_options.archive_policy);
                        if (!planned) {
                            cleanup_temp(temporary_dir);
                            restore_previous(previous_path, had_previous);
                            return std::unexpected(planned.error());
                        }
                        archive_path = *planned;
                        continue;
                    }
                    cleanup_temp(temporary_dir);
                    restore_previous(previous_path, had_previous);
                    return preserved;
                }
            }
        }

        std::filesystem::rename(staged_path, active_path, ec);
        if (ec) {
            if (had_old_active) {
                const auto saved_path = candidate_options.archive_policy == ArchivePolicy::Overwrite
                    ? backup_path : archive_path;
                std::error_code restore_error;
                std::filesystem::rename(saved_path, active_path, restore_error);
                if (restore_error) {
                    restore_previous(previous_path, had_previous);
                    cleanup_temp(temporary_dir);
                    return std::unexpected(make_error(
                        ErrorCode::Filesystem,
                        filesystem_message("Could not install new log or restore prior log",
                                           active_path, restore_error)));
                }
            }
            cleanup_temp(temporary_dir);
            restore_previous(previous_path, had_previous);
            return std::unexpected(make_error(
                ErrorCode::Filesystem,
                filesystem_message("Could not install new active log", active_path, ec)));
        }
        cleanup_temp(temporary_dir);

        stream.clear();
        stream.open(active_path, std::ios::binary | std::ios::out | std::ios::app);
        if (!stream) {
            initialized = false;
            opened_path.clear();
            return std::unexpected(make_error(ErrorCode::Io,
                                              "Could not open initialized active log for writing"));
        }
        opened_path = active_path;
        initialized = true;
        options = std::move(candidate_options);
        return {};
    }

    Result log(std::uint32_t level_id, std::string_view message) {
        if (!valid_utf8(message)) {
            return std::unexpected(make_error(ErrorCode::InvalidUtf8,
                                              "message is not valid UTF-8"));
        }
        std::lock_guard lock(mutex);
        if (!initialized || !stream.is_open()) {
            return std::unexpected(make_error(ErrorCode::Io, "logger is not initialized"));
        }
        const std::string encoded = encode_message(message, options.text_encoding);
        const auto found = levels.find(level_id);
        const std::string raw_level_name = found == levels.end()
            ? built_in_name(level_id) : found->second;
        const std::string level_name = encode_level_name(raw_level_name);
        const std::string record = "[" + current_timestamp() + "][" + level_name +
                                   "]: " + encoded + "\n";
        stream.write(record.data(), static_cast<std::streamsize>(record.size()));
        stream.flush();
        if (!stream) {
            return std::unexpected(make_error(ErrorCode::Io, "Could not write log record"));
        }
        if (options.console_output) {
            std::cout.write(record.data(), static_cast<std::streamsize>(record.size()));
            std::cout.flush();
            if (!std::cout) {
                return std::unexpected(make_error(ErrorCode::Io,
                                                  "Could not write log record to console"));
            }
        }
        return {};
    }

    Result register_level(std::uint32_t level_id, std::string_view name) {
        if (level_id <= highest_builtin_level) {
            return std::unexpected(make_error(ErrorCode::InvalidArgument,
                                              "custom level ID must be greater than 4"));
        }
        if (name.empty()) {
            return std::unexpected(make_error(ErrorCode::InvalidArgument,
                                              "custom level name must not be empty"));
        }
        if (!valid_utf8(name)) {
            return std::unexpected(make_error(ErrorCode::InvalidUtf8,
                                              "level name is not valid UTF-8"));
        }
        std::lock_guard lock(mutex);
        if (levels.contains(level_id)) {
            return std::unexpected(make_error(ErrorCode::InvalidArgument,
                                              "level ID is already registered"));
        }
        levels.emplace(level_id, name);
        return {};
    }

    Result flush() {
        std::lock_guard lock(mutex);
        if (!initialized || !stream.is_open()) {
            return std::unexpected(make_error(ErrorCode::Io, "logger is not initialized"));
        }
        stream.flush();
        if (!stream) return std::unexpected(make_error(ErrorCode::Io, "Could not flush log file"));
        if (options.console_output) {
            std::cout.flush();
            if (!std::cout) {
                return std::unexpected(make_error(ErrorCode::Io, "Could not flush console output"));
            }
        }
        return {};
    }

private:
    std::filesystem::path make_temporary_directory(const std::filesystem::path& parent) {
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            const auto id = temporary_directory_counter.fetch_add(1, std::memory_order_relaxed);
            const auto candidate = parent / (".glogger-tmp-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(id));
            std::error_code ec;
            if (std::filesystem::create_directory(candidate, ec)) return candidate;
            if (ec && ec != std::errc::file_exists) return {};
        }
        return {};
    }

    void cleanup_temp(const std::filesystem::path& path) {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    void restore_previous(const std::filesystem::path& path, bool was_initialized) {
        if (!was_initialized || path.empty()) return;
        stream.clear();
        stream.open(path, std::ios::binary | std::ios::out | std::ios::app);
        if (stream) {
            opened_path = path;
            initialized = true;
        }
    }

    std::expected<std::filesystem::path, Error> choose_archive_path(
        const std::filesystem::path& active_path,
        const std::filesystem::path& archive_directory,
        ArchivePolicy archive_policy) {
        if (archive_policy == ArchivePolicy::IncrementingNumber) {
            for (std::uint64_t number = 1;; ++number) {
                const auto candidate = archive_directory /
                    (active_path.stem().string() + "." + std::to_string(number) +
                     active_path.extension().string());
                std::error_code ec;
                const bool exists = std::filesystem::exists(candidate, ec);
                if (ec) {
                    return std::unexpected(make_error(
                        ErrorCode::Archival,
                        filesystem_message("Could not inspect archive path", candidate, ec)));
                }
                if (!exists) return candidate;
            }
        }

        std::string stamp;
        if (archive_policy == ArchivePolicy::HeaderTimestamp) {
            std::ifstream previous(active_path, std::ios::binary);
            std::string header;
            if (!previous || !std::getline(previous, header)) {
                return std::unexpected(make_error(
                    ErrorCode::Archival, "Could not read the previous log start-time header"));
            }
            constexpr std::string_view prefix = "# glogger-start-v1:";
            if (header.starts_with(prefix)) {
                header.erase(0, prefix.size());
            }
            if (!header.empty() && header.back() == '\r') header.pop_back();
            if (!valid_archive_timestamp(header)) {
                return std::unexpected(make_error(
                    ErrorCode::Archival, "Previous log has an invalid or unsupported start-time header"));
            }
            stamp = archive_stamp(header);
        } else {
            const auto timestamp = metadata_timestamp(active_path);
            if (!timestamp) {
                return std::unexpected(make_error(
                    ErrorCode::Archival,
                    filesystem_message("Could not read active log metadata time", active_path,
                                       timestamp.error())));
            }
            stamp = format_time(*timestamp);
        }

        const auto candidate = numbered_archive_path(archive_directory, active_path, stamp);
        if (candidate.empty()) {
            return std::unexpected(make_error(ErrorCode::Archival,
                                              "Could not find an unused archive filename"));
        }
        return candidate;
    }

    LoggerOptions options;
    std::mutex mutex;
    std::map<std::uint32_t, std::string> levels;
    std::ofstream stream;
    std::filesystem::path opened_path;
    bool initialized = false;
};

Logger::Logger(LoggerOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Logger::~Logger() {
    if (impl_) {
        auto ignored = impl_->flush();
        (void)ignored;
    }
}
Result Logger::initialize() { return impl_->initialize_current(); }
Result Logger::initialize(LoggerOptions options) {
    return impl_->initialize(std::move(options));
}
Result Logger::log(std::uint32_t level_id, std::string_view message) {
    return impl_->log(level_id, message);
}
Result Logger::register_level(std::uint32_t level_id, std::string_view name) {
    return impl_->register_level(level_id, name);
}
Result Logger::flush() { return impl_->flush(); }

} // namespace glogger
