#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/city/building.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"

using namespace OpenApoc;

namespace
{

// OPE-16 (partial): Cargo::refund used to debit originalOwner a second time on top of crediting
// the buyer back, even though nothing on this path (or, on current master, anywhere else) ever
// credits the seller for the sale -- so the debit was a plain unrelated penalty, not a reversal.
// This is a pure fund-accounting fixture -- no city, no framework -- exercising only
// Cargo::refund()'s cost > 0 path. Both organisations here are deliberately not the player, so
// the `destination->owner == state.getPlayer()` branch (which pushes a GameBaseEvent through
// fw()) never fires and the test stays framework-free.
bool testRefundCreditsBuyerWithoutRechargingSeller()
{
	GameState state;

	auto seller = mksp<Organisation>();
	seller->id = "ORG_TEST_SELLER";
	seller->balance = 500;
	state.organisations[seller->id] = seller;

	auto buyer = mksp<Organisation>();
	buyer->id = "ORG_TEST_BUYER";
	buyer->balance = 1000;
	state.organisations[buyer->id] = buyer;

	auto building = mksp<Building>();
	building->owner = StateRef<Organisation>{&state, buyer->id};
	state.buildings["BUILDING_TEST"] = building;

	Cargo cargo;
	cargo.type = Cargo::Type::Bio;
	cargo.id = "TEST_CARGO_ITEM";
	cargo.count = 4;
	cargo.divisor = 1;
	cargo.cost = 100;
	cargo.originalOwner = StateRef<Organisation>{&state, seller->id};
	cargo.destination = StateRef<Building>{&state, "BUILDING_TEST"};

	const int expectedRefund = cargo.cost * cargo.count / cargo.divisor;
	const int sellerBalanceBefore = seller->balance;
	const int buyerBalanceBefore = buyer->balance;

	cargo.refund(state, {});

	if (buyer->balance != buyerBalanceBefore + expectedRefund)
	{
		LogError("Buyer balance after refund was {0}, expected {1}", buyer->balance,
		         buyerBalanceBefore + expectedRefund);
		return false;
	}
	// This is the regression lock: the seller was already paid via the purchase path, so expiry
	// must not touch their balance a second time.
	if (seller->balance != sellerBalanceBefore)
	{
		LogError("Seller balance after refund was {0}, expected unchanged {1}", seller->balance,
		         sellerBalanceBefore);
		return false;
	}
	if (cargo.count != 0)
	{
		LogError("Cargo was not cleared after refund, count={0}", cargo.count);
		return false;
	}

	return true;
}

} // anonymous namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	if (!testRefundCreditsBuyerWithoutRechargingSeller())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
