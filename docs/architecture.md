# Architecture

This document describes how the code is meant to be organised and where new
code goes. It is a guide, not a description of what exists today: the
repository currently holds only the build scaffolding and a placeholder
`main()`.

## Layers

```text
                ┌──────────────┐
                │     core     │  application state, game loop, timing
                └──────┬───────┘
                       │ owns
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
   ┌─────────┐   ┌──────────┐   ┌──────────┐  gameplay rules
   │  world  │   │ entities │   │ systems  │
   └─────────┘   └──────────┘   └──────────┘
        │              │              │
        └──────────────┼──────────────┘
                       ▼
                ┌──────────────┐
                │    render    │  reads state, never writes it
                └──────────────┘
                       ▲
                       │ reads
                ┌──────────────┐
                │   platform   │  the only OS specific code
                └──────────────┘
```

The arrows are one way. The renderer may read the world and the entities, but
no gameplay system may depend on the renderer, and nothing below `platform`
may include a system header.

### core

Owns the application: the state machine (title, playing, paused, dead), the
game loop and the timing policy. It is the only place that decides what
happens in a frame.

The loop separates two clocks:

- the **simulation clock** advances in fixed steps of 1/60 s;
- the **frame clock** redraws as often as the terminal allows.

Simulation time is the only time gameplay code may rely on. Reading the wall
clock inside a system makes the system untestable.

### platform

Everything that touches the operating system: `termios`, `poll`, `ioctl`,
signal handling. It is the only directory allowed to include POSIX headers
directly, and it exposes plain C++ types to the rest of the code.

Two rules:

- terminal state is owned by an RAII object, so it is restored on every exit
  path, including signals;
- the platform layer never makes gameplay decisions.

### world

Maps, tiles, collision and spatial queries. Terrain is tile based, actors
have continuous positions. The world answers questions such as "is this
position solid?"; it does not move actors.

### entity

The player, enemies, projectiles and world objects. Entities are plain values
held in containers owned by the world or the application, not by each other.

Actors are never referenced by raw pointer across an operation that can
reallocate a container. Use an identifier (an index or a small wrapper around
one) when a reference has to survive.

### systems

Combat, AI, inventory, progression, interaction and persistence. A system
consumes state and produces state; it does not own the objects it operates
on and it does not draw anything.

### render

Turns game state into a framebuffer and a framebuffer into terminal output.
The renderer never mutates the world, never resolves damage and never decides
what happens next.

## Build structure

| Target          | Kind    | Contents                                    |
| --------------- | ------- | ------------------------------------------- |
| `dungeon_core`  | static  | every subsystem, no `main()`                 |
| `dungeon`       | exe     | the entry point, links `dungeon_core`        |
| `dungeon_tests` | exe     | the test harness and all test suites         |

Splitting the library from the executable is what makes the "no terminal
required" rule enforceable: the tests link the same code the game does and
cannot reach a terminal even by accident.

## Naming

| Element                | Style                  | Example                      |
| ---------------------- | ---------------------- | ---------------------------- |
| Namespace              | `dungeon::<subsystem>` | `dungeon::render`            |
| Type / struct / enum   | `PascalCase`           | `Framebuffer`, `KeyEvent`    |
| Function / variable    | `snake_case`           | `advance`, `player_position` |
| Constant               | `kCamelCase`           | `kStepDuration`              |
| Enumerator             | `PascalCase`           | `Key::PageUp`                |
| Private member         | trailing underscore    | `step_duration_`             |

Enums are scoped (`enum class`). Prefer `std::optional` over sentinel values
and `std::variant` over parallel state.

## Adding a feature

1. Decide which layer owns the rule. If the answer is "the renderer", the
   design is wrong.
2. Put the public interface in `include/dungeon/<layer>/` and the
   implementation in `src/<layer>/`. The two trees mirror each other.
3. Add the new source file to `add_library(dungeon_core ...)` in
   `CMakeLists.txt`.
4. Add tests under `tests/<layer>/` and register the suite in
   `tests/CMakeLists.txt`.
5. Keep the new code free of warnings; the warning set in
   `cmake/CompilerWarnings.cmake` is deliberately strict.

## Testing conventions

- A test file covers one behaviour per test case and names it as a sentence:
  `time_is_accumulated_until_a_full_step_is_available`.
- `CHECK` continues the test case, `REQUIRE` stops it. Use `REQUIRE` when
  continuing would crash.
- Tests never touch the terminal, the file system outside the project
  directory, or the network.
- Anything with a time or random component is tested through injected
  dependencies, never through real waiting.
