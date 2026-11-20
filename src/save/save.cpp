#include "dungeon/save/save.hpp"

#include "dungeon/display/style.hpp"
#include "dungeon/map/chance.hpp"
#include "dungeon/map/entities.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dungeon::save {

namespace detail {

constexpr std::array<std::uint8_t, 8> kMagic = {
    static_cast<std::uint8_t>('D'), static_cast<std::uint8_t>('U'),
    static_cast<std::uint8_t>('N'), static_cast<std::uint8_t>('G'),
    static_cast<std::uint8_t>('E'), static_cast<std::uint8_t>('O'),
    static_cast<std::uint8_t>('N'), static_cast<std::uint8_t>('S')};

constexpr std::uint32_t kVersion = 1;
constexpr std::size_t   kHeaderSize = 40;
constexpr std::uint64_t kMaxMapCells = 4'000'000ULL;

class FormatError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class Writer {
  public:
    void raw(const std::uint8_t* data, std::size_t size) {
        _bytes.insert(_bytes.end(), data, data + size);
    }

    void u8(std::uint8_t value) { _bytes.push_back(value); }

    void u32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            u8(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void u64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            u8(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }

    void boolean(bool value) { u8(value ? 1U : 0U); }

    void f64(double value) {
        static_assert(sizeof(double) == 8, "The save format requires 64-bit doubles");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void text(std::string_view value) {
        u64(static_cast<std::uint64_t>(value.size()));
        raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }

    template <typename Range, typename WriteOne>
    void sequence(const Range& values, WriteOne&& write_one) {
        u64(static_cast<std::uint64_t>(values.size()));
        for (const auto& value : values) {
            write_one(value);
        }
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return _bytes; }

  private:
    std::vector<std::uint8_t> _bytes;
};

class Reader {
  public:
    explicit Reader(const std::vector<std::uint8_t>& data) : _data(data) {}

    [[nodiscard]] std::size_t remaining() const noexcept { return _data.size() - _position; }

    std::vector<std::uint8_t> bytes(std::size_t count) {
        require(count);
        const auto begin = _data.begin() + static_cast<std::ptrdiff_t>(_position);
        _position += count;
        return {begin, begin + static_cast<std::ptrdiff_t>(count)};
    }

    std::uint8_t u8() { return bytes(1).front(); }

    std::uint32_t u32() {
        std::uint32_t result = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            result |= static_cast<std::uint32_t>(u8()) << shift;
        }
        return result;
    }

    std::uint64_t u64() {
        std::uint64_t result = 0;
        for (int shift = 0; shift < 64; shift += 8) {
            result |= static_cast<std::uint64_t>(u8()) << shift;
        }
        return result;
    }

    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    std::size_t size() {
        const std::uint64_t value = u64();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw FormatError("Save file contains an oversized count");
        }
        return static_cast<std::size_t>(value);
    }

    bool boolean() {
        const std::uint8_t value = u8();
        if (value > 1U) {
            throw FormatError("Save file contains an invalid boolean");
        }
        return value != 0U;
    }

    double f64() {
        static_assert(sizeof(double) == 8, "The save format requires 64-bit doubles");
        const std::uint64_t bits = u64();
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::string text() {
        const std::size_t count = size();
        const auto         raw  = bytes(count);
        return {raw.begin(), raw.end()};
    }

    template <typename T, typename ReadOne>
    std::vector<T> sequence(ReadOne&& read_one) {
        const std::size_t count = size();
        if (count > remaining()) {
            throw FormatError("Save file declares more items than it contains");
        }

        std::vector<T> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            result.push_back(read_one());
        }
        return result;
    }

  private:
    void require(std::size_t count) const {
        if (count > remaining()) {
            throw FormatError("Save file ended unexpectedly");
        }
    }

    const std::vector<std::uint8_t>& _data;
    std::size_t                       _position = 0;
};

template <typename Enum>
void write_enum(Writer& writer, Enum value) {
    writer.u8(static_cast<std::uint8_t>(value));
}

template <typename Enum>
Enum read_enum(Reader& reader, std::uint8_t maximum) {
    const std::uint8_t value = reader.u8();
    if (value > maximum) {
        throw FormatError("Save file contains an invalid enum value");
    }
    return static_cast<Enum>(value);
}

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime       = 1099511628211ULL;

