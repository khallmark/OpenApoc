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

function(add_openapoc_test NAME)
	cmake_parse_arguments(AOT "" "" "ARGS" ${ARGN})
	add_executable(${NAME} ${NAME}.cpp)
	target_link_libraries(${NAME} OpenApoc_Library OpenApoc_Framework
			OpenApoc_GameState)
	target_compile_definitions(${NAME} PRIVATE -DUNIT_TEST)
	add_test(NAME ${NAME} COMMAND ${NAME} ${AOT_ARGS}
			--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
endfunction()

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
add_openapoc_test(test_app_paths)
add_openapoc_test(test_base_die ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
add_openapoc_test(test_city_rules ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
