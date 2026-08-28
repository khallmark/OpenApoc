# test_unit_ai_priority needs a loaded difficulty + base_gamestate so
# weapon_priority_inputs_are_sane_for_real_weapons can walk the real
# AEquipmentType table (same requirement as test_serialize).
set(AOT_AUTO_ARGS
		${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0
		${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)
