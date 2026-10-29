#pragma once

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/tile_map_layer.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

using namespace godot;

namespace better_terrain {

// Mirrors BetterTerrain.TerrainType.
enum TerrainType {
  TERRAIN_MATCH_TILES = 0,
  TERRAIN_MATCH_VERTICES = 1,
  TERRAIN_CATEGORY = 2,
  TERRAIN_DECORATION = 3,
};

// Mirrors BetterTerrain.TileCategory.
enum TileCategory {
  CATEGORY_EMPTY = -1,
  CATEGORY_NON_TERRAIN = -2,
};

struct PeeringRule {
  int neighbor;
  std::vector<int> types;

  bool matches(int type) const {
    for (const int t : types) {
      if (t == type) return true;
    }
    return false;
  }
};

// One entry of a BetterTerrain cache list:
// [source_id, atlas_coords, alternative_tile, peering, probability].
struct TileChoice {
  int source_id;
  Vector2i atlas_coords;
  int alternative_tile;
  std::vector<PeeringRule> peerings;
  double probability;
};

class Solver {
 public:
  bool setup(const Array& cache, const PackedInt32Array& terrain_types,
             int tile_shape);

  /**
   * Same as BetterTerrain.update_terrain_cells.
   * @return False without touching the tilemap if a cell isn't a Vector2i.
   */
  bool update_terrain_cells(TileMapLayer* tm, const Array& cells,
                            bool and_surrounding_cells, bool use_seed,
                            const Ref<RandomNumberGenerator>& rng) const;

 private:
  // Indexed like the GDScript cache, the decoration list comes last.
  std::vector<std::vector<TileChoice>> cache;
  std::vector<int> terrain_types;
};

}  // namespace better_terrain
