#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "better_terrain_solver.hpp"

using namespace godot;

void initialize_better_terrain_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
  ClassDB::register_class<BetterTerrainSolver>();
}

void uninitialize_better_terrain_module(ModuleInitializationLevel p_level) {}

extern "C" {
GDExtensionBool GDE_EXPORT better_terrain_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization) {
  GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library,
                                          r_initialization);
  init_obj.register_initializer(initialize_better_terrain_module);
  init_obj.register_terminator(uninitialize_better_terrain_module);
  init_obj.set_minimum_library_initialization_level(
      MODULE_INITIALIZATION_LEVEL_SCENE);
  return init_obj.init();
}
}
