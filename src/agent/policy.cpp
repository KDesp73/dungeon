#include "agent/policy.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dungeon::agent {
namespace {

using nlohmann::json;

using controls::KeyEvent;
using controls::KeyKind;

constexpr std::size_t kInvalidCell = std::numeric_limits<std::size_t>::max();

bool same_cell(AgentObservation::Cell lhs, AgentObservation::Cell rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

std::string string_or(const json& object, const char* key, std::string fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return fallback;
    }
    return object[key].get<std::string>();
}

double number_or(const json& object, const char* key, double fallback) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_number()) {
        return fallback;
    }
    return object[key].get<double>();
}

void parse_choice(const json& answers, const char* key, KevAnswers::Choice& out) {
    if (!answers.contains(key) || !answers[key].is_object()) {
        return;
    }

    const json& question = answers[key];
    out.choice     = string_or(question, "choice", "");
    out.confidence = number_or(question, "confidence", 0.0);

    if (question.contains("probabilities") && question["probabilities"].is_object()) {
        for (auto it = question["probabilities"].begin(); it != question["probabilities"].end();
             ++it) {
            if (it.value().is_number()) {
                out.probabilities[it.key()] = it.value().get<double>();
            }
        }
    }
}

const char* glyph_name(char glyph) noexcept {
    switch (glyph) {
    case 'r':
        return "a rat";
    case 'g':
        return "a goblin";
    case 'O':
        return "an ogre";
    case 'B':
    case 'b':
        return "the boss";
    case 'A':
        return "a boss anchor";
    case '*':
        return "a fireball";
    case 'k':
        return "the key";
    case '!':
        return "a health potion";
    case '?':
        return "a chest";
    case '$':
        return "coins";
    case 'V':
        return "a diamond";
    case '/':
        return "a sword";
    case 'T':
        return "a greatsword";
    case 'd':
        return "a shield";
    case 'm':
        return "chainmail";
    case '[':
        return "the exit";
    default:
        return "floor";
    }
}

char glyph_at(const AgentObservation& observation, AgentObservation::Cell cell) noexcept {
    for (const AgentObservation::Obstacle& obstacle : observation.obstacles) {
        if (obstacle.x == cell.x && obstacle.y == cell.y) {
            return obstacle.glyph;
        }
    }
    if (in_bounds(observation, cell.x, cell.y)) {
        return observation.visible[cell.y][cell.x];
    }
    return ' ';
}

const char* direction_word(KeyKind kind) noexcept {
    switch (kind) {
    case KeyKind::Up:
        return "north";
    case KeyKind::Down:
        return "south";
    case KeyKind::Left:
        return "west";
    case KeyKind::Right:
        return "east";
    default:
        return "around";
    }
}

bool has_obstacle(const AgentObservation& observation, std::size_t x, std::size_t y) noexcept {
    for (const AgentObservation::Obstacle& obstacle : observation.obstacles) {
        if (obstacle.x == x && obstacle.y == y) {
            return true;
        }
    }
    return false;
}

std::optional<AgentObservation::Cell> nearest_obstacle(const AgentObservation& observation,
                                                       char glyph) {
    std::optional<AgentObservation::Cell> best;
    std::size_t best_distance = std::numeric_limits<std::size_t>::max();
    for (const AgentObservation::Obstacle& obstacle : observation.obstacles) {
        if (obstacle.glyph != glyph) {
            continue;
        }
        const AgentObservation::Cell cell{obstacle.x, obstacle.y};
        const std::size_t distance = manhattan(observation.player, cell);
        if (distance < best_distance) {
            best_distance = distance;
            best         = cell;
        }
    }
    return best;
}

bool low_health(const AgentObservation& observation) noexcept {
    return observation.hp > 0 && observation.hp * 3 <= observation.max_hp;
}

