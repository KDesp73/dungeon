# Kev agent implementation plan

Status: planned. This document records how to implement the `TODO` entry about a
Kev-like model playing the game. No code changes are included yet.

## Goal

Make `jaredpalmer/kev-0.8b` play `dungeon` through a typed decision interface,
without giving the model direct control over game state or bypassing the
existing scene/input system.

Kev reads one shared `state` and answers several finite questions in one
forward pass. It does not generate text or choose arbitrary actions. The game
code must therefore:

1. Build a compact observation from `Game`/`Map`.
2. Enumerate legal answers and actions.
3. Ask Kev narrow `choice`, `noul`, and `score` questions.
4. Combine the returned probabilities with deterministic rules.
5. Turn the chosen action into ordinary input events.

This is the same pattern used by Kev's chess demo: the board is the state,
legal moves are the options, and code validates and applies the chosen move.

Kev-0.8B is small and weak out of domain, so zero-shot play is expected to be
mediocre. The plan includes a fine-tuning and evaluation path.

## Design principles

- **Code owns the workflow.** Kev supplies judgments, not side effects.
- **Use the existing input path.** The bot should emit `controls::KeyEvent`s
  that flow through `SceneManager::handle_key()`, just like human input.
- **Keep the 60 Hz loop free of inference.** Model calls are asynchronous and
  run at a lower cadence.
- **Observe structured state, not ANSI output.** Build observations from
  `Game`, `Map`, and entity APIs.
- **Keep responses finite and legal.** The model can only choose from options
  the game code generated.
- **Start native-only.** WASM can be added later through the existing JS key
  bridge.

## Architecture

```text
GameScene::game() / Game / Map / entities
              |
              v
       AgentObservation builder
              |
              v
       Kev request builder  --->  POST /v1/systemone
              |                          |
              |                          v
              |                 choice / noul / score answers
              v
       deterministic policy layer
              |
              v
       controls::KeyEvent queue
              |
              v
Application::step() -> SceneManager::handle_key()
              |
              v
   GameScene / StoreScene / TitleScene
```

## Phase 1: injectable input

Currently `Application::step()` calls `controls::poll_key()` directly and
dispatches at most one key per frame:

```cpp
const dungeon::controls::KeyEvent key = dungeon::controls::poll_key();
if (!key.is_none()) {
    static_cast<void>(_scenes.handle_key(key, _context));
}
```

Add a `KeySource` abstraction under `include/dungeon/controls/`:

```cpp
// include/dungeon/controls/input_source.hpp
#pragma once

#include "dungeon/controls.hpp"

#include <deque>

namespace dungeon::controls {

class KeySource {
  public:
    virtual ~KeySource() = default;
    virtual KeyEvent poll() = 0;
};

class TerminalKeySource final : public KeySource {
  public:
    KeyEvent poll() override { return poll_key(); }
};

class QueuedKeySource final : public KeySource {
  public:
    void push(KeyEvent key) { _keys.push_back(key); }

    KeyEvent poll() override {
        if (_keys.empty()) return {};
        const KeyEvent key = _keys.front();
        _keys.pop_front();
        return key;
    }

  private:
    std::deque<KeyEvent> _keys;
};

} // namespace dungeon::controls
```

Change `Application` in `src/main.cpp` to own:

- a `std::unique_ptr<controls::KeySource> _input` for real input,
- a `controls::QueuedKeySource _injected` for agent input,
- an optional `std::unique_ptr<agent::Agent> _agent`.

Drain queued keys before terminal keys, several per frame:

```cpp
for (int i = 0; i < 16; ++i) {
    KeyEvent key = _injected.poll();
    if (key.is_none()) key = _input->poll();
    if (key.is_none()) break;

    static_cast<void>(_scenes.handle_key(key, _context));
}
```

Why this design:

- Human play is unchanged.
- Bot play exercises the same scene handlers.
- `StoreScene`, `PauseScene`, and `TitleScene` continue to receive only the
  top-scene input.
- Store auto-opening through `GameScene::update_store_proximity()` still works.
- Existing scene tests already use this path.

`Application` currently lives in an anonymous namespace in `main.cpp`. If the
driver needs direct unit tests, move it into `dungeon_core` under
`include/dungeon/` and `src/`.

## Phase 2: agent subsystem

Add a new subsystem:

