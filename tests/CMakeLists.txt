PROJECT (OpenApoc_Tests CXX C)

# add_openapoc_test(<src-name> [ARGS <args...>] [TEST_NAME <ctest-name>])
#
# Builds tests/<src-name>.cpp into an executable and registers it with ctest.
# ARGS are passed to the binary before the --Framework.* flags; use this for
# tests that need a difficulty file + a generated base_gamestate. TEST_NAME
# overrides the name ctest lists the test under (defaults to <src-name>) -
# only needed to keep a pre-existing ctest name stable.
function(add_openapoc_test SRC_NAME)
	cmake_parse_arguments(AOT "" "TEST_NAME" "ARGS" ${ARGN})
	if(NOT AOT_TEST_NAME)
		set(AOT_TEST_NAME ${SRC_NAME})
	endif()
	add_executable(${SRC_NAME} ${SRC_NAME}.cpp)
	target_link_libraries(${SRC_NAME} OpenApoc_Library OpenApoc_Framework
			OpenApoc_GameState)
	target_compile_definitions(${SRC_NAME} PRIVATE -DUNIT_TEST)
	add_test(NAME ${AOT_TEST_NAME} COMMAND ${SRC_NAME} ${AOT_ARGS}
			--Framework.CD=${CD_PATH} --Framework.Data=${CMAKE_SOURCE_DIR}/data)
endfunction()

# Every tests/test_*.cpp is discovered and registered automatically, so a
# future mini adding a plain test needs to touch nothing in this file at all -
# just add the .cpp.
#
# A test that needs extra COMMAND args (e.g. a loaded difficulty + a
# generated base_gamestate) or a non-default ctest name ships its own
# tests/<src-name>.cmake sidecar next to the .cpp. That sidecar sets
# AOT_AUTO_ARGS and/or AOT_AUTO_TEST_NAME before add_openapoc_test() is
# called for it, so the opt-in lives entirely in files owned by that one
# test and this shared CMakeLists.txt is never touched again.
file(GLOB OPENAPOC_TEST_SOURCES CONFIGURE_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
foreach(OPENAPOC_TEST_SRC ${OPENAPOC_TEST_SOURCES})
	get_filename_component(OPENAPOC_TEST_SRC_NAME ${OPENAPOC_TEST_SRC} NAME_WE)

	unset(AOT_AUTO_ARGS)
	unset(AOT_AUTO_TEST_NAME)
	set(OPENAPOC_TEST_SIDECAR
			${CMAKE_CURRENT_SOURCE_DIR}/${OPENAPOC_TEST_SRC_NAME}.cmake)
	if(EXISTS ${OPENAPOC_TEST_SIDECAR})
		include(${OPENAPOC_TEST_SIDECAR})
	endif()

	set(OPENAPOC_TEST_EXTRA_KWARGS)
	if(DEFINED AOT_AUTO_ARGS)
		list(APPEND OPENAPOC_TEST_EXTRA_KWARGS ARGS ${AOT_AUTO_ARGS})
	endif()
	if(DEFINED AOT_AUTO_TEST_NAME)
		list(APPEND OPENAPOC_TEST_EXTRA_KWARGS TEST_NAME ${AOT_AUTO_TEST_NAME})
	endif()

	add_openapoc_test(${OPENAPOC_TEST_SRC_NAME} ${OPENAPOC_TEST_EXTRA_KWARGS})
endforeach()