/// Chooses a reachable collectible: nearest first, then highest gold value.
std::optional<AgentObservation::Cell> best_loot(const AgentObservation& observation) {
    const AgentObservation::LootInfo* best = nullptr;
    for (const AgentObservation::LootInfo& item : observation.loot) {
        if (!in_bounds(observation, item.cell.x, item.cell.y) ||
            !walkable(observation, item.cell.x, item.cell.y)) {
            continue;
        }
        if (best == nullptr || item.distance < best->distance ||
            (item.distance == best->distance && item.gold > best->gold)) {
            best = &item;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return best->cell;
}

void push(AgentDecision& decision, KeyEvent key) {
    decision.keys.push_back(key);
}

void push_character(AgentDecision& decision, char character) {
    push(decision, KeyEvent::from_character(character));
}

/// Keys that move a store selection from @p from to @p to using wrap-around.
void push_store_selection(AgentDecision& decision, std::size_t from, std::size_t to,
                          std::size_t count) {
    if (count == 0 || from == to) {
        return;
    }

    const auto size = static_cast<std::ptrdiff_t>(count);
    auto delta      = static_cast<std::ptrdiff_t>(to) - static_cast<std::ptrdiff_t>(from);
    if (delta > size / 2) {
        delta -= size;
    } else if (delta < -(size / 2)) {
        delta += size;
    }

    const KeyKind kind = delta > 0 ? KeyKind::Down : KeyKind::Up;
    for (std::ptrdiff_t step = 0; step < (delta > 0 ? delta : -delta); ++step) {
        push(decision, KeyEvent::from_kind(kind));
    }
}

std::optional<std::size_t> parse_index(const std::string& text) {
    if (text.empty() ||
        !std::all_of(text.begin(), text.end(), [](char character) {
            return character >= '0' && character <= '9';
        })) {
        return std::nullopt;
    }

    std::size_t value = 0;
    for (const char character : text) {
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

std::size_t best_store_item(const AgentObservation& observation) {
    std::size_t best = 0;
    int best_score   = std::numeric_limits<int>::min();
    for (std::size_t index = 0; index < observation.store_items.size(); ++index) {
        const auto& item = observation.store_items[index];
        if (!item.affordable) {
            continue;
        }
        const std::string& name = item.name;
        int score               = 0;
        if (name.find("Sword") != std::string::npos || name.find("sword") != std::string::npos) {
            score = 4;
        } else if (name.find("mail") != std::string::npos ||
                   name.find("Shield") != std::string::npos ||
                   name.find("shield") != std::string::npos) {
            score = 3;
        } else if (name.find("Boots") != std::string::npos ||
                   name.find("boots") != std::string::npos) {
            score = 1;
        }
        if (score > best_score) {
            best_score = score;
            best       = index;
        }
    }
    return best;
}

} // namespace

std::vector<LegalAction> legal_actions(const AgentObservation& observation) {
    std::vector<LegalAction> actions;
    if (observation.scene != SceneKind::Game || observation.game_over || observation.victory ||
        observation.tiles.empty()) {
        return actions;
    }

    const auto add_direction = [&](const char* id, KeyKind kind, std::size_t x, std::size_t y) {
        if (!in_bounds(observation, x, y) || !walkable(observation, x, y)) {
            return;
        }
        const AgentObservation::Cell cell{x, y};
        std::ostringstream description;
        description << "Move " << direction_word(kind) << " into "
                    << glyph_name(glyph_at(observation, cell));
        actions.push_back({id, description.str(), KeyEvent::from_kind(kind)});
    };

    const auto& player = observation.player;
    if (player.y > 0) {
        add_direction("up", KeyKind::Up, player.x, player.y - 1);
    }
    if (player.y + 1 < observation.tiles.size()) {
        add_direction("down", KeyKind::Down, player.x, player.y + 1);
    }
    if (player.x > 0) {
        add_direction("left", KeyKind::Left, player.x - 1, player.y);
    }
    if (player.x + 1 < observation.tiles[player.y].size()) {
        add_direction("right", KeyKind::Right, player.x + 1, player.y);
    }

    if (observation.potions > 0 && observation.hp < observation.max_hp) {
        actions.push_back({"drink_potion", "Drink one health potion",
                           KeyEvent::from_character('h')});
    }
    actions.push_back({"wait", "Hold position", {}});
    return actions;
}

std::optional<KeyEvent> step_toward(const AgentObservation& observation,
                                    AgentObservation::Cell target) {
    if (observation.tiles.empty() || same_cell(observation.player, target) ||
        !in_bounds(observation, target.x, target.y) ||
        !walkable(observation, target.x, target.y)) {
        return std::nullopt;
    }

    const std::size_t height = observation.tiles.size();
    const std::size_t width  = observation.tiles.front().size();
    const auto& start        = observation.player;
    if (start.x >= width || start.y >= height) {
        return std::nullopt;
    }

    const KeyKind directions[] = {KeyKind::Up, KeyKind::Down, KeyKind::Left, KeyKind::Right};
    const int offsets[][2]     = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    // First try to route around observed hazards. If that fails (for example,
    // when the target itself is an enemy), allow stepping through them.
    for (int avoid = 1; avoid >= 0; --avoid) {
        std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));
        std::vector<std::vector<AgentObservation::Cell>> parent(
            height, std::vector<AgentObservation::Cell>(width, {kInvalidCell, kInvalidCell}));
        std::queue<AgentObservation::Cell> pending;

        visited[start.y][start.x] = true;
        parent[start.y][start.x]  = start;
        pending.push(start);

        bool found = false;
        while (!pending.empty() && !found) {
            const AgentObservation::Cell current = pending.front();
            pending.pop();

            for (std::size_t index = 0; index < 4; ++index) {
                const int nx = static_cast<int>(current.x) + offsets[index][0];
                const int ny = static_cast<int>(current.y) + offsets[index][1];
                if (nx < 0 || ny < 0) {
                    continue;
                }

                const auto x = static_cast<std::size_t>(nx);
                const auto y = static_cast<std::size_t>(ny);
                if (x >= width || y >= height || !walkable(observation, x, y) || visited[y][x]) {
                    continue;
                }
                if (avoid != 0 && has_obstacle(observation, x, y) &&
                    !(x == target.x && y == target.y)) {
                    continue;
                }

                visited[y][x] = true;
                parent[y][x]  = current;
                pending.push({x, y});
                if (x == target.x && y == target.y) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            continue;
        }

        AgentObservation::Cell step = target;
        while (!same_cell(parent[step.y][step.x], start) &&
               !same_cell(parent[step.y][step.x], step)) {
            step = parent[step.y][step.x];
        }

        for (std::size_t index = 0; index < 4; ++index) {
            const int nx = static_cast<int>(start.x) + offsets[index][0];
            const int ny = static_cast<int>(start.y) + offsets[index][1];
            if (nx >= 0 && ny >= 0 && static_cast<std::size_t>(nx) == step.x &&
                static_cast<std::size_t>(ny) == step.y) {
                return KeyEvent::from_kind(directions[index]);
            }
        }
    }

    return std::nullopt;
}

AgentDecision decide(const AgentObservation& observation, const KevAnswers& answers) {
    AgentDecision decision;

    if (observation.scene == SceneKind::Title) {
        push_character(decision, '\n');
        return decision;
    }
    if (observation.game_over || observation.victory) {
        push_character(decision, 'r');
        return decision;
    }
    if (observation.scene == SceneKind::Pause) {
        return decision;
    }

    if (observation.scene == SceneKind::Store) {
        std::optional<std::size_t> wanted;
        if (answers.received && answers.buy.choice == "skip") {
            push(decision, KeyEvent::from_kind(KeyKind::Escape));
            return decision;
        }
        if (answers.received) {
            wanted = parse_index(answers.buy.choice);
        }
        if (!wanted || *wanted >= observation.store_items.size() ||
            !observation.store_items[*wanted].affordable) {
            bool any_affordable = false;
            for (const auto& item : observation.store_items) {
                any_affordable = any_affordable || item.affordable;
            }
            if (!any_affordable) {
                push(decision, KeyEvent::from_kind(KeyKind::Escape));
                return decision;
            }
            wanted = best_store_item(observation);
        }

        push_store_selection(decision, observation.store_selection, *wanted,
                             observation.store_items.size());
        push_character(decision, '\n');
        return decision;
    }

    const std::vector<LegalAction> actions = legal_actions(observation);
    if (actions.empty()) {
        return decision;
    }

    const bool heal = low_health(observation) && observation.potions > 0 &&
                      (!answers.received || answers.danger.score >= 1.5 ||
                       answers.engage.noul < 0.35);
    if (heal) {
        push_character(decision, 'h');
        return decision;
    }

    if (observation.chest_nearby && !observation.chest_items.empty()) {
        if (!answers.received || answers.chest.choice == "take" ||
            answers.chest.choice.empty()) {
            push_character(decision, '\n');
            return decision;
        }
        if (answers.chest.choice == "next") {
            push_character(decision, ']');
            return decision;
        }
        if (answers.chest.choice == "prev") {
            push_character(decision, '[');
            return decision;
        }
    }

    if (answers.received) {
        for (const LegalAction& action : actions) {
            if (action.id == answers.move.choice) {
                push(decision, action.key);
                return decision;
            }
        }
    }

    std::vector<AgentObservation::Cell> targets;
    const bool key_worth_grabbing = !observation.has_key && observation.key_on_map &&
                                    observation.distance_to_key <= kNearKeyDistance;
    if (!observation.has_key && observation.key_on_map && !key_worth_grabbing) {
        targets.push_back(observation.key_cell);
    }
    if (observation.exit_unlocked) {
        targets.push_back(observation.exit_cell);
    }
    // Detour for coins and items whenever it is safe. This is also the
    // deterministic fallback's way of building wealth for store upgrades.
    if (const auto loot_cell = best_loot(observation)) {
        targets.insert(targets.begin(), *loot_cell);
    }
    if (observation.boss_level) {
        if (observation.anchors_remaining > 0) {
            if (const auto anchor = nearest_obstacle(observation, 'A')) {
                targets.insert(targets.begin(), *anchor);
            }
        } else if (observation.nearest_enemy && observation.nearest_enemy->is_boss) {
            targets.insert(targets.begin(), observation.nearest_enemy->cell);
        }
    }
    if (observation.nearest_enemy && observation.nearest_enemy->distance <= 2) {
        targets.insert(targets.begin(), observation.nearest_enemy->cell);
    }

    if (answers.received) {
        if (answers.goal.choice == "reach_exit" && observation.exit_unlocked) {
            targets.insert(targets.begin(), observation.exit_cell);
        } else if (answers.goal.choice == "find_key" && observation.key_on_map) {
            targets.insert(targets.begin(), observation.key_cell);
        } else if (answers.goal.choice == "fight" && observation.nearest_enemy) {
            targets.insert(targets.begin(), observation.nearest_enemy->cell);
        } else if (answers.goal.choice == "loot") {
            if (const auto loot_cell = best_loot(observation)) {
                targets.insert(targets.begin(), *loot_cell);
            }
        }
    }

    // A nearby key unlocks the exit and costs little to grab, so it outranks
    // optional loot (including Kev's explicit "loot" goal). Only an immediate
    // melee threat is allowed to take precedence.
    if (key_worth_grabbing &&
        !(observation.nearest_enemy && observation.nearest_enemy->distance <= 2)) {
        targets.insert(targets.begin(), observation.key_cell);
    }

    for (const AgentObservation::Cell& target : targets) {
        if (const auto key = step_toward(observation, target)) {
            push(decision, *key);
            return decision;
        }
    }

    // A legal move always exists here, so fall back to the first one that is
    // an actual movement rather than the no-op "wait" action.
    for (const LegalAction& action : actions) {
        if (action.id != "wait" && !action.key.is_none()) {
            push(decision, action.key);
            return decision;
        }
    }
    return decision;
}

bool parse_kev_response(const std::string& body, KevAnswers& answers) {
    const json parsed = json::parse(body, nullptr, false);
    if (!parsed.is_object() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
        return false;
    }

    const json& source = parsed["answers"];
    KevAnswers result;
    parse_choice(source, "goal", result.goal);
    parse_choice(source, "move", result.move);
    parse_choice(source, "buy", result.buy);
    parse_choice(source, "chest", result.chest);

    if (source.contains("engage") && source["engage"].is_object()) {
        result.engage.noul = number_or(source["engage"], "noul", 0.5);
    }
    if (source.contains("danger") && source["danger"].is_object()) {
        result.danger.score      = number_or(source["danger"], "score", 0.0);
        result.danger.confidence = number_or(source["danger"], "confidence", 0.0);
    }

    result.received = !result.goal.choice.empty() || !result.move.choice.empty() ||
                      !result.buy.choice.empty() || !result.chest.choice.empty() ||
                      source.contains("engage") || source.contains("danger");
    answers = result;
    return result.received;
}

std::string build_kev_request(const AgentObservation& observation, std::size_t observation_id) {
    json state = json::parse(observation.to_json());
    state["observation_id"] = observation_id;

    json goal = json::object();
    goal["type"]         = "choice";
    goal["instructions"] =
        "Choose the player's immediate objective. Prefer survival, then grab the key when it is "
        "close, then collect nearby coins and valuable items (gold buys permanent upgrades), "
        "then reach the exit.";
    goal["criteria"]     = {{"heal", "Health is low and a potion is available"},
                            {"flee", "The nearest enemy is dangerous and avoiding it is safer"},
                            {"fight", "The nearest enemy is worth killing for gold or because it "
                                      "blocks progress"},
                            {"find_key", "The exit is locked; grab the key before optional loot "
                                      "when it is close"},
                            {"reach_exit", "The player has the key and should descend"},
                            {"loot", "Coins, diamonds, equipment, or a chest are reachable and "
                                      "safe to collect; gold pays for upgrades"}};

    json engage = json::object();
    engage["type"]         = "noul";
    engage["instructions"] = "Is fighting the nearest enemy currently a good idea given the "
                             "player's health, attack, defence and potions?";

    json danger = json::object();
    danger["type"]         = "score";
    danger["instructions"] = "How urgent is the danger in the visible area?";
    danger["criteria"]     = {"safe", "be careful", "act now"};

    json move = json::object();
    move["type"]         = "choice";
    move["instructions"] = "Choose the best legal next action. Do not choose an option that is "
                           "not listed.";
    move["criteria"]     = json::object();
    for (const LegalAction& action : legal_actions(observation)) {
        move["criteria"][action.id] = action.description;
    }
    if (move["criteria"].empty()) {
        move["criteria"]["wait"] = "Hold position";
    }

    json questions = json::object();
    questions["goal"]   = goal;
    questions["engage"] = engage;
    questions["danger"] = danger;
    questions["move"]   = move;

    if (observation.scene == SceneKind::Store) {
        json buy = json::object();
        buy["type"]         = "choice";
        buy["instructions"] = "Which affordable upgrade best improves the player's chance of "
                              "surviving deeper dungeons?";
        buy["criteria"]     = json::object();
        for (std::size_t index = 0; index < observation.store_items.size(); ++index) {
            const auto& item = observation.store_items[index];
            if (!item.affordable) {
                continue;
            }
            std::ostringstream description;
            description << item.name << ", " << item.description << ", price " << item.price;
            buy["criteria"][std::to_string(index)] = description.str();
        }
        buy["criteria"]["skip"] = "Save gold";
        questions["buy"]        = buy;
    }

    if (observation.chest_nearby && !observation.chest_items.empty()) {
        json chest = json::object();
        chest["type"]         = "choice";
        chest["instructions"] = "What should the player do with the nearby chest?";
        chest["criteria"]     = {{"take", "Take the selected item"},
                                 {"next", "Look at the next item"},
                                 {"prev", "Look at the previous item"},
                                 {"leave", "Walk away from the chest"}};
        questions["chest"]    = chest;
    }

    json request = json::object();
    request["state"]   = state;
    request["model"]   = "kev-latest";
    request["questions"] = questions;
    return request.dump();
}

} // namespace dungeon::agent
