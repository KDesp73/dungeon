# dungeon

A real-time, top-down 2D action RPG under development in C++20 for the
terminal, built without a game engine.

The long-term direction is ANSI-coloured character-cell rendering with a
simulation kept independent from the presentation layer, so every gameplay
rule can be unit tested. The current build provides the build and test
scaffolding, terminal-size detection, procedural ASCII map generation, a
three-panel game UI, a fixed-timestep loop, a scene stack with title and pause
screens, WASD player movement, action hooks for items, enemy AI with combat,
consumable health potions, character progression, and periodic store levels
with a walk-up shopkeeper.

<img width="1272" height="1266" alt="Στιγμιότυπο οθόνης 2026-11-11, 11 22 26 πμ" src="https://github.com/user-attachments/assets/7b65b6e0-03ee-4aca-a7f1-3ac7ead340a9" />


## Status

| Area         | State                                                                 |
| ------------ | --------------------------------------------------------------------- |
| Build system | CMake presets, strict warnings, ASan/UBSan, CTest, GitHub Actions      |
| Terminal     | `dungeon::display::tui::term_size()` reads the terminal dimensions     |
| Map          | Optional 3x3 room grid, orthogonal corridors, entrance/exit, item spawns |
| Rendering    | Double-buffered framebuffer with game, stats, inventory, message, and chest panels |
| Scenes       | Stack manager with title, game, store, and pause-menu scenes          |
| Tests        | Lightweight harness with `core`, `display`, `game`, `map`, and `scene` suites |
| Gameplay     | WASD/arrow movement, item action hooks, enemy AI, combat, coin drops, health potions, leveling, and store upgrades |

The executable accepts `--help`, `--version`, `--seed <n>`, and `--print-map`.
`--seed` makes dungeon generation reproducible, and `--print-map` writes the
generated dungeon to stdout, reports its seed on stderr, and exits. Without those
options it opens the title screen; press `Enter` to start a run. The fixed-timestep
loop draws the generated map in the left game viewport, a stats column on the
right, and an inventory, message log, and chest panel across the bottom.
Simulation time advances at 60 ticks per second; rendering is paced to the next
tick. `W`, `A`, `S`, and `D` or the arrow keys move the player one cell in the
matching direction. While next to a chest, `[` and `]` cycle through its items,
`1`-`9` select one directly, and `Enter` takes the selected item. Move into an
enemy to attack it. While carrying a health potion, press `H` to drink one and
recover a random amount of health. Press `Esc` to pause; the pause menu freezes
the simulation, lists the controls, and offers Resume, Save game, and Quit.
Press `Esc` again to resume or `Ctrl+C` to quit. When the player dies, press `R`
to start a fresh run. Every fifth dungeon is a store level: walk close to the
`&` shopkeeper to open the shop panel, use the arrow keys or `W`/`S` to choose
an upgrade, `Enter` to buy it, and `Esc` to leave. The store exit is already
open; step into the `[` to continue to the next dungeon.

## Command line

```sh
./build/dev/dungeon --help
./build/dev/dungeon --version
./build/dev/dungeon --seed 12345
./build/dev/dungeon --print-map --seed 12345
```

Passing the same seed to `--print-map` twice produces the same dungeon.
Without `--seed`, a fresh random seed is chosen and reported on stderr.

## Map output

Generated maps use these glyphs:

| Glyph    | Tile                              |
| -------- | --------------------------------- |
| `-`, `\|` | room walls                        |
| `.`      | room floor                        |
| `#`      | walkway                           |
| `+`      | door                              |
| `@`      | player (placed at the entrance)   |
| `[`      | unlocked exit                     |
| `r`      | rat (fast enemy)                  |
| `g`      | goblin (normal enemy)             |
| `O`      | ogre (slow, tough enemy)          |
| `$`      | coin                              |
| `V`      | diamond                           |
| `k`      | key                               |
| `!`      | health potion                     |
| `?`      | chest                             |
| `&`      | shopkeeper (store level)          |
| `=`      | store counter                     |
| `/`      | sword (store upgrade)             |
| `T`      | greatsword (store upgrade)        |
| `d`      | shield (store upgrade)            |
| `m`      | chainmail (store upgrade)         |
| `b`      | boots (store upgrade)             |
| space    | empty space outside rooms         |

Every map has a seed exposed through `Map::seed()`; the executable prints it and
accepts `--seed` to replay a run. `Map::make_spec()`
returns a `MapSpec` containing the rooms, connections, entrance, exit, and
random item spawns. Rooms are placed on an optional 3x3 grid with slight random
variation in size and position, and corridors only turn horizontally or
vertically, so generated walkways never run diagonally. Each dungeon contains
exactly one key, biased away from the player's starting room and its neighbours,
and diamonds spawn rarely. Rooms can also contain health potions. Each room can
also spawn rats, goblins, and ogres; the player's starting room is less likely to
contain one. A chest may contain the dungeon's key instead of it being placed
directly in a room; in that case no second key is generated. `Map::generate(const
MapSpec&)` rebuilds that exact layout and places the player, items, and enemies
without regenerating anything randomly. `Map::make_store_spec()` builds a single
room with a shopkeeper, a counter, no enemies, and an exit that is open without a
key.

