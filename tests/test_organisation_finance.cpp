// Coverage for OPE-16: organisation vehicle-park finances.
//
// Exercises Organisation::purchase()'s settleMarketPurchase helper (buyer debited, seller
// credited unless they are the same org) and Organisation::updateVehicleAgentPark()'s
// vehicle-park purchase/surplus-sale behaviour, including the insufficient-funds and
// no-surplus boundary cases named in the ticket's acceptance criteria.
//
// This does not exercise Cargo::refund's double-deduction fix - that lives in vehicle.cpp/h
// and is covered where that code is owned.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vammotype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
#include <algorithm>
#include <iostream>
#include <set>

using namespace OpenApoc;

// Count this org's vehicles of the given type that are still alive (mirrors the counting rule
// updateVehicleAgentPark itself uses to decide whether to buy or sell).
static int countOwnedAlive(sp<GameState> state, StateRef<Organisation> org,
                           StateRef<VehicleType> type)
{
	int count = 0;
	for (auto &v : state->vehicles)
	{
		if (v.second->owner == org && v.second->type == type && !v.second->isDead())
		{
			count++;
		}
	}
	return count;
}

// The player's starting base building is assigned in GameState::fillPlayerStartingProperty(),
// which runs after City::initCity() has already snapshotted Organisation::buildings - so the
// player's own buildings cache stays empty even in a fully started game. Go through
// player_bases instead, exactly as the existing lab-assignment test does.
static StateRef<Building> findPlayerBuilding(sp<GameState> state)
{
	for (auto &base : state->player_bases)
	{
		if (base.second->building)
		{
			return base.second->building;
		}
	}
	return {};
}

// updateVehicleAgentPark() gates all of its behaviour on the org actually owning a CITYMAP_HUMAN
// building (see the "found" check at the top of that function). The player's own buildings
// cache is empty (see findPlayerBuilding above), so calling it on the player would silently
// no-op every vehicle-park test below. Use a real NPC org instead, which had its buildings
// cache populated correctly by City::initCity() before any ownership handoff.
static StateRef<Organisation> findOrgWithHumanBuilding(sp<GameState> state)
{
	auto player = state->getPlayer();
	for (auto &orgPair : state->organisations)
	{
		StateRef<Organisation> candidate{state.get(), orgPair.first};
		if (candidate == player)
		{
			continue;
		}
		for (auto &b : candidate->buildings)
		{
			if (b->city.id == "CITYMAP_HUMAN")
			{
				return candidate;
			}
		}
	}
	return {};
}

// Find a vehicle type that isn't registered as a space-liner type for this org in any city -
// the surplus-sale loop is intentionally gated off (!spaceLiner) for those, and a space-liner
// type would make the surplus-sale tests trivially (and misleadingly) no-op. Picked from types
// already in active use by a non-alien vehicle in the loaded city, rather than any
// ruleset-defined VehicleType wholesale - some of those are battle-only placeholders never
// meant to be placed in the city, and this keeps the test on realistic ground.
static StateRef<VehicleType> findNonSpaceLinerVehicleType(sp<GameState> state,
                                                          StateRef<Organisation> org)
{
	std::set<UString> spaceLinerTypes;
	for (auto &cityMissions : org->recurring_missions)
	{
		for (auto &mission : cityMissions.second)
		{
			if (mission.pattern.target == Organisation::MissionPattern::Target::ArriveFromSpace ||
			    mission.pattern.target == Organisation::MissionPattern::Target::DepartToSpace)
			{
				for (auto &t : mission.pattern.allowedTypes)
				{
					spaceLinerTypes.insert(t.id);
				}
			}
		}
	}

	auto aliens = state->getAliens();
	for (auto &v : state->vehicles)
	{
		if (v.second->owner == aliens)
		{
			continue;
		}
		if (spaceLinerTypes.find(v.second->type.id) != spaceLinerTypes.end())
		{
			continue;
		}
		return v.second->type;
	}
	return {};
}

