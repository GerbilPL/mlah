#include "glogger.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
    #include <sys/sysinfo.h>
    #include <sys/utsname.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t producer_count = 100;
constexpr auto warmup_duration = std::chrono::seconds(5);
constexpr auto measurement_duration = std::chrono::seconds(60);
constexpr auto record_period = std::chrono::milliseconds(10);
constexpr std::uint64_t maximum_output_bytes = 5ULL * 1024 * 1024 * 1024;
constexpr std::uint64_t log_header_bytes = 40;
constexpr std::uint64_t record_overhead_bytes = 31;
std::atomic<std::uint64_t> output_bytes_reserved{0};

std::string compiler_name() {
#if defined(__clang__)
    return std::string("Clang ") + __clang_version__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#else
    return "unknown";
#endif
}

std::string benchmark_payload() {
    std::string result;
    result.reserve(128 * 10);
    result.append(16, '0');
    result.append(112, 'a');
    for (int i = 0; i < 128; ++i) {
        result += "\xc3\xa9";
        result += "\xe2\x80\x80";
        result += "\xf0\x90\x80\x80";
    }
    return result;
}

std::string escaped_payload() {
    std::string result;
    result.reserve(128 * 23);
    result.append(16, '0');
    result.append(112, 'a');
    for (int i = 0; i < 128; ++i) {
        result += "\\u00E9\\u2000\\U00010000";
    }
    return result;
}

void write_record_id(std::string& payload, std::uint64_t id) {
    constexpr char hex[] = "0123456789ABCDEF";
    for (std::size_t digit = 0; digit < 16; ++digit) {
        const auto shift = static_cast<unsigned>((15 - digit) * 4);
        payload[digit] = hex[(id >> shift) & 0x0f];
    }
}

void report_system() {
    std::cout << "compiler=" << compiler_name() << '\n'
              << "hardware_threads=" << std::thread::hardware_concurrency() << '\n'
              << "logger_mode=synchronous-mutex\n"
#if defined(NDEBUG)
              << "build_configuration=Release\n"
#else
              << "build_configuration=Debug\n"
#endif
              << "storage_device="
              << (std::getenv("GLOGGER_BENCH_STORAGE_DEVICE")
                      ? std::getenv("GLOGGER_BENCH_STORAGE_DEVICE") : "not supplied")
              << '\n'
              << "filesystem="
              << (std::getenv("GLOGGER_BENCH_FILESYSTEM")
                      ? std::getenv("GLOGGER_BENCH_FILESYSTEM") : "not supplied")
              << '\n';
#if defined(__linux__)
    utsname system{};
    if (uname(&system) == 0) {
        std::cout << "operating_system=" << system.sysname << ' ' << system.release << '\n';
    }
    struct sysinfo memory{};
    if (sysinfo(&memory) == 0) {
        std::cout << "memory_bytes="
                  << static_cast<unsigned long long>(memory.totalram) * memory.mem_unit << '\n';
    }
    std::ifstream cpu_info("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpu_info, line)) {
        if (line.starts_with("model name")) {
            std::cout << "cpu=" << line.substr(line.find(':') + 2) << '\n';
            break;
        }
    }
#else
    std::cout << "operating_system=unreported\nmemory_bytes=unreported\ncpu=unreported\n";
#endif
}

std::string percentile(std::vector<std::uint64_t>& samples, double quantile) {
    if (samples.empty()) return "n/a";
    const auto index = static_cast<std::size_t>(quantile * (samples.size() - 1));
    std::nth_element(samples.begin(), samples.begin() + index, samples.end());
    return std::to_string(samples[index]);
}