    std::uint64_t hash = kOffsetBasis;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= kPrime;
    }
    return hash;
}

SaveResult write_atomic(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    const std::string temporary = path + ".tmp";

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        return SaveResult::failure("Could not open the save file for writing.");
    }

    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        std::remove(temporary.c_str());
        return SaveResult::failure("Could not write the save file.");
    }

#if defined(_WIN32)
    std::remove(path.c_str());
#endif
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return SaveResult::failure("Could not replace the save file.");
    }

    return SaveResult::success("Game saved.");
}

} // namespace detail

using detail::FormatError;
using detail::Reader;
using detail::Writer;

class BinaryCodec {
  public:
    static void write(Writer& writer, const game::Game& game) { write_game(writer, game); }

    /// Fully decodes a payload before replacing @p game, so validation failures
    /// leave the caller's run untouched. Callbacks are preserved.
    static void load(Reader& reader, game::Game& game) {
        game::Game loaded;
        read_game(reader, loaded);
        if (reader.remaining() != 0) {
            throw FormatError("The save file contains trailing data.");
        }

        auto hook = std::move(game._action_hook);
        game      = std::move(loaded);
        game._action_hook = std::move(hook);
    }

  private:
    using Entity = map::entities::Entity;
    using Item   = map::entities::Item;
    using Stats  = map::entities::Stats;

    static constexpr std::uint8_t kTagPlayer     = 1;
    static constexpr std::uint8_t kTagBoss       = 2;
    static constexpr std::uint8_t kTagEnemy      = 3;
    static constexpr std::uint8_t kTagAnchor     = 4;
    static constexpr std::uint8_t kTagFireball   = 5;
    static constexpr std::uint8_t kTagShopkeeper = 6;
    static constexpr std::uint8_t kTagStand      = 7;
    static constexpr std::uint8_t kTagChest      = 8;
    static constexpr std::uint8_t kTagItem       = 9;
    static constexpr std::uint8_t kTagCoin       = 10;
    static constexpr std::uint8_t kTagCoinDouble = 11;
    static constexpr std::uint8_t kTagDiamond    = 12;
    static constexpr std::uint8_t kTagPotion     = 13;
    static constexpr std::uint8_t kTagKey        = 14;
    static constexpr std::uint8_t kTagSword      = 15;
    static constexpr std::uint8_t kTagGreatsword = 16;
    static constexpr std::uint8_t kTagShield     = 17;
    static constexpr std::uint8_t kTagChainmail  = 18;
    static constexpr std::uint8_t kTagBoots      = 19;

    static void write_chance(Writer& writer, const map::chance::Chance& chance) {
        writer.u32(chance._seed);

        std::ostringstream state;
        state << chance._rng;
        writer.text(state.str());
    }

    static void read_chance(Reader& reader, map::chance::Chance& chance) {
        chance._seed = reader.u32();

        const std::string state = reader.text();
        std::istringstream stream(state);
        stream >> chance._rng;
        if (stream.fail()) {
            throw FormatError("Save file contains an invalid random generator state");
        }
    }

    static void write_point(Writer& writer, const map::Point& point) {
        writer.u64(static_cast<std::uint64_t>(point.x));
        writer.u64(static_cast<std::uint64_t>(point.y));
    }

    static map::Point read_point(Reader& reader) {
        return {reader.size(), reader.size()};
    }

    static void write_stats(Writer& writer, const Stats& stats) {
        writer.u64(static_cast<std::uint64_t>(stats.level));
        writer.i32(stats.hp);
        writer.i32(stats.attack);
        writer.i32(stats.defence);
        writer.i32(stats.stamina);
        writer.i32(stats.gold);
    }

    static Stats read_stats(Reader& reader) {
        return Stats{reader.size(), reader.i32(), reader.i32(), reader.i32(), reader.i32(),
                     reader.i32()};
    }