// A market purchase should debit the buyer and credit the seller (a real seller org, not the
// buyer itself) by the same amount, so the transaction nets to zero across the two balances.
static bool test_settle_market_purchase_credits_seller(sp<GameState> state)
{
	LogInfo("Testing settleMarketPurchase credits the seller...");

	auto player = state->getPlayer();
	StateRef<Building> buyerBuilding = findPlayerBuilding(state);
	if (!buyerBuilding)
	{
		LogError("Player has no base building, cannot test purchase");
		return false;
	}

	StateRef<Organisation> manufacturer;
	for (auto &orgPair : state->organisations)
	{
		StateRef<Organisation> candidate{state.get(), orgPair.first};
		if (candidate == player)
		{
			continue;
		}
		bool sharesCity = false;
		for (auto &b : candidate->buildings)
		{
			if (b->city == buyerBuilding->city && b != buyerBuilding && b->isAlive())
			{
				sharesCity = true;
				break;
			}
		}
		if (sharesCity)
		{
			manufacturer = candidate;
			break;
		}
	}
	if (!manufacturer)
	{
		LogWarning("No manufacturer org sharing the player's city was found, skipping "
		           "seller-credit test");
		return true;
	}

	StateRef<VAmmoType> ammo;
	for (auto &pair : state->vehicle_ammo)
	{
		ammo = {state.get(), pair.first};
		break;
	}
	if (!ammo)
	{
		LogWarning("No vehicle ammo type found, skipping seller-credit test");
		return true;
	}

	state->economy[ammo.id].currentPrice = 100;

	int buyerBalanceBefore = player->balance;
	int sellerBalanceBefore = manufacturer->balance;

	manufacturer->purchase(*state, buyerBuilding, ammo, 3);

	int expectedTotal = 3 * 100;
	if (player->balance != buyerBalanceBefore - expectedTotal)
	{
		LogError("Buyer balance should decrease by {0}: was {1}, now {2}", expectedTotal,
		         buyerBalanceBefore, player->balance);
		return false;
	}
	if (manufacturer->balance != sellerBalanceBefore + expectedTotal)
	{
		LogError("Seller balance should increase by {0}: was {1}, now {2}", expectedTotal,
		         sellerBalanceBefore, manufacturer->balance);
		return false;
	}

	LogInfo("settleMarketPurchase seller-credit test passed");
	return true;
}

// Purchasing from one's own building (seller == buyer) must debit exactly once - it must not
// also credit the same org back, which would silently halve the effective cost.
static bool test_settle_market_purchase_self_no_double_dip(sp<GameState> state)
{
	LogInfo("Testing settleMarketPurchase does not credit a self-purchase...");

	auto player = state->getPlayer();
	StateRef<Building> ownBuilding = findPlayerBuilding(state);
	if (!ownBuilding)
	{
		LogError("Player has no base building, cannot test self-purchase");
		return false;
	}

	StateRef<VAmmoType> ammo;
	for (auto &pair : state->vehicle_ammo)
	{
		ammo = {state.get(), pair.first};
		break;
	}
	if (!ammo)
	{
		LogWarning("No vehicle ammo type found, skipping self-purchase test");
		return true;
	}

	state->economy[ammo.id].currentPrice = 50;
	int balanceBefore = player->balance;

	player->purchase(*state, ownBuilding, ammo, 2);

	int expectedTotal = 2 * 50;
	if (player->balance != balanceBefore - expectedTotal)
	{
		LogError("Self-purchase should debit exactly {0} once: was {1}, now {2}", expectedTotal,
		         balanceBefore, player->balance);
		return false;
	}

	LogInfo("Self-purchase no-double-dip test passed");
	return true;
}

