// Tests for BattleItem::applyFireHazard() (game/state/battle/battleitem.{h,cpp}).
//
// applyFireHazard() ported from the fork's `develop` branch to unblock the excluded
// fire-overlay item-effect path from the battlehazard extraction (see battlehazard.cpp's
// applyOriginalFireItemEffect(), which calls item->applyFireHazard(state, effectPower)).
//
// Scope note, mirroring the precedent set by the sibling battlehazard test: the arithmetic
// and guard-clause behavior below is exercised with a hand-built GameState/AEquipment/
// BattleItem fixture -- no Battle, TileMap, or CD-extracted gamestate save is loaded. The
// destroy branch (item->armor dropping below 1 -> die(state)) is deliberately NOT exercised
// here: BattleItem::die() unconditionally dereferences this->tileObject/this->shadowObject
// and state.current_battle, none of which exist on a bare fixture, and standing up a real
// Battle + TileMap is out of scope for this single-translation-unit slice. Every case below
// is chosen so the post-call armor stays >= 1 and die() is never reached; if a mutation were
// to make it reached anyway, the resulting null-pointer dereference crashes the test binary,
// which ctest still reports as a failure.
//
// The AEquipmentType::fireHazardDamage()/hazardResist formula itself (factor/resist math) is
// covered by the aequipmenttype PR's own test (test_aequipment_fire_hazard.cpp); the cases
// here focus on this unit's specific responsibility -- wiring powerByte through to
// item->type->fireHazardDamage(), applying the signed delta to item->armor, and the null-item
// / null-type / zero-delta guard clauses -- not on re-deriving the formula's constants.

#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battleitem.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/shared/aequipment.h"
#include "library/sp.h"
#include "library/strings.h"
#include <string>

using namespace OpenApoc;

namespace
{
int failureCount = 0;

void check(bool condition, const std::string &description)
{
	if (!condition)
	{
		failureCount++;
		LogError("FAILED: {0}", description);
	}
}

// Registers a minimal AEquipmentType in `state` and returns a StateRef to it. Not resolved
// until first use, so no real gamestate/save is needed -- StateObject<AEquipmentType>::get()
// just looks the id up in state.agent_equipment.
StateRef<AEquipmentType> makeEquipmentType(GameState &state, const UString &id, int hazardResist)
{
	auto type = mksp<AEquipmentType>();
	type->hazardResist = hazardResist;
	state.agent_equipment[id] = type;
	return {&state, id};
}

sp<BattleItem> makeBattleItem(StateRef<AEquipmentType> type, int armor)
{
	auto item = mksp<AEquipment>();
	item->type = type;
	item->armor = armor;

	auto battleItem = mksp<BattleItem>();
	battleItem->item = item;
	return battleItem;
}

void test_null_item_is_noop()
{
	GameState state;
	auto battleItem = mksp<BattleItem>();
	// item left default-constructed (null sp<AEquipment>)
	battleItem->applyFireHazard(state, 21);
	check(battleItem->item == nullptr, "applyFireHazard on a BattleItem with no item is a no-op");
}

void test_unresolved_type_is_noop()
{
	GameState state;
	auto item = mksp<AEquipment>();
	// item->type left default-constructed: empty id, so StateRef resolves to null
	item->armor = 50;
	auto battleItem = mksp<BattleItem>();
	battleItem->item = item;

	battleItem->applyFireHazard(state, 21);
	check(item->armor == 50, "applyFireHazard with an unresolved item->type leaves armor untouched");
}

void test_zero_power_byte_is_noop()
{
	// factor = (max(0, 0) + 19) / 20 == 0 (integer division) -> delta == 0 regardless of
	// resist, and the function returns before touching armor at all.
	GameState state;
	auto type = makeEquipmentType(state, "AEQUIPMENTTYPE_zero_power", 0);
	auto battleItem = makeBattleItem(type, 50);

	battleItem->applyFireHazard(state, 0);
	check(battleItem->item->armor == 50, "powerByte 0 leaves armor untouched (delta computes to 0)");
}

void test_positive_delta_decrements_armor()
{
	// powerByte 21 -> factor = (21 + 19) / 20 == 2. resist 0 -> delta == 2.
	GameState state;
	auto type = makeEquipmentType(state, "AEQUIPMENTTYPE_no_resist", 0);
	auto battleItem = makeBattleItem(type, 50);

	battleItem->applyFireHazard(state, 21);
	check(battleItem->item->armor == 48,
	      "powerByte 21 with hazardResist 0 decrements armor by the expected delta (2)");
}

void test_high_resist_can_produce_negative_delta_and_repair_armor()
{
	// Same factor as above (2), but hazardResist 200 (the documented max) makes
	// delta = factor - (resist*factor)/100 = 2 - (200*2)/100 = 2 - 4 = -2, i.e. negative.
	// applyFireHazard applies this unconditionally as `armor -= delta`, so a negative delta
	// increases armor. This is a direct, faithful consequence of the ported formula (not an
	// invented behavior) -- documented here so a future reader isn't surprised by it.
	GameState state;
	auto type = makeEquipmentType(state, "AEQUIPMENTTYPE_high_resist", 200);
	auto battleItem = makeBattleItem(type, 50);

	battleItem->applyFireHazard(state, 21);
	check(battleItem->item->armor == 52,
	      "hazardResist 200 yields a negative delta, which increases armor rather than "
	      "decreasing it");
}

void test_armor_stays_above_destroy_threshold_without_crashing()
{
	// Chosen so armor lands well clear of the < 1 destroy threshold, keeping this fixture
	// (no Battle/TileMap) safe to run -- see the file-level scope note.
	GameState state;
	auto type = makeEquipmentType(state, "AEQUIPMENTTYPE_partial_damage", 0);
	auto battleItem = makeBattleItem(type, 10);

	battleItem->applyFireHazard(state, 21); // delta 2
	check(battleItem->item->armor == 8, "a partial hit leaves armor positive and does not crash");
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	test_null_item_is_noop();
	test_unresolved_type_is_noop();
	test_zero_power_byte_is_noop();
	test_positive_delta_decrements_armor();
	test_high_resist_can_produce_negative_delta_and_repair_armor();
	test_armor_stays_above_destroy_threshold_without_crashing();

	if (failureCount > 0)
	{
		LogError("{0} check(s) failed", failureCount);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
