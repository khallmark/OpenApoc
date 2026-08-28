# test_research needs a loaded difficulty + base_gamestate (same as
# test_serialize / test_lab_assignment) so it can exercise ItemDependency
# against a real player base and real agent/vehicle equipment types.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
