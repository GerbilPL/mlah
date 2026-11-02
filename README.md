# mlah

Small C and C++ libraries, kept as independent subprojects.

## Libraries

| Library | Purpose | Layout | Build type |
| --- | --- | --- | --- |
| [gargs](gargs/documentation.md) | Callback-based command-line parsing | `include/`, `src/`, `examples/`, `tests/` | Static only (`gargs`) |
| [glogger](glogger/documentation.md) | Timestamped C++ and C logging | `include/`, `src/`, `examples/`, `tests/` | Shared only (`glogger`) |
| [glinguist](glinguist/documentation.md) | JSON-backed locale lookup | `include/`, `src/` | Static only (`glinguist`) |

Each library is configured and built from its own directory. The repository does not currently provide a combined root build.

## Requirements

- CMake 3.20 or newer
- A C++23 compiler
- `nlohmann_json` for `glinguist`

## Build and test

For `gargs` or `glogger`, configure, build, and run tests from the repository root:

```sh
cmake -S gargs -B build/gargs -DBUILD_TESTING=ON
cmake --build build/gargs
ctest --test-dir build/gargs --output-on-failure

cmake -S glogger -B build/glogger -DBUILD_TESTING=ON
cmake --build build/glogger
ctest --test-dir build/glogger --output-on-failure
```

`glinguist` currently has no test target. Configure it separately after installing the `nlohmann_json` CMake package:

```sh
cmake -S glinguist -B build/glinguist
cmake --build build/glinguist
```

## Structure

Subprojects keep public headers in `include/`, implementation files in `src/`, and runnable examples and tests in their own directories when present. Build type is intentionally explicit per library; see the table above before linking a project as a shared library.