```text
include/dungeon/agent/agent.hpp
include/dungeon/agent/observation.hpp
include/dungeon/agent/kev_client.hpp
src/agent/agent.cpp
src/agent/observation.cpp
src/agent/kev_client.cpp
tests/agent/agent_tests.cpp
```

The CMake build already globs `src/*.cpp`, so new source files under `src/`
are picked up automatically.

Suggested interface:

```cpp
namespace dungeon::agent {

struct AgentDecision {
    std::vector<controls::KeyEvent> keys;
};

class Agent {
  public:
    virtual ~Agent() = default;

    virtual void observe(const game::Game& game,
                         const scene::SceneManager& scenes) = 0;

    virtual std::optional<AgentDecision> poll() = 0;
};

} // namespace dungeon::agent
```

`Application::step()` calls `observe()` every N ticks and pushes any returned
key sequence into `_injected`.

### Observation builder

`AgentObservation::from(const game::Game&, const scene::SceneManager&)` should
collect:

- `depth`, `store_level()`, `boss_level()`, `game_over()`, `victory()`
- `player->stats()`, `max_hp()`, `inventory()`, `has_key()`
- a small tile crop from `map.at()` / `walkable()` / `glyph()`
- `map.exit()` and `map.exit_unlocked()`
- entities from `map.entity_ptrs()` with `dynamic_cast` for concrete types
- `nearest_enemy()`, `boss()`, `anchors_remaining()`
- `nearby_chest()`, `nearby_shopkeeper()`, `store_items()`
- `messages()`, `run_stats()`
- current top scene type, via `dynamic_cast`, so store/title questions can be
  selected correctly

Use `entity_ptrs()`, not `entities()`. `Map::entities()` copies through the
base class and slices derived entity state.

There is currently no fog-of-war. `TODO` line 18, `Discovering`, would change
that. If visibility is implemented later, add the same filter to the agent
observation so it does not cheat.

`Game::set_action_hook()` is useful for recording item-related events in
training logs, but it is not an input hook.

## Phase 3: Kev request and response

Start a local server:

```sh
git clone https://github.com/jaredpalmer/kev.git
cd kev
uv sync --extra serve
uv run --extra serve python -m kev.serve \
  --run jaredpalmer/kev-0.8b --port 8008
```

The endpoint is `POST http://127.0.0.1:8008/v1/systemone`.

Example request shape:

```json
{
  "state": {
    "game": "dungeon",
    "depth": 7,
    "player": {
      "hp": 14,
      "max_hp": 30,
      "gold": 42,
      "attack": 6,
      "defence": 2,
      "potions": 1,
      "has_key": false
    },
    "progress": {
      "exit_unlocked": false,
      "distance_to_key": 12,
      "nearest_enemy": "goblin, hp 9, distance 3",
      "anchors_remaining": 0
    },
    "visible_map": [
      "#################",
      "#...............#",
      "#....@..g...$...#",
      "#....#...#......#",
      "#################"
    ],
    "recent_messages": [
      "You hit the goblin.",
      "The goblin hits you."
    ]
  },
  "model": "kev-latest",
  "questions": {
    "goal": {
      "type": "choice",
      "instructions": "Choose the player's immediate objective. Prefer survival, then getting the key, then reaching the exit.",
      "criteria": {
        "heal": "Health is low and a potion is available",
        "flee": "The nearest enemy is dangerous and avoiding it is safer",
        "fight": "The nearest enemy is worth killing for gold or because it blocks progress",
        "find_key": "The exit is locked and the key has not been found",
        "reach_exit": "The player has the key and should descend",
        "loot": "A chest or valuable item is nearby and it is safe to take"
      }
    },
    "engage": {
      "type": "noul",
      "instructions": "Is fighting the nearest enemy currently a good idea given the player's health, attack, defence and potions?"
    },
    "danger": {
      "type": "score",
      "instructions": "How urgent is the danger in the visible area?",
      "criteria": ["safe", "be careful", "act now"]
    },
    "move": {
      "type": "choice",
      "instructions": "Choose the best legal next action. Do not choose an option that is not listed.",
      "criteria": {
        "up": "Move north into floor",
        "left": "Move west toward a goblin and attack it",
        "drink_potion": "Drink one health potion",
        "wait": "Hold position"
      }
    }
  }
}
```

Store levels use a different question:

