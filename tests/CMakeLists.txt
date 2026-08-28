PROJECT (OpenApoc_Tests CXX C)

set (TEST_LIST test_rect test_voxel test_tilemap test_rng test_images
		test_unicode test_colour test_backtrace)

foreach(TEST ${TEST_LIST})
		add_executable(${TEST} ${TEST}.cpp)
		# Just add all libs?
		target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
				OpenApoc_GameState)
		target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)
		add_test(NAME ${TEST} COMMAND ${TEST}
				--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
endforeach()

# test_serialize requires args so needs to be slightly separate
set(TEST test_serialize)
add_executable(${TEST} ${TEST}.cpp)
# Just add all libs?
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)
add_test(NAME ${TEST}_difficulty0 COMMAND ${TEST}
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate
		--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)

# test_lab_assignment requires args (same as test_serialize)
set(TEST test_lab_assignment)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)
add_test(NAME ${TEST} COMMAND ${TEST}
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate
		--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)

# Helper for the extended test suite: every test is a single .cpp using
# tests/test_helpers.h, optionally taking the difficulty0/base_gamestate pair
# as positional args when it needs a fully loaded GameState.
function(add_openapoc_test NAME)
	cmake_parse_arguments(AOT "" "" "ARGS" ${ARGN})
	add_executable(${NAME} ${NAME}.cpp)
	target_link_libraries(${NAME} OpenApoc_Library OpenApoc_Framework
			OpenApoc_GameState)
	target_compile_definitions(${NAME} PRIVATE -DUNIT_TEST)
	add_test(NAME ${NAME} COMMAND ${NAME} ${AOT_ARGS}
			--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
endfunction()

# Pure-logic tests: no extracted game data required.
add_openapoc_test(test_gametime)
add_openapoc_test(test_vec)
add_openapoc_test(test_line)
add_openapoc_test(test_enum_traits)
add_openapoc_test(test_damage_predicates)
add_openapoc_test(test_economy)
add_openapoc_test(test_organisation)
add_openapoc_test(test_diplomacy)
add_openapoc_test(test_research)
add_openapoc_test(test_agent_mission)
add_openapoc_test(test_vehicle_mission)
add_openapoc_test(test_battle_hazard)
add_openapoc_test(test_display_size)

# Tests that need a fully loaded GameState (difficulty0 + base_gamestate).
add_openapoc_test(test_base_die ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_city_rules ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_battle_large_unit ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_ground_vehicle_path ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_psionics ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_tu_reservation ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)

# These three reproduce defects that exist in master today; they are expected to
# fail until the underlying bugs are fixed. Labelled so the regression gate can
# exclude them:  ctest -LE known-master-bug
set_tests_properties(test_base_die test_ground_vehicle_path test_battle_large_unit
		PROPERTIES LABELS known-master-bug)
