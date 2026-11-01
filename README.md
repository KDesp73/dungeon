# dungeon

A real-time, top-down 2D action RPG under development in C++20 for the
terminal, built without a game engine.

The long-term direction is ANSI-coloured character-cell rendering with a
simulation kept independent from the presentation layer, so every gameplay
rule can be unit tested. The current build provides the build and test
scaffolding, terminal-size detection, procedural ASCII map generation, a
three-panel game UI, and a fixed-timestep loop. Input, combat, and enemy AI are
still to come.

## Status

| Area         | State                                                                 |
| ------------ | --------------------------------------------------------------------- |
| Build system | CMake presets, strict warnings, ASan/UBSan, CTest, GitHub Actions      |
| Terminal     | `dungeon::display::tui::term_size()` reads the terminal dimensions     |
| Map          | Rooms, doors, walkways, entrance/exit, and random item spawns; replay |
| Rendering    | Double-buffered framebuffer with game, stats, and inventory panels    |
| Tests        | Lightweight harness with `core`, `display`, and `map` suites           |
| Gameplay     | Player spawn and room items; no input, combat, or enemy AI yet         |

The executable currently prints the project version and run seed, then enters a
fixed-timestep loop that draws the generated map in the left game viewport, a
stats column on the right, and an inventory bar across the bottom. Simulation
time advances at 60 ticks per second; rendering is paced to the next tick.

## Map output

Generated maps use these glyphs:

| Glyph    | Tile                              |
| -------- | --------------------------------- |
| `-`, `\|` | room walls                        |
| `.`      | room floor                        |
| `#`      | walkway                           |
| `+`      | door                              |
| `@`      | player (placed at the entrance)   |
| `$`      | coin                              |
| `k`      | key                               |
| `?`      | chest                             |
| space    | empty space outside rooms         |

Every map has a seed exposed through `Map::seed()`; the executable prints it so
a run can later be replayed once seed input is supported. `Map::make_spec()`
returns a `MapSpec` containing the rooms, connections, entrance, exit, and
random item spawns. Each dungeon contains exactly one key, biased away from the
player's starting room and its neighbours. `Map::generate(const MapSpec&)`
rebuilds that exact layout and places the player and items without regenerating
anything randomly.

## Requirements

- CMake 3.21 or newer
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

The binary is written to `build/dev/dungeon`. Run it directly and press
`Ctrl+C` to quit and restore the terminal cursor.

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
./build/dev/tests/dungeon_tests --suite core      # run a single suite
./build/dev/tests/dungeon_tests --suite display   # run the framebuffer tests
./build/dev/tests/dungeon_tests --suite map       # run the map tests
./build/dev/tests/dungeon_tests --list             # list every test case
```

Each subdirectory of `tests/` that contains test cases becomes one CTest
entry; the suite name is the directory name. The current suites are `core`,
`display`, and `map`.

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
│   ├── core/                 fixed-timestep ticker and project version
│   ├── display/              framebuffer, terminal size, and UI helpers
│   └── map/                  map generation and tile API
├── src/
│   ├── core/                 ticker and version implementations
│   ├── display/              framebuffer, terminal, and UI implementations
│   ├── map/                  map generation, tiles, and text output
│   └── main.cpp              process entry point
└── tests/
    ├── core/                 core suite
    ├── display/              display suite
    ├── map/                  map suite
    └── test_harness.*        lightweight test framework
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
