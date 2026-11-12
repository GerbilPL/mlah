#include "glogger.hpp"
#include "glogger_c.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <atomic>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" int glogger_c_api_smoke(const char* directory);

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string timestamp_archive_stem(const std::string& header) {
    const auto value = header.starts_with("# glogger-start-v1:")
        ? header.substr(std::string("# glogger-start-v1:").size()) : header;
    return value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2) + "_" +
           value.substr(11, 2) + value.substr(14, 2) + value.substr(17, 2);
}

void test_concurrent_logging(const std::filesystem::path& root) {
    glogger::LoggerOptions options;
    options.directory = root / "concurrent";
    options.console_output = false;
    options.archive_policy = glogger::ArchivePolicy::Overwrite;
    glogger::Logger logger(options);
    require(logger.initialize().has_value(), "Concurrent logger initialization failed");
    require(logger.register_level(50, "CUSTOM").has_value(), "Custom level registration failed");
    require(!logger.register_level(50, "DUPLICATE"), "Duplicate level registration succeeded");
    require(!logger.register_level(4, "RESERVED"), "Reserved level registration succeeded");

    constexpr int thread_count = 8;
    constexpr int records_per_thread = 125;
    std::atomic<bool> write_failed{false};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([&, thread] {
            for (int record = 0; record < records_per_thread; ++record) {
                const auto message = "record-" + std::to_string(thread) + "-" +
                                     std::to_string(record);
                const auto result = logger.log(50, message);
                if (!result) write_failed.store(true, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    require(!write_failed.load(std::memory_order_relaxed), "A concurrent log call failed");
    require(logger.log(99, "unknown-level").has_value(), "Unknown level logging failed");
    require(logger.flush().has_value(), "Logger flush failed");

    std::ifstream input(options.directory / options.active_filename);
    require(static_cast<bool>(input), "Concurrent log file was not created");
    std::string line;
    std::set<std::string> records;
    std::size_t line_count = 0;
    while (std::getline(input, line)) {
        ++line_count;
        if (line.starts_with("# glogger-start-v1:")) continue;
        require(line.find("][CUSTOM]: record-") != std::string::npos ||
                    line.find("][LEVEL_99]: unknown-level") != std::string::npos,
                "A concurrent log record was incomplete or had an incorrect level");
        records.insert(line);
    }
    require(line_count == thread_count * records_per_thread + 2,
            "Concurrent writes did not produce the expected complete line count");
    require(records.size() == thread_count * records_per_thread + 1,
            "Concurrent writes were duplicated or missing");
}

void test_concurrent_registration(const std::filesystem::path& root) {
    glogger::LoggerOptions options;
    options.directory = root / "registration-race";
    options.console_output = false;
    glogger::Logger logger(options);
    require(logger.initialize().has_value(), "Registration-race logger initialization failed");

    constexpr int level_count = 20;
    constexpr int writer_count = 4;
    constexpr int records_per_writer = 500;
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::thread registrar([&] {
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (int level = 0; level < level_count; ++level) {
            if (!logger.register_level(100 + level, "CUSTOM_" + std::to_string(level))) {
                failed.store(true, std::memory_order_relaxed);
            }
        }
    });
    std::vector<std::thread> writers;
    for (int writer = 0; writer < writer_count; ++writer) {
        writers.emplace_back([&, writer] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int record = 0; record < records_per_writer; ++record) {
                const auto level = static_cast<std::uint32_t>(100 + record % level_count);
                if (!logger.log(level, "race-" + std::to_string(writer) + "-" +
                                           std::to_string(record))) {
                    failed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    start.store(true, std::memory_order_release);
    registrar.join();
    for (auto& writer : writers) writer.join();
    require(!failed.load(std::memory_order_relaxed), "Concurrent registration or logging failed");
    require(logger.flush().has_value(), "Registration-race flush failed");

    std::ifstream input(options.directory / options.active_filename);
    std::string line;
    std::size_t records = 0;
    while (std::getline(input, line)) {
        if (line.starts_with("# glogger-start-v1:")) continue;
        const auto level_begin = line.find("][") + 2;
        const auto level_end = line.find("]: ", level_begin);
        require(level_begin >= 2 && level_end != std::string::npos,
                "Concurrent registration produced a malformed record");
        const auto level_name = line.substr(level_begin, level_end - level_begin);
        require(level_name.starts_with("CUSTOM_") || level_name.starts_with("LEVEL_"),
                "Concurrent registration produced a torn level name");
        require(line.find("race-") != std::string::npos,
                "Concurrent registration produced a malformed message");
        ++records;
    }
    require(records == writer_count * records_per_writer,
            "Concurrent registration lost or duplicated records");
}

void test_archival(const std::filesystem::path& root) {
    const auto active_dir = root / "active";
    const auto archive_dir = root / "archives";
    glogger::LoggerOptions options;
    options.directory = active_dir;
    options.archive_directory = archive_dir;
    options.active_filename = "application.log";
    options.archive_policy = glogger::ArchivePolicy::HeaderTimestamp;
    options.console_output = false;

    {
        glogger::Logger logger(options);
        require(logger.initialize().has_value(), "Initial archive logger initialization failed");
        require(logger.log(glogger::Level::Info, "old active").has_value(),
                "Could not write initial active log");
        require(logger.flush().has_value(), "Could not flush initial active log");
    }
    const auto original = read_file(active_dir / "application.log");
    std::string header = original.substr(0, original.find('\n'));
    const auto expected = archive_dir / ("application." + timestamp_archive_stem(header) + ".log");
    {
        std::ofstream existing_archive(expected);
        existing_archive << "preexisting archive";
    }
    {
        glogger::Logger logger(options);
        require(logger.initialize().has_value(), "Header archive reinitialization failed");
    }
    require(read_file(expected) == "preexisting archive",
            "Timestamp collision overwrote an existing archive");
    require(read_file(archive_dir / ("application." + timestamp_archive_stem(header) +
                                     ".1.log")) == original,
            "Header timestamp archive did not preserve the old active log");
    require(read_file(active_dir / "application.log").starts_with("# glogger-start-v1:"),
            "New active log is missing its machine-readable header");

    options.archive_policy = glogger::ArchivePolicy::IncrementingNumber;
    {
        glogger::Logger logger(options);
        require(logger.initialize().has_value(), "Numbered archive initialization failed");
        require(logger.log(glogger::Level::Info, "numbered source").has_value(),
                "Could not write numbered source");
    }
    {
        glogger::Logger logger(options);
        require(logger.initialize().has_value(), "Numbered archive reinitialization failed");
    }
    require(std::filesystem::exists(archive_dir / "application.1.log"),
            "Incrementing-number archive was not created");
    require(std::filesystem::exists(archive_dir / "application.2.log"),
            "Incrementing-number archive did not choose the next free suffix");
}

void test_other_policies_and_encoding(const std::filesystem::path& root) {
    glogger::LoggerOptions metadata;
    metadata.directory = root / "metadata";
    metadata.archive_directory = root / "metadata-archives";
    metadata.active_filename = "metadata.log";
    metadata.archive_policy = glogger::ArchivePolicy::FileMetadataTimestamp;
    metadata.console_output = false;
    {
        glogger::Logger logger(metadata);
        require(logger.initialize().has_value(), "Metadata logger initialization failed");
        require(logger.log(glogger::Level::Info, "metadata source").has_value(),
                "Could not write metadata source");
    }
    {
        glogger::Logger logger(metadata);
        require(logger.initialize().has_value(), "Metadata archive initialization failed");
    }
    require(std::distance(std::filesystem::directory_iterator(metadata.archive_directory),
                          std::filesystem::directory_iterator{}) == 1,
            "Metadata timestamp archive was not created");

    glogger::LoggerOptions utf8;
    utf8.directory = root / "utf8";
    utf8.console_output = false;
    {
        glogger::Logger logger(utf8);
        require(logger.initialize().has_value(), "UTF-8 logger initialization failed");
        require(logger.log(glogger::Level::Info, "café").has_value(),
                "UTF-8 message write failed");
        require(logger.flush().has_value(), "UTF-8 logger flush failed");
    }
    require(read_file(utf8.directory / "latest.log").find("café") != std::string::npos,
            "UTF-8 output did not preserve the original message bytes");

    glogger::LoggerOptions escaped;
    escaped.directory = root / "escaped";
    escaped.console_output = false;
    escaped.text_encoding = glogger::TextEncoding::AsciiEscaped;
    glogger::Logger logger(escaped);
    require(logger.initialize().has_value(), "Escaped logger initialization failed");
    require(logger.register_level(5, "CAFÉ").has_value(),
            "Unicode custom level registration failed");
    require(logger.register_level(6, "BAD\nNAME").has_value(),
            "Custom level with newline registration failed");
    const std::string unicode = "é€😀";
    require(logger.log(glogger::Level::Info, unicode).has_value(),
            "Valid UTF-8 message was rejected");
    require(logger.log(5, "custom name").has_value() &&
                logger.log(6, "control name").has_value(),
            "Custom level name logging failed");
    const std::string malformed("\xc0\xaf", 2);
    const auto invalid = logger.log(glogger::Level::Info, malformed);
    require(!invalid && invalid.error().code == glogger::ErrorCode::InvalidUtf8,
            "Invalid UTF-8 message was not rejected");
    require(logger.flush().has_value(), "Escaped logger flush failed");
    const auto content = read_file(escaped.directory / "latest.log");
    require(content.find("\\u00E9\\u20AC\\U0001F600") != std::string::npos,
            "ASCII escaping did not encode BMP and supplementary characters");
    require(content.find("][CAF\\u00C9]: custom name") != std::string::npos &&
                content.find("][BAD\\u000ANAME]: control name") != std::string::npos,
            "Custom level metadata was not escaped safely");
    for (const unsigned char byte : content) {
        require(byte < 0x80, "ASCII escaped log contains a non-ASCII byte");
    }
}

void test_failed_archive_preserves_active(const std::filesystem::path& root) {
    glogger::LoggerOptions options;
    options.directory = root / "failure";
    options.active_filename = "preserve.log";
    options.archive_policy = glogger::ArchivePolicy::HeaderTimestamp;
    options.console_output = false;
    std::filesystem::create_directories(options.directory);
    {
        std::ofstream old(options.directory / options.active_filename);
        old << "invalid header\nold content\n";
    }
    glogger::Logger logger(options);
    const auto result = logger.initialize();
    require(!result && result.error().code == glogger::ErrorCode::Archival,
            "Invalid archive header did not report an archival error");
    require(read_file(options.directory / options.active_filename) ==
                "invalid header\nold content\n",
            "Archival failure damaged the previous active log");

    glogger::LoggerOptions active;
    active.directory = root / "reconfiguration-active";
    active.console_output = false;
    active.archive_policy = glogger::ArchivePolicy::Overwrite;
    glogger::Logger reconfigurable(active);
    require(reconfigurable.initialize().has_value(),
            "Initial reconfiguration logger initialization failed");
    require(reconfigurable.log(glogger::Level::Info, "before failed reconfiguration").has_value(),
            "Could not write before failed reconfiguration");
    glogger::LoggerOptions invalid_reconfiguration;
    invalid_reconfiguration.directory = root / "reconfiguration-invalid";
    invalid_reconfiguration.active_filename = "bad.log";
    invalid_reconfiguration.archive_policy = glogger::ArchivePolicy::HeaderTimestamp;
    invalid_reconfiguration.console_output = false;
    std::filesystem::create_directories(invalid_reconfiguration.directory);
    {
        std::ofstream bad(invalid_reconfiguration.directory / invalid_reconfiguration.active_filename);
        bad << "bad start header\n";
    }
    const auto reconfigure_result = reconfigurable.initialize(invalid_reconfiguration);
    require(!reconfigure_result &&
                reconfigure_result.error().code == glogger::ErrorCode::Archival,
            "Failed reconfiguration did not return its archival error");
    require(reconfigurable.log(glogger::Level::Info, "after failed reconfiguration").has_value(),
            "Logger did not recover its old destination after failed reconfiguration");
    require(reconfigurable.flush().has_value(), "Could not flush recovered logger");
    const auto prior_log = read_file(active.directory / active.active_filename);
    require(prior_log.find("before failed reconfiguration") != std::string::npos &&
                prior_log.find("after failed reconfiguration") != std::string::npos,
            "Failed reconfiguration lost or redirected the previous logger output");
}

void test_overwrite_and_c_api(const std::filesystem::path& root) {
    glogger::LoggerOptions options;
    options.directory = root / "overwrite";
    options.active_filename = "fresh.log";
    options.console_output = false;
    options.archive_policy = glogger::ArchivePolicy::Overwrite;
    std::filesystem::create_directories(options.directory);
    {
        std::ofstream old(options.directory / options.active_filename);
        old << "old data\n";
    }
    {
        glogger::Logger logger(options);
        require(logger.initialize().has_value(), "Overwrite logger initialization failed");
        require(logger.logf(glogger::Level::Info, "port {}", 8080).has_value(),
                "C++23 formatted logging failed");
    }
    const auto overwritten = read_file(options.directory / options.active_filename);
    require(overwritten.find("old data") == std::string::npos &&
                overwritten.find("port 8080") != std::string::npos,
            "Overwrite policy did not replace the active file");

    const auto c_directory = root / "c-api";
    std::filesystem::create_directories(c_directory);
    require(glogger_c_api_smoke(c_directory.string().c_str()) == 0,
            "C translation unit could not initialize and use the C API");
    require(read_file(c_directory / "c-api.log").find("][CUSTOM]: C caller") !=
                std::string::npos,
            "C API custom-level record was not written");
    require(glogger_log(nullptr, GLOGGER_LEVEL_INFO, "bad handle") ==
                GLOGGER_STATUS_INVALID_ARGUMENT,
            "C API did not reject a null logger handle");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "mlah-glogger-v2-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    try {
        std::filesystem::create_directories(root);
        test_concurrent_logging(root);
        test_concurrent_registration(root);
        test_archival(root);
        test_other_policies_and_encoding(root);
        test_failed_archive_preserves_active(root);
        test_overwrite_and_c_api(root);
        std::filesystem::remove_all(root, ec);
        std::cout << "All glogger tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "glogger test failure: " << error.what() << '\n';
        std::filesystem::remove_all(root, ec);
        return 1;
    }
}
