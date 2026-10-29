#include "solver.hpp"

#include <godot_cpp/classes/tile_data.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace better_terrain {

namespace {

constexpr int NEIGHBOR_COUNT = 16;
constexpr int MISSING_TYPE = INT_MIN;

// TileSet.get_neighbor_cell for square tiles, indexed by TileSet.CellNeighbor.
// Non-square neighbors resolve to the cell itself, like the engine does.
const Vector2i SQUARE_NEIGHBOR_OFFSETS[NEIGHBOR_COUNT] = {
    Vector2i(1, 0),   Vector2i(0, 0), Vector2i(0, 0),  Vector2i(1, 1),
    Vector2i(0, 1),   Vector2i(0, 0), Vector2i(0, 0),  Vector2i(-1, 1),
    Vector2i(-1, 0),  Vector2i(0, 0), Vector2i(0, 0),  Vector2i(-1, -1),
    Vector2i(0, -1),  Vector2i(0, 0), Vector2i(0, 0),  Vector2i(1, -1),
};

// BetterTerrainData._terrain_peering_square_tiles, the order matters for the
// order in which cells are solved.
const int SQUARE_WIDEN_PEERINGS[] = {0, 3, 4, 7, 8, 11, 12, 15};

uint64_t pack_coord(const Vector2i& c) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32) |
         static_cast<uint32_t>(c.y);
}

Vector2i to_vector2i(const Variant& v) {
  if (v.get_type() == Variant::VECTOR2) {
    return Vector2i(static_cast<Vector2>(v));
  }
  return v;
}

// Same as BetterTerrain._widen: insertion ordered and unique.
std::vector<Vector2i> widen(const std::vector<Vector2i>& coords) {
  std::vector<Vector2i> result;
  result.reserve(coords.size() * 2);
  std::unordered_set<uint64_t> seen;
  seen.reserve(coords.size() * 2);

  for (const Vector2i& c : coords) {
    if (seen.insert(pack_coord(c)).second) result.push_back(c);
    for (const int peering : SQUARE_WIDEN_PEERINGS) {
      const Vector2i n = c + SQUARE_NEIGHBOR_OFFSETS[peering];
      if (seen.insert(pack_coord(n)).second) result.push_back(n);
    }
  }
  return result;
}

// Terrain types per cell, dense over the bounding box unless it's too sparse.
class CellTypes {
 public:
  explicit CellTypes(const std::vector<Vector2i>& cells) {
    if (cells.empty()) return;
    Vector2i min_c = cells[0];
    Vector2i max_c = cells[0];
    for (const Vector2i& c : cells) {
      min_c = Vector2i(MIN(min_c.x, c.x), MIN(min_c.y, c.y));
      max_c = Vector2i(MAX(max_c.x, c.x), MAX(max_c.y, c.y));
    }
    const int64_t w = static_cast<int64_t>(max_c.x) - min_c.x + 1;
    const int64_t h = static_cast<int64_t>(max_c.y) - min_c.y + 1;
    const int64_t area = w * h;
    if (area <= MAX(static_cast<int64_t>(1) << 16,
                    static_cast<int64_t>(cells.size()) * 8)) {
      origin = min_c;
      width = static_cast<int>(w);
      height = static_cast<int>(h);
      dense.assign(static_cast<size_t>(area), MISSING_TYPE);
    } else {
      sparse.reserve(cells.size());
    }
  }

  void set(const Vector2i& c, int type) {
    if (!dense.empty()) {
      dense[index(c)] = type;
      return;
    }
    sparse[pack_coord(c)] = type;
  }

  int get(const Vector2i& c) const {
    if (!dense.empty()) {
      const int x = c.x - origin.x;
      const int y = c.y - origin.y;
      if (x < 0 || y < 0 || x >= width || y >= height) return MISSING_TYPE;
      return dense[index(c)];
    }
    const auto it = sparse.find(pack_coord(c));
    return it == sparse.end() ? MISSING_TYPE : it->second;
  }

