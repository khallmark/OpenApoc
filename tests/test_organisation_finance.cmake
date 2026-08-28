# test_organisation_finance needs a loaded difficulty + base_gamestate (same as
# test_serialize and test_lab_assignment) so it has real organisations, buildings,
# vehicle types and economy prices to exercise.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
