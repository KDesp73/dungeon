# Project Context: dungeon

## 1. Project Overview

**dungeon** is a real-time, top-down 2D action RPG played entirely in a terminal. It is being developed from scratch in **C++20** for Linux and Unix-like systems, without a game engine.

The game uses an ASCII/Unicode character-cell display with ANSI terminal colors to represent the world, characters, combat effects, items, and user interface. Gameplay should feel responsive and action-oriented despite the terminal-based presentation.

The project is both a playable game and an opportunity to develop practical C++20 skills through a substantial, cohesive software project.

### Core goals

- Build a complete, playable action RPG with exploration, combat, enemies, loot, equipment, progression, and a boss encounter.
- Use modern C++20 features and idioms appropriately.
- Develop the game engine and supporting systems from scratch.
- Keep the code modular, testable, maintainable, and easy to extend.
- Support Linux and Unix-like systems using standard C++ and a small, well-defined platform-specific layer.
- Prioritize gameplay quality and correctness over premature abstraction or feature count.

## 2. Developer Background

The developer has a strong background in C and systems programming and has already built projects including:

- A Game Boy emulator in C, including debugger/disassembler functionality.
- A 3D raycasting engine and game in C.
- A chess engine in C.

Do not treat this as a beginner programming project. The goal is to learn and apply modern C++ design, ownership, value semantics, and standard-library facilities while building something more ambitious than an introductory game.

Avoid introductory explanations of basic programming concepts unless requested. Focus explanations on C++-specific design decisions, trade-offs, and implementation details.

Do not introduce raycasting or a first-person rendering engine. The game is top-down and terminal-based.

## 3. Technology and Constraints

- **Language:** C++20.
- **Build system:** CMake.
- **Platform:** Linux and Unix-like systems.
- **Rendering:** Terminal character cells, ASCII/Unicode glyphs, and ANSI colors.
- **Input and terminal control:** POSIX APIs such as `termios` and `poll` where appropriate.
- **Testing:** CTest with a suitable lightweight testing approach.
- **Debugging:** GDB or LLDB.
- **Runtime diagnostics:** AddressSanitizer and UndefinedBehaviorSanitizer during development.
- **Version control:** Git.

Prefer standard C++20 facilities wherever possible. Isolate platform-specific code behind a small interface.

Do not introduce a game engine or heavyweight dependency. A terminal library such as ncurses may be considered if terminal compatibility, input handling, or resizing becomes unnecessarily difficult to implement and maintain manually. Keep that decision localized to the platform/presentation layer.

## 4. Gameplay and Visual Direction

The game is a dark-fantasy, top-down action RPG set in dungeon-like environments.

### Gameplay features

The intended feature set includes:

- Real-time player movement and collision.
- Exploration of connected rooms and corridors.
- Melee combat with attack timing and directional attacks.
- Dodging, stamina management, and temporary invulnerability frames.
- Enemies with distinct behaviors and readable attack telegraphs.
- Ranged enemies and projectiles.
- Enemy health, damage, knockback, and death.
- Experience, leveling, and character progression.
- Inventory, equipment, consumables, loot, and chests.
- Doors, room transitions, and environmental interactions.
- A boss encounter, victory condition, death screen, and restart flow.
- Save and load functionality.

The initial playable version should be deliberately limited: a small handcrafted dungeon, a few enemy archetypes, one primary melee weapon, one ranged ability or weapon, basic progression, and one boss.

Avoid procedural generation, large amounts of content, elaborate crafting systems, or complex quest systems until the core movement and combat are enjoyable.

### Terminal presentation

Use a consistent visual language. For example:

- `@` — player.
- `s` — skeleton or basic enemy.
- `#` — wall.
- `.` — floor.
- `+` — doorway.
- `C` — chest.
- Distinct colors for player, enemies, loot, hazards, and interactive objects.

The presentation should include a readable HUD for health, stamina, experience, equipment, and relevant combat information.

Rendering must remain separate from simulation. The renderer displays game state; it must not make gameplay decisions or mutate the world.

## 5. Architecture

Organize the code by responsibility. Avoid both a monolithic game class and unnecessary architectural complexity.

The principal subsystems are:

### Core

Responsible for the game loop, application lifecycle, high-level state transitions, and coordination between subsystems.

