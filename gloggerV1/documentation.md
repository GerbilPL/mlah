# glogger

`glogger` writes timestamped messages to the console and to `latest.log`. The logger creates its output directory when needed.

Its CMake target is shared (`glogger`) and exports both the C wrapper and C++ `Logger` methods.

## Quick example

```cpp
#include "glogger.hpp"

int main()
{
    gLogger::Logger log;
    if (!log.init("logs")) {
        return 1;
    }

    log.log(gLogger::LogLevel::INFO, "Application started");
    log(gLogger::LogLevel::DBG) << "Connected to port " << 8080;
}
```

The stream form emits its message when the temporary stream object is destroyed. Build the library and example from this directory with:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Things to know

- Call `init` before logging. Calls to `log` before successful initialization are ignored.
- `init` returns `false` when the log directory or file cannot be opened. Calling it again closes the prior file and switches to the new directory.
- Each initialization truncates the new `latest.log`. If one already exists there, the logger tries to rename it using the timestamp on its first line.
- Logging is synchronous. Do not call the same `Logger` instance concurrently from multiple threads.
- Levels are `ERR`, `DBG`, `WARN`, `INFO`, and `CHAT`.

The C interface is declared in [include/glogger_c.h](include/glogger_c.h); create a logger, check `glogger_init`, log with level integers `0` through `4` in the order above, then destroy it.

See [examples/basic.cpp](examples/basic.cpp) for the runnable example and [tests/test_glogger.cpp](tests/test_glogger.cpp) for the reinitialization check.