    static void write_entity_base(Writer& writer, const Entity& entity) {
        writer.u64(static_cast<std::uint64_t>(entity.x));
        writer.u64(static_cast<std::uint64_t>(entity.y));
        writer.u8(static_cast<std::uint8_t>(entity._tile));
        write_enum(writer, entity._color);
        write_enum(writer, entity._style);
        writer.boolean(entity._visible);
        writer.i32(entity._id);
    }

    static void read_entity_base(Reader& reader, Entity& entity, int& max_id,
                                 std::unordered_set<int>& ids) {
        entity.x      = reader.size();
        entity.y      = reader.size();
        entity._tile  = static_cast<char>(reader.u8());
        entity._color = read_enum<map::entities::Color>(
            reader, static_cast<std::uint8_t>(map::entities::Color::BrightWhite));
        entity._style = read_enum<map::entities::Style>(
            reader, static_cast<std::uint8_t>(map::entities::Style::Strikethrough));
        entity._visible = reader.boolean();
        entity._id      = reader.i32();

        if (entity._id <= 0 || !ids.insert(entity._id).second) {
            throw FormatError("Save file contains duplicate or invalid entity ids");
        }
        max_id = std::max(max_id, entity._id);
    }

    static void write_item_fields(Writer& writer, const Item& item) {
        write_stats(writer, item._base_stats);
        write_stats(writer, item._stats_diff);
        write_enum(writer, item._type);
        write_enum(writer, item._kind);
        writer.u64(static_cast<std::uint64_t>(item._level));
    }

    static void read_item_fields(Reader& reader, Item& item) {
        item._base_stats = read_stats(reader);
        item._stats_diff = read_stats(reader);
        item._type       = read_enum<map::entities::ItemType>(
            reader, static_cast<std::uint8_t>(map::entities::ItemType::Interactable));
        item._kind  = read_enum<map::entities::ItemKind>(
            reader, static_cast<std::uint8_t>(map::entities::ItemKind::Boots));
        item._level = reader.size();
    }

    static Item read_item(Reader& reader, int& max_id, std::unordered_set<int>& ids) {
        const auto kind = read_enum<map::entities::ItemKind>(
            reader, static_cast<std::uint8_t>(map::entities::ItemKind::Boots));

        Item item = map::entities::make_item(kind);
        read_entity_base(reader, item, max_id, ids);
        read_item_fields(reader, item);
        return item;
    }

    static map::EntityPtr read_item_ptr(Reader& reader, int& max_id,
                                        std::unordered_set<int>& ids) {
        return std::make_shared<Item>(read_item(reader, max_id, ids));
    }

    template <typename ItemType>
    static map::EntityPtr read_typed_item(Reader& reader, int& max_id,
                                          std::unordered_set<int>& ids) {
        ItemType item{0, 0};
        read_entity_base(reader, item, max_id, ids);
        read_item_fields(reader, item);
        return std::make_shared<ItemType>(std::move(item));
    }

    static void write_enemy_fields(Writer& writer, const map::entities::Enemy& enemy) {
        write_stats(writer, enemy._stats);
        write_enum(writer, enemy._kind);
        writer.i32(enemy._max_hp);
        writer.i32(enemy._gold_bonus);
        writer.f64(enemy._move_elapsed);
        writer.f64(enemy._attack_elapsed);
        writer.boolean(enemy._alerted);
        writer.i32(enemy._wander_direction);
    }

    static void read_enemy_fields(Reader& reader, map::entities::Enemy& enemy) {
        enemy._stats            = read_stats(reader);
        enemy._kind             = read_enum<map::entities::EnemyKind>(
            reader, static_cast<std::uint8_t>(map::entities::EnemyKind::Boss));
        enemy._max_hp           = reader.i32();
        enemy._gold_bonus       = reader.i32();
        enemy._move_elapsed     = reader.f64();
        enemy._attack_elapsed   = reader.f64();
        enemy._alerted          = reader.boolean();
        enemy._wander_direction = reader.i32();
    }