 private:
  size_t index(const Vector2i& c) const {
    return static_cast<size_t>(c.y - origin.y) * width + (c.x - origin.x);
  }

  Vector2i origin;
  int width = 0;
  int height = 0;
  std::vector<int> dense;
  std::unordered_map<uint64_t, int> sparse;
};

struct TileKey {
  int source_id;
  Vector2i atlas_coords;
  int alternative_tile;

  bool operator==(const TileKey& o) const {
    return source_id == o.source_id && atlas_coords == o.atlas_coords &&
           alternative_tile == o.alternative_tile;
  }
};

struct TileKeyHash {
  size_t operator()(const TileKey& k) const {
    uint64_t h = static_cast<uint32_t>(k.source_id);
    h = h * 1000003u ^ pack_coord(k.atlas_coords);
    h = h * 1000003u ^ static_cast<uint32_t>(k.alternative_tile);
    return static_cast<size_t>(h);
  }
};

// Same as BetterTerrain.get_cell, memoizing the tile meta lookup per tile.
class CellReader {
 public:
  explicit CellReader(TileMapLayer* p_tm) : tm(p_tm) {}

  int read(const Vector2i& c) {
    const int source_id = tm->get_cell_source_id(c);
    if (source_id == -1) return CATEGORY_EMPTY;

    const TileKey key{source_id, tm->get_cell_atlas_coords(c),
                      tm->get_cell_alternative_tile(c)};
    const auto it = tile_types.find(key);
    if (it != tile_types.end()) return it->second;

    int type = CATEGORY_NON_TERRAIN;
    TileData* td = tm->get_cell_tile_data(c);
    if (td != nullptr && td->has_meta(terrain_meta)) {
      const Dictionary meta = td->get_meta(terrain_meta);
      type = meta.get("type", CATEGORY_NON_TERRAIN);
    }
    tile_types.emplace(key, type);
    return type;
  }

 private:
  TileMapLayer* tm;
  const StringName terrain_meta = StringName("_better_terrain");
  std::unordered_map<TileKey, int, TileKeyHash> tile_types;
};

struct Placement {
  int source_id;
  Vector2i atlas_coords;
  int alternative_tile;
};

// Same as BetterTerrain._weighted_selection_seeded.
bool weighted_selection(const std::vector<const TileChoice*>& choices,
                        const Vector2i& coord, bool apply_empty_probability,
                        bool use_seed, RandomNumberGenerator* rng,
                        Placement& r_placement) {
  if (use_seed) {
    rng->set_seed(static_cast<uint64_t>(UtilityFunctions::hash(coord)));
  }

  if (choices.empty()) return false;

  double weight = 0.0;
  for (const TileChoice* c : choices) weight += c->probability;

  const TileChoice* pick_choice = nullptr;
  if (apply_empty_probability && weight < 1.0 &&
      static_cast<double>(rng->randf()) > weight) {
    r_placement = Placement{-1, Vector2i(0, 0), -1};
    return true;
  }

  if (choices.size() == 1) {
    pick_choice = choices[0];
  } else if (weight == 0.0) {
    pick_choice = choices[rng->randi() % choices.size()];
  } else {
    double pick = static_cast<double>(rng->randf()) * weight;
    pick_choice = choices.back();
    for (const TileChoice* c : choices) {
      if (pick < c->probability) {
        pick_choice = c;
        break;
      }
      pick -= c->probability;
    }
  }

  r_placement = Placement{pick_choice->source_id, pick_choice->atlas_coords,
                          pick_choice->alternative_tile};
  return true;
}

}  // namespace

