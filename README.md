# dungeon

A real-time, top-down 2D action RPG that runs entirely in a terminal, written
in C++20 without a game engine.

The game renders with ANSI colours and character cells, reads input directly
from the tty, and keeps the simulation independent from the presentation layer
so that every gameplay rule can be unit tested.

## Status

The project is at the start of stage 1 of the development plan
(see [AGENT.md](AGENT.md)). The repository currently holds the build and test
scaffolding only:

| Area         | State                                                                |
| ------------ | -------------------------------------------------------------------- |
| Build system | CMake + presets, strict warnings, sanitizer options, CTest, CI         |
| Tests        | Lightweight test harness, one suite (`core`)                         |
| Code         | `dungeon::core::kVersion` and a placeholder `main()`                  |

Everything else - terminal handling, the game loop, world, entities, systems
and rendering - is still to be written.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler (GCC 11+, Clang 14+, Apple Clang 14+)
- Ninja (optional, only for the presets that use it)
- A POSIX system: Linux, macOS or a similar Unix

No third party libraries are required.

## Building

The recommended way is a CMake preset:

```sh
cmake --preset dev
cmake --build --preset dev
```

The binary is written to `build/dev/dungeon`.

Available presets:

| Preset    | Build type     | Purpose                 |
| --------- | -------------- | ----------------------- |
| `dev`     | RelWithDebInfo | Day to day development  |
| `debug`   | Debug          | Debugger friendly       |
| `asan`    | RelWithDebInfo | Address + UB sanitizers |
| `release` | Release        | Optimised build, no tests |

Without presets:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

### Build options

| Option                       | Default | Meaning                                            |
| ---------------------------- | ------- | -------------------------------------------------- |
| `DUNGEON_BUILD_TESTS`        | `ON`    | Build the test binary                              |
| `DUNGEON_WARNINGS_AS_ERRORS` | `OFF`   | Turn warnings into errors                          |
| `DUNGEON_SANITIZE`           | `none`  | `address`, `undefined`, `address,undefined`, `thread` |

## Testing

```sh
ctest --preset dev --output-on-failure
./build/dev/tests/dungeon_tests --suite core   # run a single suite
./build/dev/tests/dungeon_tests --list          # list every test case
```

Each subdirectory of `tests/` that contains test cases becomes one CTest
entry; the suite name is the directory name.

## Sanitizers

```sh
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

Undefined behaviour aborts on the first occurrence instead of continuing with
a corrupted state.

## Repository layout

```text
dungeon/
├── AGENT.md                  project context and development plan
├── CMakeLists.txt            top level build definition
├── CMakePresets.json         convenience build configurations
├── cmake/                    build system modules (warnings, sanitizers)
├── docs/                     architecture and design notes
├── include/dungeon/          public headers, mirrors src/
│   └── core/
├── src/                      implementation
│   ├── core/
│   └── main.cpp              process entry point
└── tests/                    one directory per tested subsystem
```

New subsystems get a directory under `include/dungeon/` and `src/`; see
[docs/architecture.md](docs/architecture.md).

## Conventions

- Namespaces are `dungeon::<subsystem>`, for example `dungeon::render`.
- Types are `PascalCase`, functions and variables are `snake_case`, constants
  are `kCamelCase`.
- Headers use the `.hpp` extension and carry `#pragma once`.
- `.clang-format` describes the formatting; run `clang-format -i <file>`
  before committing.
- Public headers do not include what they do not use, and every header
  compiles on its own.

## Development plan

The staged plan lives in [AGENT.md](AGENT.md); the layering rules and the
rationale behind the structure live in
[docs/architecture.md](docs/architecture.md).