    static void write_entity(Writer& writer, const Entity& entity) {
        if (const auto* player = dynamic_cast<const map::entities::Player*>(&entity);
            player != nullptr) {
            writer.u8(kTagPlayer);
            write_entity_base(writer, *player);
            write_stats(writer, player->_stats);
            writer.sequence(player->inventory(),
                            [&](const Item& item) { write_item(writer, item); });
            return;
        }

        if (const auto* boss = dynamic_cast<const map::entities::Boss*>(&entity);
            boss != nullptr) {
            writer.u8(kTagBoss);
            writer.u64(static_cast<std::uint64_t>(boss->_tier));
            writer.boolean(boss->_final);
            write_entity_base(writer, *boss);
            write_enemy_fields(writer, *boss);
            writer.u64(static_cast<std::uint64_t>(boss->_anchors_remaining));
            writer.f64(boss->_fireball_elapsed);
            writer.u64(static_cast<std::uint64_t>(boss->_volley_rotation));
            writer.u64(static_cast<std::uint64_t>(boss->_volleys_fired));
            write_enum(writer, boss->_attack);
            return;
        }

        if (const auto* enemy = dynamic_cast<const map::entities::Enemy*>(&entity);
            enemy != nullptr) {
            writer.u8(kTagEnemy);
            write_enum(writer, enemy->_kind);
            write_entity_base(writer, *enemy);
            write_enemy_fields(writer, *enemy);
            return;
        }

        if (const auto* anchor = dynamic_cast<const map::entities::Anchor*>(&entity);
            anchor != nullptr) {
            writer.u8(kTagAnchor);
            write_entity_base(writer, *anchor);
            writer.i32(anchor->_hits);
            return;
        }

        if (const auto* fireball = dynamic_cast<const map::entities::Fireball*>(&entity);
            fireball != nullptr) {
            writer.u8(kTagFireball);
            write_entity_base(writer, *fireball);
            writer.i32(fireball->_dx);
            writer.i32(fireball->_dy);
            writer.i32(fireball->_damage);
            writer.u64(static_cast<std::uint64_t>(fireball->_range));
            writer.f64(fireball->_step_interval);
            writer.f64(fireball->_elapsed);
            return;
        }

        if (dynamic_cast<const map::entities::Shopkeeper*>(&entity) != nullptr) {
            writer.u8(kTagShopkeeper);
            write_entity_base(writer, entity);
            return;
        }

        if (dynamic_cast<const map::entities::StoreStand*>(&entity) != nullptr) {
            writer.u8(kTagStand);
            write_entity_base(writer, entity);
            return;
        }

        if (const auto* chest = dynamic_cast<const map::entities::Chest*>(&entity);
            chest != nullptr) {
            writer.u8(kTagChest);
            write_entity_base(writer, *chest);
            write_item_fields(writer, *chest);
            writer.sequence(chest->contents(), [&](const Item& item) { write_item(writer, item); });
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Coin*>(&entity);
            item != nullptr) {
            writer.u8(kTagCoin);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::CoinDouble*>(&entity);
            item != nullptr) {
            writer.u8(kTagCoinDouble);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Diamond*>(&entity);
            item != nullptr) {
            writer.u8(kTagDiamond);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::HealthPotion*>(&entity);
            item != nullptr) {
            writer.u8(kTagPotion);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Key*>(&entity); item != nullptr) {
            writer.u8(kTagKey);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Sword*>(&entity);
            item != nullptr) {
            writer.u8(kTagSword);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Greatsword*>(&entity);
            item != nullptr) {
            writer.u8(kTagGreatsword);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Shield*>(&entity);
            item != nullptr) {
            writer.u8(kTagShield);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Chainmail*>(&entity);
            item != nullptr) {
            writer.u8(kTagChainmail);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const map::entities::Boots*>(&entity);
            item != nullptr) {
            writer.u8(kTagBoots);
            write_item_payload(writer, *item);
            return;
        }

        if (const auto* item = dynamic_cast<const Item*>(&entity); item != nullptr) {
            writer.u8(kTagItem);
            write_item(writer, *item);
            return;
        }

        throw FormatError("Save file contains an unsupported entity type");
    }

    static void write_item_payload(Writer& writer, const Item& item) {
        write_entity_base(writer, item);
        write_item_fields(writer, item);
    }

    static void write_item(Writer& writer, const Item& item) {
        write_enum(writer, item._kind);
        write_item_payload(writer, item);
    }