Use a fixed simulation timestep, initially targeting 60 updates per second. Rendering may run independently of simulation updates. Clamp excessive accumulated time and avoid unbounded catch-up loops.

### Platform

Responsible for terminal initialization and restoration, input polling, timing support where necessary, terminal dimensions, and other operating-system-specific functionality.

Terminal settings must always be restored on normal exit and handled as safely as practical when interrupted or when initialization fails.

### World

Responsible for maps, world coordinates, collision, room transitions, and spatial queries.

Maps may use tiles for terrain and navigation, but the player and enemies should have continuous world positions rather than being restricted to moving one tile at a time.

### Entities

Responsible for the player, enemies, projectiles, and relevant world objects.

Prefer composition and simple value types. Introduce polymorphism only when it provides a concrete benefit.

### Gameplay systems

Keep combat, enemy AI, inventory, progression, interactions, and persistence in focused components rather than accumulating these responsibilities in entity classes.

### Rendering

Responsible for the framebuffer, glyph and color selection, visual effects, HUD, menus, and terminal output.

Build a frame representation from game state and output it efficiently. Avoid clearing and redrawing the terminal unnecessarily if a practical incremental rendering strategy can reduce flicker.

### Testing

Test important logic independently of terminal output. Prioritize collision, movement, combat timing, damage resolution, inventory rules, progression, and save/load behavior.

## 6. Design Principles

Follow these principles when implementing or modifying the project:

1. **Prefer simple, explicit designs.** Do not create abstractions until there is a concrete need for them.
2. **Use RAII.** Terminal state, file handles, and other resources must have clear lifetimes and reliable cleanup.
3. **Prefer value semantics.** Use ordinary values and standard containers when they adequately express ownership.
4. **Make ownership explicit.** Use references for non-owning access where lifetime guarantees are clear, and smart pointers for genuine dynamic ownership.
5. **Avoid unsafe entity references.** Do not keep raw pointers or references to elements in a `std::vector` across operations that may reallocate or erase them. Use stable identifiers or another deliberate lifetime strategy where necessary.
6. **Separate simulation from presentation.** Game logic should be testable without running a terminal renderer.
7. **Separate input collection from gameplay decisions.** Convert raw input into meaningful actions before applying those actions to game state.
8. **Keep timing deterministic where practical.** Gameplay systems should use simulation time or fixed ticks rather than relying on rendering frequency.
9. **Use data-driven content where useful.** External map or item definitions should simplify content iteration, not introduce a complicated configuration framework.
10. **Handle failure explicitly.** Validate external data and report actionable errors for malformed maps, invalid saves, and terminal initialization failures.
11. **Avoid premature optimization.** Start with straightforward data structures and optimize only when measurements justify it.
12. **Do not overengineer.** A full ECS, dependency injection framework, custom allocator system, or elaborate event bus is not required unless a demonstrated need emerges.

## 7. C++20 Conventions

- Use `enum class` for scoped enumerations.
- Prefer `std::vector`, `std::string`, `std::array`, `std::optional`, and `std::variant` when appropriate.
- Use `std::unique_ptr` for exclusive dynamic ownership and use `std::shared_ptr` only when shared ownership is genuinely necessary.
- Avoid owning raw pointers and manual resource management.
- Use `const` and references appropriately to communicate intent.
- Keep interfaces small and dependencies explicit.
- Prefer meaningful types over loosely related primitive values when the distinction matters.
- Use a consistent naming and formatting convention throughout the codebase.
- Avoid unnecessary templates, metaprogramming, macros, and complicated generic frameworks.
- Do not add third-party dependencies without a clear justification.

Keep compiler warnings enabled and resolve warnings rather than suppressing them without explanation.

## 8. Movement, Collision, and Combat

### Movement and collision

Use continuous positions with tile-based terrain. Begin with straightforward collision detection and axis-separated resolution, allowing the player to slide along walls rather than stopping completely when moving diagonally into them.

Keep movement speed and simulation timing consistent across machines.

### Combat

Model attacks as explicit phases, such as:

1. Wind-up.
2. Active frames.
3. Recovery.

Attacks should have clearly defined range, direction or arc, damage, timing, and resource costs.

Resolve hits through gameplay logic rather than rendering code. Ensure a single attack does not accidentally damage the same target repeatedly unless that behavior is intentional.

