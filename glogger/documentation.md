# glogger v2

`glogger` is a synchronous, thread-safe C++23 logger with a C interoperability
API. It serializes each record and configuration operation on a logger instance.
Separate instances writing to the same file are not coordinated.

## C++ usage

```cpp
#include "glogger.hpp"

int main() {
    glogger::LoggerOptions options;
    options.directory = "logs";
    options.active_filename = "server.log";
    options.archive_policy = glogger::ArchivePolicy::IncrementingNumber;

    glogger::Logger logger(options);
    if (auto result = logger.initialize(); !result) {
        return 1;
    }

    if (auto result = logger.log(glogger::Level::Info, "Server started"); !result) {
        return 1;
    }
    return logger.logf(glogger::Level::Info, "Listening on port {}", 8080) ? 0 : 1;
}
```

`LoggerOptions` defaults to `logs/latest.log`, console output enabled, UTF-8
text, and `ArchivePolicy::Overwrite`. The active filename must be a plain
filename. A successful `initialize(options)` can reconfigure an existing logger;
configuration changes are serialized with writes. Check every returned
`glogger::Result`; errors contain an `ErrorCode` and diagnostic message.
`flush()` explicitly reports delayed stream errors. Destroying a logger flushes
and closes it, but callers that need to observe flush errors should call
`flush()` first. Ensure all producer threads have stopped before destruction.

Built-in numeric levels are stable: `0 ERROR`, `1 DEBUG`, `2 WARN`, `3 INFO`,
and `4 CHAT`. Custom names may be registered for identifiers greater than 4.
Unregistered identifiers are written as `LEVEL_<id>`.

## Records and encoding

The first line is a machine-readable start-time header:

```text
# glogger-start-v1:YYYY-MM-DDTHH:MM:SSZ
```

Record timestamps use UTC in the form `[YYYY-MM-DDTHH:MM:SSZ]`; each record is
newline-terminated. Embedded message newlines are retained. The message and
custom level name must be valid UTF-8. `TextEncoding::Utf8` preserves UTF-8.
`TextEncoding::AsciiEscaped` emits ASCII, escaping non-ASCII Unicode scalar
values as `\uXXXX` or `\UXXXXXXXX`; invalid UTF-8 and surrogate code points are
rejected. Custom level names are kept ASCII in records; non-ASCII scalar values
and control characters in those names are escaped independently of message
encoding.

## Archival

If the active file exists at initialization:

- `Overwrite` stages the new output before replacing the old active log.
- `HeaderTimestamp` archives using the timestamp in the first-line header.
  It reads this v2 header and the legacy `YYYYMMDD_HHMMSS` header.
- `FileMetadataTimestamp` uses the filesystem last-write time. This is the
  documented portable fallback because C++23 does not expose file birth time.
- `IncrementingNumber` chooses the first available suffix, such as
  `application.1.log`.

Archives go to `archive_directory`, or to the active directory when it is
empty. Timestamp collisions receive a numeric suffix. An archival failure
returns an error and does not proceed to truncate the old active file. When
moving between filesystems, the source is removed only after its archive copy
completes successfully.

## C API

Include `glogger_c.h` from C or C++. Call `glogger_create`, initialize with a
`glogger_config` whose `struct_size` is `sizeof(glogger_config)` and whose
version is `GLOGGER_CONFIG_VERSION_1`, then check every returned status. String
pointers in the configuration are copied during initialization. Null required
arguments return `GLOGGER_STATUS_INVALID_ARGUMENT`. All C entry points contain
C++ exceptions.

## Build, tests, and benchmark

The default build is shared. Configure a static library from the same project
with `-DGLOGGER_BUILD_SHARED=OFF`:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-static -DBUILD_TESTING=ON -DGLOGGER_BUILD_SHARED=OFF
cmake --build build-static
ctest --test-dir build-static --output-on-failure
```

The optional `glogger_benchmark` target runs 100 producer threads, each
scheduled for 100 records per second, with a five-second warm-up and a
60-second measurement. Each message contains 512 Unicode scalar values with
equal 1-, 2-, 3-, and 4-byte UTF-8 widths. UTF-8 and ASCII-escaped output are
measured separately. It reports throughput, payload byte rate, producer-call
latency percentiles, CPU, memory, OS, compiler, storage device, filesystem,
logger mode, and build configuration, then verifies record count and content
integrity. A shared, hard 5 GiB output budget covers both encodings. Each run
uses a unique temporary subdirectory that is removed when the benchmark exits.
Set `GLOGGER_BENCH_DIRECTORY`, `GLOGGER_BENCH_STORAGE_DEVICE`, and
`GLOGGER_BENCH_FILESYSTEM` to select and identify the local storage used. The
benchmark is not part of routine tests. This reduced producer count is useful
for bounded local measurements, not for validating the 10,000-producer
throughput goal; a reference system and an appropriately bounded run must be
fixed before treating that goal as a pass/fail threshold.

```sh
cmake -S . -B build-bench -DGLOGGER_BUILD_BENCHMARK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target glogger_benchmark
./build-bench/glogger_benchmark
```

The synchronous implementation is a correctness-first baseline. If it misses
the throughput goal on the agreed reference system, a bounded queued mode
must be added and measured separately; it must block rather than drop on queue
saturation and surface writer errors through `flush()`.