    static map::EntityPtr read_entity(Reader& reader, int& max_id,
                                      std::unordered_set<int>& ids) {
        const std::uint8_t tag = reader.u8();

        switch (tag) {
        case kTagPlayer: {
            map::entities::Player player(0, 0);
            read_entity_base(reader, player, max_id, ids);
            const Stats            stats = read_stats(reader);
            std::vector<Item>      inventory = reader.sequence<Item>(
                [&] { return read_item(reader, max_id, ids); });
            player.restore(stats, std::move(inventory));
            return std::make_shared<map::entities::Player>(std::move(player));
        }
        case kTagBoss: {
            const std::size_t tier  = reader.size();
            const bool        final = reader.boolean();

            map::entities::Boss boss(0, 0, tier, final);
            read_entity_base(reader, boss, max_id, ids);
            read_enemy_fields(reader, boss);
            boss._anchors_remaining = reader.size();
            boss._fireball_elapsed  = reader.f64();
            boss._volley_rotation   = reader.size();
            boss._volleys_fired     = reader.size();
            boss._attack            = read_enum<map::entities::BossAttack>(
                reader, static_cast<std::uint8_t>(map::entities::BossAttack::Aimed));
            return std::make_shared<map::entities::Boss>(std::move(boss));
        }
        case kTagEnemy: {
            const auto kind = read_enum<map::entities::EnemyKind>(
                reader, static_cast<std::uint8_t>(map::entities::EnemyKind::Boss));

            map::entities::Enemy enemy(kind, 0, 0);
            read_entity_base(reader, enemy, max_id, ids);
            read_enemy_fields(reader, enemy);
            return std::make_shared<map::entities::Enemy>(std::move(enemy));
        }
        case kTagAnchor: {
            map::entities::Anchor anchor(0, 0, 1);
            read_entity_base(reader, anchor, max_id, ids);
            anchor._hits = reader.i32();
            return std::make_shared<map::entities::Anchor>(std::move(anchor));
        }
        case kTagFireball: {
            map::entities::Fireball fireball(0, 0, 0, 0, 0, 0);
            read_entity_base(reader, fireball, max_id, ids);
            fireball._dx            = reader.i32();
            fireball._dy            = reader.i32();
            fireball._damage        = reader.i32();
            fireball._range         = reader.size();
            fireball._step_interval = reader.f64();
            fireball._elapsed       = reader.f64();
            return std::make_shared<map::entities::Fireball>(std::move(fireball));
        }
        case kTagShopkeeper: {
            auto shopkeeper = std::make_shared<map::entities::Shopkeeper>();
            read_entity_base(reader, *shopkeeper, max_id, ids);
            return shopkeeper;
        }
        case kTagStand: {
            auto stand = std::make_shared<map::entities::StoreStand>();
            read_entity_base(reader, *stand, max_id, ids);
            return stand;
        }
        case kTagChest: {
            auto chest = std::make_shared<map::entities::Chest>(0, 0);
            read_entity_base(reader, *chest, max_id, ids);
            read_item_fields(reader, *chest);

            auto contents = reader.sequence<Item>([&] { return read_item(reader, max_id, ids); });
            for (Item& item : contents) {
                chest->add(std::move(item));
            }
            return chest;
        }
        case kTagItem:
            return read_item_ptr(reader, max_id, ids);
        case kTagCoin:
            return read_typed_item<map::entities::Coin>(reader, max_id, ids);
        case kTagCoinDouble:
            return read_typed_item<map::entities::CoinDouble>(reader, max_id, ids);
        case kTagDiamond:
            return read_typed_item<map::entities::Diamond>(reader, max_id, ids);
        case kTagPotion:
            return read_typed_item<map::entities::HealthPotion>(reader, max_id, ids);
        case kTagKey:
            return read_typed_item<map::entities::Key>(reader, max_id, ids);
        case kTagSword:
            return read_typed_item<map::entities::Sword>(reader, max_id, ids);
        case kTagGreatsword:
            return read_typed_item<map::entities::Greatsword>(reader, max_id, ids);
        case kTagShield:
            return read_typed_item<map::entities::Shield>(reader, max_id, ids);
        case kTagChainmail:
            return read_typed_item<map::entities::Chainmail>(reader, max_id, ids);
        case kTagBoots:
            return read_typed_item<map::entities::Boots>(reader, max_id, ids);
        default:
            throw FormatError("Save file contains an unknown entity tag");
        }
    }