// Boundary: balance one short of the price must block the purchase entirely.
static bool test_vehicle_park_insufficient_funds_blocks_purchase(sp<GameState> state,
                                                                 StateRef<Organisation> org,
                                                                 StateRef<VehicleType> type)
{
	LogInfo("Testing vehicle-park purchase blocks below the price boundary...");

	int countBefore = countOwnedAlive(state, org, type);

	state->economy[type.id].currentPrice = 1000;
	org->balance = 999;
	org->vehiclePark[type] = countBefore + 1;

	org->updateVehicleAgentPark(*state);

	if (countOwnedAlive(state, org, type) != countBefore)
	{
		LogError("Insufficient funds should not allow a vehicle purchase");
		return false;
	}
	if (org->balance != 999)
	{
		LogError("Insufficient-funds attempt should not touch balance: expected 999, got {0}",
		         org->balance);
		return false;
	}

	LogInfo("Insufficient-funds boundary test passed");
	return true;
}

// Boundary: balance exactly equal to the price must complete the purchase and leave 0 behind.
static bool test_vehicle_park_exact_funds_completes_purchase(sp<GameState> state,
                                                             StateRef<Organisation> org,
                                                             StateRef<VehicleType> type)
{
	LogInfo("Testing vehicle-park purchase completes at the exact-funds boundary...");

	int countBefore = countOwnedAlive(state, org, type);

	state->economy[type.id].currentPrice = 1000;
	org->balance = 1000;
	org->vehiclePark[type] = countBefore + 1;

	org->updateVehicleAgentPark(*state);

	if (countOwnedAlive(state, org, type) != countBefore + 1)
	{
		LogError("Exact-funds purchase should buy exactly one vehicle");
		return false;
	}
	if (org->balance != 0)
	{
		LogError("Exact-funds purchase should spend the full balance: {0} left", org->balance);
		return false;
	}

	LogInfo("Exact-funds boundary test passed");
	return true;
}

// A vehicle park over its target with an idle, unassigned surplus vehicle available should sell
// exactly one off and credit its current market price.
static bool test_vehicle_park_surplus_sale_credits_balance(sp<GameState> state,
                                                           StateRef<Organisation> org,
                                                           StateRef<VehicleType> type)
{
	LogInfo("Testing vehicle-park surplus sale credits the balance...");

	int countBefore = countOwnedAlive(state, org, type);
	if (countBefore < 1)
	{
		LogError("Expected at least one owned vehicle carried over from the purchase test");
		return false;
	}

	state->economy[type.id].currentPrice = 750;
	org->balance = 0;
	org->vehiclePark[type] = countBefore - 1;

	org->updateVehicleAgentPark(*state);

	if (countOwnedAlive(state, org, type) != countBefore - 1)
	{
		LogError("Surplus vehicle should have been sold off");
		return false;
	}
	if (org->balance != 750)
	{
		LogError("Surplus sale should credit the current price: expected 750, got {0}",
		         org->balance);
		return false;
	}

	LogInfo("Surplus-sale test passed");
	return true;
}