bool reserve_output_bytes(std::uint64_t count) {
    auto used = output_bytes_reserved.load(std::memory_order_relaxed);
    while (used <= maximum_output_bytes - count) {
        if (output_bytes_reserved.compare_exchange_weak(
                used, used + count, std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

std::filesystem::path create_run_directory(const std::filesystem::path& parent) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) return {};
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        const auto candidate = parent /
            (".glogger-benchmark-" +
             std::to_string(Clock::now().time_since_epoch().count()) + "-" +
             std::to_string(attempt));
        if (std::filesystem::create_directory(candidate, ec)) return candidate;
        if (ec && ec != std::errc::file_exists) return {};
    }
    return {};
}

class RunDirectoryCleanup {
public:
    explicit RunDirectoryCleanup(std::filesystem::path path) : path_(std::move(path)) {}
    ~RunDirectoryCleanup() {
        if (!path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }

    bool remove() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        if (ec) {
            std::cerr << "Could not remove benchmark output directory '" << path_.string()
                      << "': " << ec.message() << '\n';
            return false;
        }
        path_.clear();
        return true;
    }

private:
    std::filesystem::path path_;
};

} // namespace

int run_encoding(const std::filesystem::path& root, glogger::TextEncoding encoding,
                 std::string_view encoding_name) {
    glogger::LoggerOptions options;
    options.directory = root;
    options.active_filename = "benchmark-" + std::string(encoding_name) + ".log";
    options.archive_policy = glogger::ArchivePolicy::Overwrite;
    options.console_output = false;
    options.text_encoding = encoding;
    glogger::Logger logger(options);
    if (!reserve_output_bytes(log_header_bytes)) {
        std::cerr << "The 5 GiB benchmark output budget is exhausted\n";
        return 1;
    }
    if (auto result = logger.initialize(); !result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }

    const std::string payload = benchmark_payload();
    const std::string encoded_payload = encoding == glogger::TextEncoding::Utf8
        ? payload : escaped_payload();
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<std::uint64_t> warmup_submitted{0};
    std::atomic<bool> failed{false};
    std::atomic<bool> budget_exhausted{false};
    std::vector<std::vector<std::uint64_t>> thread_latencies(producer_count);
    std::vector<std::uint32_t> producer_submitted(producer_count, 0);
    std::vector<std::vector<std::uint32_t>> record_sequences(producer_count);
    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    std::atomic<std::size_t> ready{0};
    std::atomic<bool> start{false};
    Clock::time_point measure_start;
    Clock::time_point measure_end;
    for (std::size_t producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            auto next = measure_start - warmup_duration;
            std::uint32_t sequence = 0;
            while (Clock::now() < measure_end &&
                   !budget_exhausted.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_until(next);
                const auto now = Clock::now();
                if (now >= measure_end) break;
                next += record_period;

                std::string record = payload;
                const auto id = (static_cast<std::uint64_t>(producer) << 32) | sequence++;
                write_record_id(record, id);
                const auto record_bytes = static_cast<std::uint64_t>(encoded_payload.size()) +
                                          record_overhead_bytes;
                if (!reserve_output_bytes(record_bytes)) {
                    budget_exhausted.store(true, std::memory_order_relaxed);
                    break;
                }
                const auto before = Clock::now();
                const auto result = logger.log(glogger::Level::Info, record);
                const auto after = Clock::now();
                if (!result) {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
                ++producer_submitted[producer];
                if (before >= measure_start) {
                    thread_latencies[producer].push_back(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
                    submitted.fetch_add(1, std::memory_order_relaxed);
                } else {
                    warmup_submitted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != producer_count) {
        std::this_thread::yield();
    }
    const auto launch = Clock::now();
    measure_start = launch + warmup_duration;
    measure_end = measure_start + measurement_duration;
    start.store(true, std::memory_order_release);
    for (auto& producer : producers) producer.join();
    if (failed.load(std::memory_order_relaxed)) {
        std::cerr << "A producer log call failed\n";
        return 1;
    }
    if (auto result = logger.flush(); !result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }

    std::vector<std::uint64_t> latencies;
    for (auto& values : thread_latencies) {
        latencies.insert(latencies.end(), values.begin(), values.end());
    }
    const auto elapsed = std::chrono::duration<double>(measure_end - measure_start).count();
    const auto count = submitted.load(std::memory_order_relaxed);
    const auto warmup_count = warmup_submitted.load(std::memory_order_relaxed);
    std::cout << "output_bytes_reserved_total="
              << output_bytes_reserved.load(std::memory_order_relaxed) << '\n';
    const auto message_bytes = static_cast<std::uint64_t>(encoded_payload.size()) * count;
    std::cout << "encoding=" << encoding_name << '\n'
              << "records=" << count << '\n'
              << "warmup_records=" << warmup_count << '\n'
              << "records_per_second=" << count / elapsed << '\n'
              << "message_bytes_per_second=" << message_bytes / elapsed << '\n'
              << "producer_latency_ns_p50=" << percentile(latencies, 0.50) << '\n'
              << "producer_latency_ns_p95=" << percentile(latencies, 0.95) << '\n'
              << "producer_latency_ns_p99=" << percentile(latencies, 0.99) << '\n';

    std::ifstream input(root / options.active_filename, std::ios::binary);
    if (!input) {
        std::cerr << "Could not read benchmark log for verification\n";
        return 1;
    }
    std::string line;
    std::uint64_t verified = 0;
    while (std::getline(input, line)) {
        if (line.starts_with("# glogger-start-v1:")) continue;
        const auto separator = line.find("]: ");
        if (separator == std::string::npos) {
            std::cerr << "Record integrity verification failed\n";
            return 1;
        }
        const auto message = line.substr(separator + 3);
        if (message.size() != encoded_payload.size() ||
            message.substr(16) != encoded_payload.substr(16)) {
            std::cerr << "Record integrity verification failed\n";
            return 1;
        }
        std::uint64_t id = 0;
        const auto [end, error] = std::from_chars(message.data(), message.data() + 16, id, 16);
        if (error != std::errc{} || end != message.data() + 16) {
            std::cerr << "Invalid benchmark record identifier\n";
            return 1;
        }
        const auto producer = static_cast<std::size_t>(id >> 32);
        if (producer >= producer_count) {
            std::cerr << "Invalid benchmark producer identifier\n";
            return 1;
        }
        record_sequences[producer].push_back(static_cast<std::uint32_t>(id));
        ++verified;
    }
    const auto expected_records = count + warmup_count;
    if (verified != expected_records) {
        std::cerr << "Record count mismatch: submitted " << expected_records
                  << ", verified " << verified << '\n';
        return 1;
    }
    for (std::size_t producer = 0; producer < producer_count; ++producer) {
        auto& sequences = record_sequences[producer];
        std::sort(sequences.begin(), sequences.end());
        if (sequences.size() != producer_submitted[producer]) {
            std::cerr << "Per-producer benchmark record count mismatch\n";
            return 1;
        }
        for (std::size_t sequence = 0; sequence < sequences.size(); ++sequence) {
            if (sequences[sequence] != sequence) {
                std::cerr << "Missing or duplicate benchmark record identifier\n";
                return 1;
            }
        }
    }
    return 0;
}

int main() {
    const char* configured_directory = std::getenv("GLOGGER_BENCH_DIRECTORY");
    const auto parent = configured_directory != nullptr
        ? std::filesystem::path(configured_directory)
        : std::filesystem::temp_directory_path();
    const auto root = create_run_directory(parent);
    if (root.empty()) {
        std::cerr << "Could not create a unique benchmark output directory\n";
        return 1;
    }
    RunDirectoryCleanup cleanup(root);
    std::cout << "benchmark_directory=" << root.string()
              << " (removed when the benchmark exits)\n"
              << "producer_count=" << producer_count
              << ", records_per_producer_per_second=100\n"
              << "maximum_output_bytes=" << maximum_output_bytes << '\n';
    report_system();
    int result = run_encoding(root, glogger::TextEncoding::Utf8, "utf8");
    if (result == 0) {
        result = run_encoding(root, glogger::TextEncoding::AsciiEscaped, "ascii-escaped");
    }
    if (!cleanup.remove()) return 1;
    return result;
}