    static void write_map(Writer& writer, const map::Map& map) {
        writer.u64(static_cast<std::uint64_t>(map._width));
        writer.u64(static_cast<std::uint64_t>(map._height));
        write_point(writer, map._entrance);
        write_point(writer, map._exit);
        writer.boolean(map._has_exit);
        writer.boolean(map._exit_open);
        write_chance(writer, map._chance);

        writer.u64(static_cast<std::uint64_t>(map._tiles.size()));
        for (const map::Tile tile : map._tiles) {
            writer.u8(static_cast<std::uint8_t>(tile));
        }

        writer.sequence(map._entities,
                        [&](const map::EntityPtr& entity) { write_entity(writer, *entity); });
    }

    static void read_map(Reader& reader, map::Map& map, int& max_id,
                         std::unordered_set<int>& ids) {
        const std::uint64_t width  = reader.u64();
        const std::uint64_t height = reader.u64();

        if (width > detail::kMaxMapCells || height > detail::kMaxMapCells ||
            (width != 0 && height > detail::kMaxMapCells / width)) {
            throw FormatError("Save file declares an oversized map");
        }
        const std::uint64_t cells = width * height;

        map.clear();
        map._width     = static_cast<std::size_t>(width);
        map._height    = static_cast<std::size_t>(height);
        map._entrance  = read_point(reader);
        map._exit      = read_point(reader);
        map._has_exit  = reader.boolean();
        map._exit_open = reader.boolean();
        read_chance(reader, map._chance);

        const std::uint64_t tile_count = reader.u64();
        if (tile_count != cells) {
            throw FormatError("Save file map tile count does not match its dimensions");
        }

        map._tiles.clear();
        map._tiles.reserve(static_cast<std::size_t>(cells));
        for (std::uint64_t i = 0; i < cells; ++i) {
            map._tiles.push_back(detail::read_enum<map::Tile>(
                reader, static_cast<std::uint8_t>(map::Tile::Door)));
        }

        auto entities = reader.sequence<map::EntityPtr>(
            [&] { return read_entity(reader, max_id, ids); });
        for (auto& entity : entities) {
            static_cast<void>(map.add(std::move(entity)));
        }
    }

    static void write_run_stats(Writer& writer, const game::RunStats& stats) {
        writer.u64(static_cast<std::uint64_t>(stats.dungeons_completed));
        writer.u64(static_cast<std::uint64_t>(stats.enemies_killed));
        writer.u64(static_cast<std::uint64_t>(stats.potions_used));
        writer.i32(stats.gold_collected);
        writer.u64(static_cast<std::uint64_t>(stats.keys_collected));
        writer.u64(static_cast<std::uint64_t>(stats.bosses_defeated));
    }

    static game::RunStats read_run_stats(Reader& reader) {
        return game::RunStats{reader.size(), reader.size(), reader.size(), reader.i32(),
                              reader.size(), reader.size()};
    }

    static void write_game(Writer& writer, const game::Game& game) {
        writer.u64(static_cast<std::uint64_t>(game._depth));
        writer.u64(static_cast<std::uint64_t>(game._store_visits));
        writer.u64(static_cast<std::uint64_t>(game._chest_selection));
        writer.boolean(game._store_level);
        writer.boolean(game._store_open);
        writer.boolean(game._boss_level);
        writer.boolean(game._victory);
        writer.boolean(game._game_over);
        writer.f64(game._giggle_elapsed);
        writer.f64(game._giggle_hold);

        write_chance(writer, game._chance);
        write_run_stats(writer, game._run_stats);

        writer.sequence(game._messages, [&](const std::string& message) { writer.text(message); });

        writer.sequence(game._store_items, [&](const game::StoreItem& item) {
            write_enum(writer, item.kind);
            writer.text(item.name);
            writer.text(item.description);
            writer.i32(item.price);
        });

        writer.sequence(game._store_sold, [&](bool sold) { writer.boolean(sold); });

        write_map(writer, game._map);
    }

