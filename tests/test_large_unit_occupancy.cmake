# test_large_unit_occupancy needs a loaded difficulty + base_gamestate (same as
# test_serialize / test_lab_assignment / test_battle_large_unit): it builds a real Battle via
# Battle::beginBattle()/enterBattle(), which needs a populated ruleset and gamestate to draw
# agent types, vehicle types and battle maps from.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
