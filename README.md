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

## Dependencies and licensing

`gargs` and `glogger` use the C++ standard library. `glinguist` also depends on [nlohmann/json](https://github.com/nlohmann/json), a third-party library maintained outside this repository. Its upstream license is MIT (also called Expat); the installed package metadata may include additional notices for bundled components.

The root [LICENSE](LICENSE) applies to this repository's own code. It does not claim ownership of third-party dependencies or replace their licenses. When redistributing a build, check the licenses and notices for the exact dependency versions you include and retain any required notices.

In the library table, **static** means the library is linked into the consuming program, while **shared** means it is provided as a separate library loaded at runtime. A **dependency** is external software required to build or use a subproject.

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