    static void read_game(Reader& reader, game::Game& game) {
        game._depth           = reader.size();
        game._store_visits    = reader.size();
        game._chest_selection = reader.size();
        game._store_level     = reader.boolean();
        game._store_open      = reader.boolean();
        game._boss_level      = reader.boolean();
        game._victory         = reader.boolean();
        game._game_over       = reader.boolean();
        game._giggle_elapsed  = reader.f64();
        game._giggle_hold     = reader.f64();

        read_chance(reader, game._chance);
        game._run_stats = read_run_stats(reader);

        game._messages = reader.sequence<std::string>([&] { return reader.text(); });

        game._store_items = reader.sequence<game::StoreItem>([&] {
            return game::StoreItem{
                read_enum<map::entities::ItemKind>(
                    reader, static_cast<std::uint8_t>(map::entities::ItemKind::Boots)),
                reader.text(), reader.text(), reader.i32()};
        });

        game._store_sold = reader.sequence<bool>([&] { return reader.boolean(); });

        int                   max_id = 0;
        std::unordered_set<int> ids;
        read_map(reader, game._map, max_id, ids);
        map::entities::Entity::restore_id_counter(max_id + 1);
    }
};

SaveResult save_to_file(const game::Game& game, const std::string& path) {
    try {
        Writer payload;
        BinaryCodec::write(payload, game);

        Writer file;
        file.raw(detail::kMagic.data(), detail::kMagic.size());
        file.u32(detail::kVersion);
        file.u32(static_cast<std::uint32_t>(detail::kHeaderSize));
        file.u64(static_cast<std::uint64_t>(payload.bytes().size()));
        file.u64(detail::checksum(payload.bytes()));
        file.u64(0); // Reserved for future flags.
        file.raw(payload.bytes().data(), payload.bytes().size());

        return detail::write_atomic(path, file.bytes());
    } catch (const std::exception& error) {
        return SaveResult::failure(error.what());
    }
}

SaveResult load_from_file(game::Game& game, const std::string& path) {
    try {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            return SaveResult::failure("No saved game was found.");
        }

        const std::streampos end = input.tellg();
        if (end < 0) {
            return SaveResult::failure("Could not read the save file.");
        }

        const auto file_size = static_cast<std::uint64_t>(end);
        if (file_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            return SaveResult::failure("The save file is too large.");
        }

        std::vector<std::uint8_t> file(static_cast<std::size_t>(file_size));
        input.seekg(0);
        if (file_size > 0) {
            input.read(reinterpret_cast<char*>(file.data()), static_cast<std::streamsize>(file_size));
            if (!input) {
                return SaveResult::failure("Could not read the save file.");
            }
        }

        Reader header(file);
        const auto magic = header.bytes(detail::kMagic.size());
        if (!std::equal(magic.begin(), magic.end(), detail::kMagic.begin())) {
            return SaveResult::failure("The file is not a dungeon save.");
        }

        const std::uint32_t version = header.u32();
        if (version == 0 || version > detail::kVersion) {
            return SaveResult::failure("This save was made by an unsupported version of dungeon.");
        }

        if (header.u32() != detail::kHeaderSize) {
            return SaveResult::failure("The save file has an invalid header.");
        }

        const std::uint64_t payload_size = header.u64();
        const std::uint64_t expected     = header.u64();
        header.u64(); // Reserved for future flags.

        if (payload_size > file_size - static_cast<std::uint64_t>(detail::kHeaderSize)) {
            return SaveResult::failure("The save file is truncated.");
        }

        const auto payload_begin = file.begin() + static_cast<std::ptrdiff_t>(detail::kHeaderSize);
        const auto payload_end   = payload_begin + static_cast<std::ptrdiff_t>(payload_size);
        const std::vector<std::uint8_t> payload(payload_begin, payload_end);

        if (detail::checksum(payload) != expected) {
            return SaveResult::failure("The save file is corrupt.");
        }

        Reader reader(payload);
        BinaryCodec::load(reader, game);
        return SaveResult::success("Game loaded.");
    } catch (const std::exception& error) {
        return SaveResult::failure(error.what());
    }
}

bool save_file_exists(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return input.good();
}

} // namespace dungeon::save