// Boundary: a vehicle park over its target with no idle surplus (every vehicle is busy) must
// not sell anything or touch the balance.
static bool test_vehicle_park_no_surplus_is_noop(sp<GameState> state, StateRef<Organisation> org,
                                                 StateRef<VehicleType> type)
{
	LogInfo("Testing vehicle-park sale is a no-op with no idle surplus...");

	// The prior test may have sold this org's last vehicle of this type - top up cheaply so
	// there is at least one to work with.
	int seedCount = countOwnedAlive(state, org, type);
	if (seedCount < 1)
	{
		state->economy[type.id].currentPrice = 1;
		org->balance = 100;
		org->vehiclePark[type] = 1;
		org->updateVehicleAgentPark(*state);
	}

	// Give every currently-owned, still-alive vehicle of this type a mission so none qualify as
	// idle surplus.
	int missionsAssigned = 0;
	for (auto &v : state->vehicles)
	{
		if (v.second->owner == org && v.second->type == type && !v.second->isDead())
		{
			if (v.second->setMission(*state, VehicleMission::gotoBuilding(*state, *v.second)))
			{
				missionsAssigned++;
			}
		}
	}
	if (missionsAssigned < 1)
	{
		LogError("Could not assign a mission to any owned vehicle, test setup is unsound");
		return false;
	}

	int countBefore = countOwnedAlive(state, org, type);
	int balanceBefore = org->balance;

	state->economy[type.id].currentPrice = 750;
	org->vehiclePark[type] = std::max(0, countBefore - 1);

	org->updateVehicleAgentPark(*state);

	if (countOwnedAlive(state, org, type) != countBefore)
	{
		LogError("No idle surplus vehicle exists, none should have been sold");
		return false;
	}
	if (org->balance != balanceBefore)
	{
		LogError("No-surplus case should not touch balance: was {0}, now {1}", balanceBefore,
		         org->balance);
		return false;
	}

	LogInfo("No-surplus boundary test passed");
	return true;
}

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");

	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	auto gamestate_name = config().getString("gamestate");
	if (gamestate_name.empty())
	{
		std::cerr << "Must provide gamestate\n";
		config().showHelp();
		return EXIT_FAILURE;
	}
	auto common_name = config().getString("common");
	if (common_name.empty())
	{
		std::cerr << "Must provide common gamestate\n";
		config().showHelp();
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);

	LogInfo("Loading common gamestate \"{0}\"", common_name);

	auto state = mksp<GameState>();
	if (!state->loadGame(common_name))
	{
		LogError("Failed to load common gamestate");
		return EXIT_FAILURE;
	}

	LogInfo("Loading gamestate \"{0}\"", gamestate_name);
	if (!state->loadGame(gamestate_name))
	{
		LogError("Failed to load supplied gamestate");
		return EXIT_FAILURE;
	}

	state->startGame();
	state->initState();
	state->fillPlayerStartingProperty();

	if (!test_settle_market_purchase_credits_seller(state))
	{
		LogError("settleMarketPurchase seller-credit test failed");
		return EXIT_FAILURE;
	}

	if (!test_settle_market_purchase_self_no_double_dip(state))
	{
		LogError("settleMarketPurchase self-purchase test failed");
		return EXIT_FAILURE;
	}

	auto vehicleParkOrg = findOrgWithHumanBuilding(state);
	if (!vehicleParkOrg)
	{
		LogError("No NPC org owning a CITYMAP_HUMAN building found for vehicle-park tests");
		return EXIT_FAILURE;
	}

	auto vehicleType = findNonSpaceLinerVehicleType(state, vehicleParkOrg);
	if (!vehicleType)
	{
		LogError("No usable vehicle type found for vehicle-park tests");
		return EXIT_FAILURE;
	}

	// These four run in sequence against the same org and vehicle type, each carrying forward
	// the vehicle count the previous one left behind.
	if (!test_vehicle_park_insufficient_funds_blocks_purchase(state, vehicleParkOrg, vehicleType))
	{
		LogError("Vehicle-park insufficient-funds test failed");
		return EXIT_FAILURE;
	}

	if (!test_vehicle_park_exact_funds_completes_purchase(state, vehicleParkOrg, vehicleType))
	{
		LogError("Vehicle-park exact-funds test failed");
		return EXIT_FAILURE;
	}

	if (!test_vehicle_park_surplus_sale_credits_balance(state, vehicleParkOrg, vehicleType))
	{
		LogError("Vehicle-park surplus-sale test failed");
		return EXIT_FAILURE;
	}

	if (!test_vehicle_park_no_surplus_is_noop(state, vehicleParkOrg, vehicleType))
	{
		LogError("Vehicle-park no-surplus test failed");
		return EXIT_FAILURE;
	}

	LogInfo("test_organisation_finance success - all tests passed");
	return EXIT_SUCCESS;
}
