#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/rules/city/ufoincursion.h"

using namespace OpenApoc;

// Feature-owned coverage for the UFO incursion "table/selection" half of OPE-14 (Restore UFO
// incursion selection and spawn behavior). This intentionally does NOT live in the monolithic
// test_city_rules.cpp -- see the ticket's acceptance criteria.
//
// Scope note: this only locks the header-only data added by this change (the withdrawal-percent
// table and the UFOIncursionSlot record). The runtime consumer of these symbols
// (game/state/gamestate.cpp's spawn/withdrawal-gate logic, the serialization schema in
// game/state/gamestate_serialize.xml, and the extractor in
// tools/extractors/extract_ufo_incursions.cpp) lands in separate PRs; at this base these symbols
// have no caller at all, so this test is the only thing exercising them.
//
// Evidence for the expected values below: docs/original-game/findings/U1b-gate-consumer.md
// (currently only on origin/develop, not part of this diff), section 4.4, lines 391-397 for the
// role->percent table, and the surrounding paragraphs for the roles-9/11 empty-band claim. That
// document is itself explicit that two points remain open ("strong structural lean, not a closed
// question" for role vs. type_percent divergence, and IDIV rounding at band boundaries not
// re-verified) -- this test locks the *values as recovered*, it does not independently
// re-derive them from the binary.

static_assert(sizeof(UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE) /
                      sizeof(UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[0]) ==
                  16,
              "UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE must have exactly 16 entries (role in [0,15])");

// The full 16-entry table, byte-for-byte as recovered (role -> withdrawal percent).
static bool test_withdraw_percent_table_matches_recovered_values()
{
	static const int expected[16] = {75, 50, 25, 15, 33, 30, 10, 30, 20, 10, 25, 10, 10, 10, 10, 0};
	for (int role = 0; role < 16; role++)
	{
		if (UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[role] != expected[role])
		{
			LogError("UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[{0}] == {1}, expected {2}", role,
			         UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[role], expected[role]);
			return false;
		}
	}
	return true;
}

// Named invariant: roles 9 (unnamed) and 11 (Escort) sit at 10%, which the recovered findings
// document proves is below crash_health on every one of the ten UFO hulls -- so the
// damaged-withdrawal band is structurally empty for these two roles and the gate can never fire
// for them. This is recovered behaviour, not a bug to round away; a regression that "fixes" these
// roles up to a non-trivial percentage must fail this check.
static bool test_roles_9_and_11_have_empty_withdraw_band()
{
	if (UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[9] != 10)
	{
		LogError("role 9 (unnamed) withdraw percent is {0}, expected 10 (empty band on every "
		         "UFO hull)",
		         UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[9]);
		return false;
	}
	if (UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[11] != 10)
	{
		LogError("role 11 (Escort) withdraw percent is {0}, expected 10 (empty band on every "
		         "UFO hull)",
		         UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[11]);
		return false;
	}
	return true;
}

// Named invariant: role 15 never withdraws.
static bool test_role_15_never_withdraws()
{
	if (UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[15] != 0)
	{
		LogError("role 15 withdraw percent is {0}, expected 0 (never withdraws)",
		         UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[15]);
		return false;
	}
	return true;
}

// Named invariant: every role this incursion population can actually receive (5=Attack,
// 7=Infiltration, 8=Subversion, 9=unnamed, 10=Overspawn, 11=Escort) reads well below 75%. This is
// the corrected finding -- an earlier pass assumed a flat 75% withdrawal threshold for all roles,
// which only held at a call site this population structurally never reaches. A regression that
// reintroduces the flat-75% assumption for any of these roles must fail this check.
static bool test_population_roles_are_below_flat_75_percent_assumption()
{
	static const int populationRoles[6] = {5, 7, 8, 9, 10, 11};
	for (int role : populationRoles)
	{
		if (UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[role] >= 75)
		{
			LogError("role {0} withdraw percent is {1}, expected < 75 (flat-75% assumption does "
			         "not hold for this population)",
			         role, UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[role]);
			return false;
		}
	}
	return true;
}

// UFOIncursionSlot must default-construct to all-zero / empty fields.
static bool test_slot_defaults_to_zero()
{
	UFOIncursionSlot slot;
	if (!slot.followVehicleType.empty())
	{
		LogError("UFOIncursionSlot::followVehicleType should default to empty, got \"{0}\"",
		         slot.followVehicleType);
		return false;
	}
	if (slot.zoneMode != 0 || slot.missionCounter != 0 || slot.scatter != 0 ||
	    slot.typePercent != 0 || slot.role != 0)
	{
		LogError("UFOIncursionSlot should default-construct all-zero, got zoneMode={0} "
		         "missionCounter={1} scatter={2} typePercent={3} role={4}",
		         slot.zoneMode, slot.missionCounter, slot.scatter, slot.typePercent, slot.role);
		return false;
	}
	return true;
}

// UFOIncursion's new parallel slot vectors must default-construct empty, matching the
// pre-existing primaryList/escortList/attackList lists they sit alongside.
static bool test_incursion_slot_vectors_default_empty()
{
	UFOIncursion incursion;
	if (!incursion.primarySlots.empty() || !incursion.escortSlots.empty() ||
	    !incursion.attackSlots.empty())
	{
		LogError("UFOIncursion::{{primary,escort,attack}}Slots should default-construct empty, "
		         "got sizes {0}/{1}/{2}",
		         (unsigned)incursion.primarySlots.size(), (unsigned)incursion.escortSlots.size(),
		         (unsigned)incursion.attackSlots.size());
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	if (!test_withdraw_percent_table_matches_recovered_values())
	{
		return EXIT_FAILURE;
	}
	if (!test_roles_9_and_11_have_empty_withdraw_band())
	{
		return EXIT_FAILURE;
	}
	if (!test_role_15_never_withdraws())
	{
		return EXIT_FAILURE;
	}
	if (!test_population_roles_are_below_flat_75_percent_assumption())
	{
		return EXIT_FAILURE;
	}
	if (!test_slot_defaults_to_zero())
	{
		return EXIT_FAILURE;
	}
	if (!test_incursion_slot_vectors_default_empty())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