```json
"buy": {
  "type": "choice",
  "instructions": "Which affordable upgrade best improves the player's chance of surviving deeper dungeons?",
  "criteria": {
    "0": "Sword, +2 attack, price 30",
    "1": "Shield, +2 defence, price 25",
    "skip": "Save gold"
  }
}
```

Kev's response contains a distribution per question, for example:

```json
{
  "answers": {
    "goal": {
      "type": "choice",
      "choice": "find_key",
      "confidence": 0.31,
      "probabilities": {
        "heal": 0.05,
        "flee": 0.08,
        "fight": 0.12,
        "find_key": 0.57,
        "reach_exit": 0.02,
        "loot": 0.16
      }
    },
    "engage": { "type": "noul", "noul": 0.41 },
    "danger": {
      "type": "score",
      "score": 0.72,
      "confidence": 0.28,
      "probabilities": { "0": 0.45, "1": 0.38, "2": 0.17 }
    }
  }
}
```

Keep thresholds and policy in C++:

```cpp
if (hp_low && potions > 0 && danger.score >= 1.5) {
    emit(KeyEvent::from_character('h'));
} else if (goal.choice == "reach_exit" && has_key) {
    emit(step_toward(exit));
} else if (engage.noul > 0.65 && danger.score < 2.0) {
    emit(step_toward(nearest_enemy));
} else {
    emit(step_toward(key));
}
```

Do not let Kev output arbitrary key names or game commands. Only options
generated by the game code are valid.

## Mapping answers to key events

| Kev answer | Key event sequence |
| --- | --- |
| `move_up` | `KeyKind::Up` |
| `move_down` | `KeyKind::Down` |
| `move_left` | `KeyKind::Left` |
| `move_right` | `KeyKind::Right` |
| `use_potion` | `'h'` |
| `chest_take` | `'\n'` |
| `chest_next` | `']'` |
| `chest_prev` | `'['` |
| `buy_item_2` | Up/Down until `StoreScene::selected() == 2`, then Enter |
| `leave_store` | Escape |
| `restart` | `'r'` |
| `start_game` | Enter |

Because `SceneManager::handle_key()` only dispatches to the top scene, these
key events naturally reach the correct scene.

## Transport choices

Two reasonable implementations:

1. **Python sidecar** (`tools/kev_agent.py`)
   - C++ writes one JSON observation line to stdin.
   - Python uses `urllib.request` to call Kev.
   - Python writes one JSON decision line to stdout.
   - No new C++ HTTP or JSON dependency.

2. **Native C++ client**
   - Add optional `nlohmann/json` through `FetchContent`.
   - Use a small HTTP client or POSIX sockets.
   - Guard with a CMake option such as `DUNGEON_ENABLE_KEV`.

Start with the sidecar or a minimal optional native client. Avoid a hard runtime
dependency because the project currently has none and also targets WASM.

## Latency and scheduling

- Native Kev-0.8B: roughly 20–100 ms per request depending on hardware.
- Apple Silicon MLX: roughly 150 ms for a new short state, ~28 ms cached.
- The game loop runs at 60 Hz.

Therefore:

- Do not call Kev inline from `Application::step()`.
- Use a worker thread on native builds.
- Submit observations every 8–15 ticks, not every tick.
- Tag each observation with an incrementing ID.
- Discard replies whose ID no longer matches the current observation.
- Use a deterministic fallback while waiting.
- Never touch `Game` from the worker thread; pass immutable snapshots only.

For offline evaluation, a simpler mode may pause simulation while waiting for
a decision.

## CLI flags

Add native-only flags in `main.cpp` under `#ifndef __EMSCRIPTEN__`:

```text
--agent <none|scripted|kev>   Agent driver to use, default none
--kev-url <url>               Kev endpoint, default http://127.0.0.1:8008
--kev-interval <ticks>        Observation cadence, default 12
--headless                    Run without terminal rendering for evaluation
```

In agent mode, either auto-start the game or synthesize the title screen's
start key.

## WASM

The browser build already has a synthetic input bridge:

- C ABI: `dungeon_push_key(kind, value)` in `src/controls.cpp`.
- JS helper: `pushKey(kind, value)` in `web/shell.html`.
- Key codes: `0` character, `1` Up, `2` Down, `3` Left, `4` Right, `5` Escape.

An async JS model loop can call `pushKey()` when a fetch resolves. No
`-sASYNCIFY` is enabled, so do not make blocking JS-to-C++ model calls.

