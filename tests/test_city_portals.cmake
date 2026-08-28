# test_city_portals needs a loaded difficulty + base_gamestate so it can read CITYMAP_HUMAN's
# real, initState()-generated portal positions (same requirement as test_serialize and
# test_lab_assignment).
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
