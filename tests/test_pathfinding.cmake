# test_pathfinding needs a loaded difficulty + base_gamestate (same as
# test_serialize/test_lab_assignment) so it can pull a real AgentType and a
# real Teleporter AEquipmentType out of the loaded ruleset.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
