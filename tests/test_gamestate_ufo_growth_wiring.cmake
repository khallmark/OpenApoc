# test_gamestate_ufo_growth_wiring needs a loaded difficulty + base_gamestate (same as
# test_serialize/test_lab_assignment) to exercise GameState::updateUfoGrowth() against a real
# extracted save.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
