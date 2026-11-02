# gargs

`gArgs` is a small callback-based command-line parser. Register options, call `parse`, and read any remaining positional arguments.

The CMake target is static (`gargs`); shared-library support is not configured.

## Quick example

```cpp
#include "gargs.hpp"
#include <iostream>

int main(int argc, const char* argv[])
{
    gArgs args;
    std::string output;

    args.pushOption('o', "--output", [&output](const std::vector<std::string>& values) {
        output = values.front();
    }, true, 1, 1);
    args.pushOption('v', "--verbose", [](const std::vector<std::string>&) {
        std::cout << "verbose mode\n";
    });

    try {
        args.parse(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << "output: " << output << '\n';
    for (const auto& path : args.getPositionalArgs()) {
        std::cout << "input: " << path << '\n';
    }
}
```

Build the library and this example from this directory with:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Option behavior

- Long names must start with `--`; the short name is a single character.
- Value-taking options specify minimum and maximum values. A maximum of `0` means unlimited, so the option consumes following non-option arguments until another option or the end of input.
- Values can be attached to long or short options (`--output=result.txt`, `-oresult.txt`). Short flag groups such as `-abc` invoke each registered flag in order. A value-taking short option ends the group and consumes the remaining suffix as its attached value (`-abn-5`).
- An explicit empty value (`--output=`) counts as one value. A flag registered without values rejects `--verbose=yes`.
- A no-value flag does not consume following filenames: `app --debug first.txt second.txt` calls the `--debug` callback and leaves both filenames in `getPositionalArgs()`.
- A negative decimal argument (including fractions and exponents) is accepted as an option value (`--number -5`) and as a positional argument when it appears by itself. The parser recognizes numeric syntax; domain-specific range or type checks belong in the callback.
- `--` ends option parsing, so `app -- -filename` passes `-filename` as a positional argument.
- Unknown options throw by default. To preserve unknown option tokens as positionals, select the policy when constructing the parser:

    ```cpp
    gArgs args(gArgs::UnknownOptionPolicy::Positional);
    ```

    With this policy, an unknown short group is passed as one token and none of its callbacks run.
- A parser instance accumulates positional arguments and callbacks run during parsing; use a fresh instance for each independent argument list.

See [examples/basic.cpp](examples/basic.cpp) for the runnable example and [tests/test_gargs.cpp](tests/test_gargs.cpp) for focused parser behavior checks.