Use readable visual feedback for attacks, damage, knockback, and temporary invulnerability.

### Enemy AI

Start with a small state machine. A basic enemy may transition between idle, patrol, chase, attack, and reposition states.

Add pathfinding and more advanced behaviors only when the basic enemy works reliably. Use A* when navigation around obstacles requires it; do not make every enemy system depend on pathfinding from the outset.

## 9. Suggested Repository Layout

Adapt the layout as the project evolves, but keep responsibilities clear.

```text
ashen-depths/
├── AGENTS.md
├── CMakeLists.txt
├── assets/
│   ├── maps/
│   ├── items/
│   └── config/
├── include/
│   └── ashen/
│       ├── core/
│       ├── platform/
│       ├── world/
│       ├── entity/
│       ├── systems/
│       └── render/
├── src/
│   ├── core/
│   ├── platform/
│   ├── world/
│   ├── entity/
│   ├── systems/
│   ├── render/
│   └── main.cpp
└── tests/
```

Do not create empty directories or placeholder classes solely to match this structure. Introduce files as the corresponding functionality is implemented.

## 10. Development Order

Develop the game incrementally, keeping it buildable and playable at each major stage.

### Stage 1: Foundation

- Set up CMake and the C++20 target.
- Establish compiler warnings and sanitizer options.
- Implement terminal initialization and reliable restoration.
- Implement input polling.
- Create a basic framebuffer and terminal renderer.
- Add the game loop and initial tests.

### Stage 2: World and movement

- Load a small handcrafted map.
- Implement world coordinates and movement.
- Implement collision and wall sliding.
- Add room boundaries, doors, and interactions.
- Display the player and a basic HUD.

### Stage 3: Combat prototype

- Add health and stamina.
- Implement melee attack phases and directional hit detection.
- Add enemy damage, knockback, death, and respawning or restarting.
- Implement one enemy that can detect and chase the player.
- Add attack telegraphs and basic combat feedback.

At the end of this stage, the game should already be a playable combat prototype.

### Stage 4: AI and combat depth

- Add enemy states and attack variations.
- Implement dodging and invulnerability frames.
- Add ranged enemies and projectiles.
- Introduce pathfinding if required.
- Test combat timing, collision, and damage resolution.

### Stage 5: RPG systems

- Add inventory, equipment, and consumables.
- Add experience and leveling.
- Add loot drops, chests, and additional rooms.
- Implement versioned save data and loading error handling.

### Stage 6: Completion and polish

- Implement a boss with distinct attack patterns.
- Add victory and death flows.
- Improve terminal rendering, menus, and combat feedback.
- Verify terminal restoration and save/load reliability.
- Run tests, sanitizers, and a clean build.
- Document build instructions, controls, and gameplay.

## 11. Expectations for Coding Agents

When working on this repository:

- Inspect the existing code and build configuration before proposing or making structural changes.
- Follow existing conventions unless there is a concrete reason to change them.
- Implement features in small, coherent increments.
- Ensure the project builds after meaningful changes.
- Add or update tests when modifying logic that can be tested independently.
- Do not silently change gameplay rules, public interfaces, or project architecture.
- Do not add unrelated features or dependencies.
- Avoid replacing working implementations with broad rewrites when a targeted fix is sufficient.
- Explain significant design choices and trade-offs concisely.
- Identify assumptions when requirements are ambiguous instead of inventing unnecessary requirements.
- Prefer complete, working implementations over large collections of TODOs or placeholder code.
- Do not claim that tests or builds passed unless they were actually run and their results were checked.

When a task is broad, first identify a small, useful implementation step and complete it without losing sight of the overall architecture.

## 12. Definition of Done

A feature is complete when:

- Its behavior matches the intended requirements.
- Its implementation fits the existing architecture.
- Relevant edge cases and failure paths are handled.
- Appropriate tests have been added or updated.
- The project builds successfully.
- No new compiler warnings or sanitizer issues are knowingly introduced.
- User-visible behavior is understandable and consistent with the game's controls and visual language.

**Primary objective:** Build a polished, playable terminal action RPG while learning modern C++ through sound engineering decisions. Favor a small, coherent game with reliable systems and satisfying combat over a large but unfinished collection of features.
