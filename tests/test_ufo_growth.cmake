# test_ufo_growth needs a loaded difficulty + base_gamestate (same as
# test_lab_assignment / test_serialize) to exercise UFOGrowth::selectForWeek
# and UFOGrowth::craftFactoryIntact against real extracted city/building data.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