## Movement and actions

`dungeon::game::Game` owns the map, exposes the player through `Game::player()`
and `Map::player()`, and moves the player with `Game::move(Direction)`.
Movement is blocked by walls, empty space, and the map edge; floors, walkways,
and doors are walkable.

After every completed move the game scans for items and calls the installed
`ActionHook`:

- `ActionTrigger::Step` fires for each item on the player's new cell.
- `ActionTrigger::Nearby` fires for each interactable item that is orthogonally
  one square away. A nearby interactable fires on every move made while it
  stays in range, so state-changing hooks should be idempotent.

Stepping on a coin adds its value to the player's gold and removes it from the
map. Stepping on the key equips it into the player inventory and removes it from
the map; while the key is equipped, the exit is shown as `[` and can be used.
Stepping on a health potion adds it to the inventory without healing immediately.
Pressing `H` drinks one, restores a random amount of health, and never raises
health above its maximum; the potion is only consumed when it actually heals.
The stats panel reads the player's live stats, and the inventory panel shows the
glyph of each equipped item.

Chests are generated with their own loot. Standing on or next to a chest shows
its contents in the bottom-right panel, where `[` and `]` cycle the selection,
`1`-`9` select an item directly, and `Enter` takes it. Chests can hold coins,
diamonds, health potions, and the dungeon key. Coins and diamonds add gold, while
other items are equipped into the inventory. The middle panel shows a short log
of gameplay messages such as pickups and level transitions.

Stepping onto the unlocked exit generates a new dungeon at the same size and
places the player at its entrance. The key is consumed by the exit, while the
player's stats and remaining inventory are carried over to the new map.

An `ActionEvent` carries the trigger, the item, and the player. Hooks are
installed with `Game::set_action_hook()`; an empty hook disables action
dispatch. Built-in pickup effects still apply when no hook is installed.

## Enemies and combat

Enemies move on their own simulation clocks. They wander until the player enters
their aggro radius and then chase him through the dungeon, using a breadth-first
search so walls and other enemies do not trap them. An enemy beside the player
attacks whenever its attack cooldown is ready; damage is reduced by the player's
defence and is never lower than one.

The three archetypes trade speed against danger. Rats are fast but fragile;
goblins sit in the middle; ogres are slow, tough, and hit hard. Moving into an
enemy attacks it with the player's attack reduced by its defence. Killed enemies
drop a randomized number of coins on their cell, with tougher enemies dropping
the largest bounties. The right-hand stats panel shows the living enemy nearest
to the player, including its health, attack, defence, speed, and bounty range.

## Stores and progression

Every fifth dungeon is a store level: a small room with a `&` shopkeeper behind
a `=` counter and an exit that is already open. Walking within two cells of the
shopkeeper opens the shop panel over the map. Move away and come back to reopen
it after leaving. The panel lists equipment upgrades together with their price:
swords and greatswords raise attack, shields and chainmail raise defence, and
boots raise stamina. Items are bought with gold collected in the dungeon, and
each offer can be bought once per store visit. The first purchase adds the item
to the player's inventory; buying the same item again in a later store upgrades
the item already carried, increasing its bonus instead of adding a duplicate.

The store exit does not need a key. Step onto the `[` at the top of the room to
descend to the next dungeon, carrying the player's stats and inventory with
them.

The player's character level also rises every fifth dungeon. A level-up grants
bonus attack, defence, and stamina and restores the player to full health. The
stats panel shows both the character level and the current dungeon number, so
the next store and level-up are always easy to anticipate.

## Requirements

- CMake 3.21 or newer
- A C++20 compiler (GCC 11+, Clang 14+, Apple Clang 14+)
- Ninja (optional, only for the presets that use it)
- A POSIX system: Linux, macOS or a similar Unix

No third party libraries are required at runtime. CLI11 is used for command
line parsing and is fetched automatically by CMake when a system package is not
available, so the first configure needs network access.

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
./build/dev/tests/dungeon_tests --suite game      # run movement and action tests
./build/dev/tests/dungeon_tests --suite map       # run the map tests
./build/dev/tests/dungeon_tests --suite scene     # run the scene-stack tests
./build/dev/tests/dungeon_tests --list             # list every test case
```

Each subdirectory of `tests/` that contains test cases becomes one CTest
entry; the suite name is the directory name. The current suites are `core`,
`display`, `game`, `map`, and `scene`.

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
│   ├── map/                  map generation and tile API
│   ├── scene/                scene stack, title screen, game, store, and pause scenes
│   └── game.hpp              movement, direction, and action hooks
├── src/
│   ├── core/                 ticker and version implementations
│   ├── display/              framebuffer, terminal, and UI implementations
│   ├── map/                  map generation, tiles, and text output
│   ├── scene/                scene manager, title, game, store, and pause scenes
│   ├── game.cpp              movement and action dispatch
│   └── main.cpp              process entry point
└── tests/
    ├── core/                 core suite
    ├── display/              display suite
    ├── game/                 movement and action suite
    ├── map/                  map suite
    ├── scene/                scene-stack and pause-menu suite
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
