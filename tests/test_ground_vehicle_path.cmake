# test_ground_vehicle_path needs a loaded difficulty + base_gamestate (same as
# test_serialize / test_lab_assignment) to exercise VehicleMission::setPathTo and
# advanceMissionCounterOnArrival() against a real city road network and building set.
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