The missing piece is sending observations from C++ to JS. Options:

- export a C function returning observation JSON and add it to
  `-sEXPORTED_FUNCTIONS`, or
- start native-only and defer WASM until the native path is stable.

Any new JS-callable C function must be marked `EMSCRIPTEN_KEEPALIVE` and added
to the exported functions list in `CMakeLists.txt`.

## Fine-tuning and evaluation

Zero-shot Kev-0.8B is unlikely to play well because dungeon state is far from
its training distribution. The plan should include a data pipeline.

1. Write a scripted expert bot using BFS and health/safety rules.
2. Run it over many fixed seeds.
3. Log observations and labels in Kev's JSONL format:

```json
{
  "state": { "...": "..." },
  "questions": {
    "goal": { "...": "...", "label": "find_key" },
    "engage": { "...": "...", "label": false },
    "danger": { "...": "...", "label": 1 },
    "move": { "...": "...", "label": "left" }
  }
}
```

4. Hold out whole seeds for evaluation.
5. Fine-tune from the released checkpoint:

```sh
uv run python -m kev.train \
  --data dungeon-kev/train.jsonl \
  --base Qwen/Qwen3.5-0.8B-Base \
  --init_from jaredpalmer/kev-0.8b \
  --epochs 2 --lr 2e-5 --batch 1 --accum 8 \
  --dtype bf16 --device auto \
  --out dungeon-kev/run
```

6. Benchmark and calibrate before serving the fine-tuned run.

Evaluate by depth reached, dungeons completed, deaths, bosses defeated,
win rate over seeds, and decisions per second. Compare at least three agents:

- scripted baseline,
- zero-shot Kev,
- fine-tuned Kev.

Keep option order fixed in production and add permutation checks in
evaluation, since option order can change Kev's answers.

## Tests

Add a `tests/agent` suite:

- observation JSON is stable for a fixed `MapSpec`,
- fake Kev answers produce the expected key sequence,
- illegal options are never emitted,
- store purchases select the requested item,
- headless fixed-seed run makes progress,
- stale observation replies are discarded,
- no test requires a running Kev server.

Register the suite in `tests/CMakeLists.txt`:

```cmake
foreach(suite IN ITEMS core display game map scene agent)
    add_test(NAME ${suite} COMMAND dungeon_tests --suite ${suite})
endforeach()
```

Also add the source file to `add_executable(dungeon_tests ...)`.

## File touchpoints

| Purpose | Files |
| --- | --- |
| Injectable input | `include/dungeon/controls/input_source.hpp`, `src/main.cpp` |
| Agent driver/interface | `include/dungeon/agent/`, `src/agent/` |
| Observation builder | `include/dungeon/agent/observation.hpp`, `src/agent/observation.cpp` |
| Game state access | `include/dungeon/game.hpp`, `include/dungeon/map/map.hpp`, `include/dungeon/map/entities.hpp` |
| Scene-aware routing | `include/dungeon/scene/`, `src/scene/` |
| Kev transport | `src/agent/kev_client.cpp`, optional `tools/kev_agent.py` |
| CLI flags | `src/main.cpp` |
| WASM bridge | `web/shell.html`, `src/controls.cpp`, `CMakeLists.txt` |
| Tests | `tests/CMakeLists.txt`, `tests/agent/` |
| Documentation | `README.md`, `TODO`, `docs/References.md` |

## Rollout checklist

1. Add `KeySource`/`QueuedKeySource` and injected key queue.
2. Add a scripted BFS bot that emits only `KeyEvent`s.
3. Add `AgentObservation` and JSON serialization.
4. Add a fake Kev client and policy tests.
5. Add the real Kev client and `--agent kev`.
6. Add headless seeded evaluation.
7. Generate training data and fine-tune.
8. Add WASM support if desired.
9. Update `README.md` and mark the `TODO` done.

## References

- Kev model card: <https://huggingface.co/jaredpalmer/kev-0.8b>
- Kev repository: <https://github.com/jaredpalmer/kev>
- Kev chess demo source: `playground/src/lib/chess.ts` and
  `playground/src/components/chess-game.tsx` in the Kev repository
- TypeSafe System One API: <https://docs.typesafe.ai/api/system-one>
- Jev architecture write-up: <https://archerhume.com/posts/jevs-architecture-unmasked>
