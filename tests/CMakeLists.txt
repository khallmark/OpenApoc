PROJECT (OpenApoc_Tests CXX C)

set (TEST_LIST test_rect test_voxel test_tilemap test_rng test_images
		test_unicode test_colour test_backtrace test_strings
		test_gravlift_lineoffire test_squad_assignment)

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

function(add_openapoc_test NAME)
	cmake_parse_arguments(AOT "" "" "ARGS" ${ARGN})
	add_executable(${NAME} ${NAME}.cpp)
	target_link_libraries(${NAME} OpenApoc_Library OpenApoc_Framework
			OpenApoc_GameState)
	target_compile_definitions(${NAME} PRIVATE -DUNIT_TEST)
	add_test(NAME ${NAME} COMMAND ${NAME} ${AOT_ARGS}
			--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
endfunction()

# The crash necessarily aborts, and ctest counts an aborted subprocess as failed whatever its
# output says -- so this test crashes a CHILD and asserts on what the child printed.
add_openapoc_test(test_crash_handler)

add_openapoc_test(test_alien_behaviour_option)
add_openapoc_test(test_game_event_pause)
add_openapoc_test(test_gametime)
add_openapoc_test(test_vec)
add_openapoc_test(test_line)
add_openapoc_test(test_enum_traits)
add_openapoc_test(test_damage_predicates)
add_openapoc_test(test_tactical_ai_retreat)
add_openapoc_test(test_economy)
add_openapoc_test(test_organisation)
add_openapoc_test(test_diplomacy)
add_openapoc_test(test_research)
add_openapoc_test(test_agent_mission)
add_openapoc_test(test_vehicle_mission)
add_openapoc_test(test_battle_hazard)
add_openapoc_test(test_battle_use_item)
add_openapoc_test(test_game_end)
add_openapoc_test(test_harness)
add_openapoc_test(test_harness_control_click)
target_link_libraries(test_harness_control_click OpenApoc_Forms)
add_openapoc_test(test_app_paths)
add_openapoc_test(test_base_die ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_city_rules ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_battle_large_unit ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_display_size)
add_openapoc_test(test_ground_vehicle_path ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_psionics ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_tu_reservation ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_unit_ai_priority ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_battle_disruptor_shield ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
# test_gravlift_lof and test_gravlift_ai_battle are manual investigation
# harnesses that boot a real battle in one of the base game's grav-lift tilesets
# to exercise the real voxel/AI code paths, which needs the fully extracted
# tileset/map/gamestate data CI's minimal cd.iso does not provide. They are
# deliberately not add_test()-ed; build and run them by hand (see the header
# comment in each .cpp for arguments).
set(TEST test_gravlift_lof)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)

set(TEST test_gravlift_ai_battle)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework OpenApoc_GameState)

# Manual diagnostic harness for issue #647, run by hand against its
# attached savegame; deliberately not add_test'd.

# Standalone harness, run by hand against a savegame; deliberately not add_test'd.
set(TEST test_mission_end_recheck)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)

# Standalone harness for crashed vehicle recovery throughput. Not wired to
# add_test: it is a manually-run before/after comparison, not a regression gate.
set(TEST test_crashed_vehicle_recovery)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)

# test_city_dimension_gate requires args (same as test_serialize)
set(TEST test_city_dimension_gate)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)
add_test(NAME ${TEST} COMMAND ${TEST}
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate
		--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)

# test_org_relations requires args (same as test_serialize)
set(TEST test_org_relations)
add_executable(${TEST} ${TEST}.cpp)
target_link_libraries(${TEST} OpenApoc_Library OpenApoc_Framework
		OpenApoc_GameState)
target_compile_definitions(${TEST} PRIVATE -DUNIT_TEST)
add_test(NAME ${TEST} COMMAND ${TEST}
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate
		--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