bool Solver::setup(const Array& p_cache, const PackedInt32Array& p_terrain_types,
                   int tile_shape) {
  cache.clear();
  terrain_types.clear();

  if (tile_shape != TileSet::TILE_SHAPE_SQUARE) return false;
  if (p_cache.size() != p_terrain_types.size() + 1) return false;

  for (int i = 0; i < p_terrain_types.size(); i++) {
    // Vertex matching isn't ported.
    if (p_terrain_types[i] == TERRAIN_MATCH_VERTICES) return false;
    terrain_types.push_back(p_terrain_types[i]);
  }

  cache.resize(p_cache.size());
  for (int t = 0; t < p_cache.size(); t++) {
    const Array entries = p_cache[t];
    std::vector<TileChoice>& choices = cache[t];
    choices.reserve(entries.size());

    for (int e = 0; e < entries.size(); e++) {
      const Array entry = entries[e];
      TileChoice choice;
      choice.source_id = entry[0];
      choice.atlas_coords = to_vector2i(entry[1]);
      choice.alternative_tile = entry[2];
      choice.probability = entry[4];

      const Dictionary peering = entry[3];
      const Array keys = peering.keys();
      for (int k = 0; k < keys.size(); k++) {
        const int neighbor = keys[k];
        if (neighbor < 0 || neighbor >= NEIGHBOR_COUNT) return false;

        PeeringRule rule;
        rule.neighbor = neighbor;
        const Array targets = peering[keys[k]];
        for (int i = 0; i < targets.size(); i++) {
          rule.types.push_back(targets[i]);
        }
        choice.peerings.push_back(std::move(rule));
      }
      choices.push_back(std::move(choice));
    }
  }

  return true;
}

bool Solver::update_terrain_cells(TileMapLayer* tm, const Array& p_cells,
                                  bool and_surrounding_cells, bool use_seed,
                                  const Ref<RandomNumberGenerator>& rng) const {
  std::vector<Vector2i> cells;
  cells.reserve(p_cells.size());
  for (int i = 0; i < p_cells.size(); i++) {
    const Variant& v = p_cells[i];
    // Other types would be distinct dictionary keys in GDScript.
    if (v.get_type() != Variant::VECTOR2I) return false;
    cells.push_back(v);
  }

  if (and_surrounding_cells) cells = widen(cells);
  const std::vector<Vector2i> needed_cells = widen(cells);

  CellTypes types(needed_cells);
  CellReader reader(tm);
  for (const Vector2i& c : needed_cells) types.set(c, reader.read(c));

  const int terrain_count = static_cast<int>(terrain_types.size());
  std::vector<const TileChoice*> best;
  int neighbor_types[NEIGHBOR_COUNT];

  for (const Vector2i& coord : cells) {
    const int type = types.get(coord);
    if (type < CATEGORY_EMPTY || type >= terrain_count) continue;

    const int terrain_type = type < 0 ? TERRAIN_DECORATION : terrain_types[type];
    if (terrain_type != TERRAIN_MATCH_TILES &&
        terrain_type != TERRAIN_DECORATION) {
      continue;
    }

    // Same as BetterTerrain._update_tile_tiles.
    const bool apply_empty_probability = terrain_type == TERRAIN_DECORATION;
    constexpr int reward = 3;
    const int penalty = apply_empty_probability ? -2000 : -10;

    for (int n = 0; n < NEIGHBOR_COUNT; n++) {
      neighbor_types[n] = types.get(coord + SQUARE_NEIGHBOR_OFFSETS[n]);
    }

    // GDScript indexes the decoration list with -1.
    const std::vector<TileChoice>& choices =
        cache[type < 0 ? cache.size() - 1 : type];

    int best_score = -1000;
    best.clear();
    for (const TileChoice& t : choices) {
      int score = 0;
      for (const PeeringRule& rule : t.peerings) {
        score += rule.matches(neighbor_types[rule.neighbor]) ? reward : penalty;
      }

      if (score > best_score) {
        best_score = score;
        best.clear();
        best.push_back(&t);
      } else if (score == best_score) {
        best.push_back(&t);
      }
    }

    Placement placement;
    if (weighted_selection(best, coord, apply_empty_probability, use_seed,
                           rng.ptr(), placement)) {
      tm->set_cell(coord, placement.source_id, placement.atlas_coords,
                   placement.alternative_tile);
    }
  }

  return true;
}

}  // namespace better_terrain
