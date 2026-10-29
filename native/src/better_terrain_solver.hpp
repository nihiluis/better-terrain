#ifndef BETTER_TERRAIN_SOLVER_H
#define BETTER_TERRAIN_SOLVER_H

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/tile_map_layer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "better_terrain/solver.hpp"

using namespace godot;

/**
 * Native port of the BetterTerrain solver for one TileSet. Built by
 * BetterTerrain.gd from its GDScript tile cache and dropped whenever that cache
 * is purged. Placements must match BetterTerrain.gd exactly, including RNG use.
 */
class BetterTerrainSolver : public RefCounted {
  GDCLASS(BetterTerrainSolver, RefCounted)

 protected:
  static void _bind_methods();

 public:
  /**
   * @param cache The array built by BetterTerrain._get_cache.
   * @param terrain_types The TerrainType of every terrain in the TileSet meta.
   * @param tile_shape The TileSet.TileShape of the TileSet.
   * @return False if the TileSet needs features the native solver doesn't
   * cover; the caller must use the GDScript solver then.
   */
  bool setup(const Array& cache, const PackedInt32Array& terrain_types,
             int tile_shape);

  /**
   * Same as BetterTerrain.update_terrain_cells.
   * @return False without touching the tilemap if the input isn't supported;
   * the caller must use the GDScript solver then.
   */
  bool update_terrain_cells(TileMapLayer* tm, const Array& cells,
                            bool and_surrounding_cells, bool use_seed,
                            const Ref<RandomNumberGenerator>& rng);

  /**
   * Places one tile on all coords.
   * @return False without touching the tilemap if a coord isn't a Vector2i.
   */
  bool set_cells(TileMapLayer* tm, const Array& coords, int source_id,
                 const Vector2i& atlas_coords, int alternative_tile);

  BetterTerrainSolver();
  ~BetterTerrainSolver();

 private:
  better_terrain::Solver solver;
};

#endif  // BETTER_TERRAIN_SOLVER_H
