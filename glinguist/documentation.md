# glinguist

`glinguist` loads JSON locale files and resolves dotted translation keys, with an optional fallback locale.

## Build

The library requires C++23 and the `nlohmann_json` CMake package. Its CMake target is static; dynamic-library support is not configured.

```sh
cmake -S . -B build
cmake --build build
```

Include the public API with `#include "glinguist.hpp"`; the CMake target adds this subproject's `include/` directory to consumers.
