# test_serialize needs a loaded difficulty + base_gamestate, and keeps its
# pre-existing ctest name ("test_serialize_difficulty0") so this refactor
# changes no test's identity.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
set(AOT_AUTO_TEST_NAME test_serialize_difficulty0)
