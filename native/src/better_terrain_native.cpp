#include "better_terrain_native.hpp"

void BetterTerrainNative::_bind_methods() {
  ClassDB::bind_method(D_METHOD("setup", "cache", "terrain_types", "tile_shape"),
                       &BetterTerrainNative::setup);
  ClassDB::bind_method(
      D_METHOD("update_terrain_cells", "tm", "cells", "and_surrounding_cells",
               "use_seed", "rng"),
      &BetterTerrainNative::update_terrain_cells);
  ClassDB::bind_method(D_METHOD("set_cells", "tm", "coords", "source_id",
                                "atlas_coords", "alternative_tile"),
                       &BetterTerrainNative::set_cells);
}

BetterTerrainNative::BetterTerrainNative() {}

BetterTerrainNative::~BetterTerrainNative() {}

bool BetterTerrainNative::setup(const Array& cache,
                                const PackedInt32Array& terrain_types,
                                int tile_shape) {
  return solver.setup(cache, terrain_types, tile_shape);
}

bool BetterTerrainNative::update_terrain_cells(
    TileMapLayer* tm, const Array& cells, bool and_surrounding_cells,
    bool use_seed, const Ref<RandomNumberGenerator>& rng) {
  ERR_FAIL_NULL_V(tm, false);
  ERR_FAIL_COND_V(rng.is_null(), false);
  return solver.update_terrain_cells(tm, cells, and_surrounding_cells,
                                     use_seed, rng);
}

bool BetterTerrainNative::set_cells(TileMapLayer* tm, const Array& coords,
                                    int source_id, const Vector2i& atlas_coords,
                                    int alternative_tile) {
  ERR_FAIL_NULL_V(tm, false);
  for (int i = 0; i < coords.size(); i++) {
    if (coords[i].get_type() != Variant::VECTOR2I) return false;
  }
  for (int i = 0; i < coords.size(); i++) {
    tm->set_cell(coords[i], source_id, atlas_coords, alternative_tile);
  }
  return true;
}
