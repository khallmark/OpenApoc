// Regression coverage for the AEquipScreen harness actions the campaign driver arms soldiers with.
//
// The driver used to arm people by clicking pixels: a row in the portrait list, then a Shift+click
// at the middle of an inventory rect. It reported "handed out 4 weapon(s)" while the armed count
// stayed put, because it counted attempts rather than effects and could not tell a click that
// landed from one that hit nothing. aequip_select / aequip_equip run the code a portrait click and
// a Shift+click run, by name, and answer OK only when the item really moved -- so these tests pin
// the effect (equipment on the agent, stock out of the base), not the reply text alone.

#include "forms/harness_actions.h"
#include "forms/ui.h"
#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/harness.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/gamestateintrospect.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/shared/aequipment.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "game/ui/general/aequipscreen.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

static UString action(const UString &line)
{
	HarnessCommand command;
	if (!parseHarnessCommand("ACTION " + line, command))
	{
		return "PARSE-ERROR " + command.error;
	}
	return getHarnessActionHandler()(command.text, command.args);
}

static UString query(const UString &q) { return getHarnessQueryHandler()(q); }

static UString control(const UString &line)
{
	HarnessCommand command;
	if (!parseHarnessCommand("CONTROL " + line, command))
		return "PARSE-ERROR";
	return getHarnessActionHandler()(command.text, command.args);
}

static size_t weaponsOn(const sp<Agent> &agent)
{
	size_t n = 0;
	for (const auto &e : agent->equipment)
	{
		if (e && e->type && e->type->type == AEquipmentType::Type::Weapon)
		{
			n++;
		}
	}
	return n;
}

static bool startsWith(const UString &s, const UString &prefix) { return s.rfind(prefix, 0) == 0; }

static bool test_select_and_equip_by_name()
{
	auto &state = *g_state;
	TEST_REQUIRE(!state.player_bases.empty(), "no player base");
	StateRef<Base> base{&state, state.player_bases.begin()->first};
	auto building = base->building;
	TEST_REQUIRE((bool)building, "base has no building");

	// A soldier standing in the base with nothing in their hands.
	sp<Agent> soldier;
	UString soldierId;
	for (auto &a : state.agents)
	{
		if (a.second && a.second->owner == state.getPlayer() && a.second->type &&
		    a.second->type->role == AgentType::Role::Soldier && a.second->type->inventory &&
		    a.second->type->allowsDirectControl)
		{
			soldier = a.second;
			soldierId = a.first;
			break;
		}
	}
	TEST_REQUIRE((bool)soldier, "no player soldier to equip");
	soldier->enterBuilding(state, building);
	for (auto e : std::list<sp<AEquipment>>(soldier->equipment))
	{
		soldier->removeEquipment(state, e);
	}
	TEST_REQUIRE(soldier->equipment.empty(), "could not strip the soldier");

	// Something this soldier can actually carry, and that needs no alien research.
	StateRef<AEquipmentType> gun;
	for (auto &t : state.agent_equipment)
	{
		StateRef<AEquipmentType> ref{&state, t.first};
		if (t.second->type == AEquipmentType::Type::Weapon &&
		    t.second->canBeUsed(state, state.getPlayer()) && soldier->findFirstSlot(ref).x != -1)
		{
			gun = ref;
			break;
		}
	}
	TEST_REQUIRE((bool)gun, "no usable weapon type in the rules");
	base->inventoryAgentEquipment.clear();
	base->inventoryAgentEquipment[gun.id] = 1;

	registerGameStateIntrospection(g_state);
	const auto screen = mksp<AEquipScreen>(g_state, soldier);
	screen->begin();
	// begin() lists the inventory before it ticks the "weapons" radio button, and the event that
	// makes the screen re-list is only delivered by the main loop; selecting the front agent again
	// refreshes the list the same way a portrait click does.
	screen->selectAgent(soldier);

	// Before the screen is asked anything, the introspection must already tell the truth.
	const auto agents = query("aequip_agents");
	TEST_REQUIRE(agents.find(soldierId + ":base=1:weapons=0") != UString::npos,
	             "aequip_agents should list the soldier at the base, unarmed: {0}", agents);
	const auto items = query("aequip_items");
	TEST_REQUIRE(items.find("id=" + gun.id) != UString::npos &&
	                 items.find("mode=Base") != UString::npos,
	             "aequip_items should offer the stocked weapon in Base mode: {0}", items);
	TEST_REQUIRE(query("agents").find("unarmed_at_base=") != UString::npos,
	             "gs agents must report how many unarmed soldiers are at a base: {0}",
	             query("agents"));

	// Refusals, none of which may change anything.
	TEST_REQUIRE(startsWith(action("aequip_select NO_SUCH_AGENT"), "ERR"),
	             "an unknown agent must be refused");
	TEST_REQUIRE(startsWith(action("aequip_equip AEQUIPMENTTYPE_NO_SUCH_THING"), "ERR"),
	             "an item that is not in the list must be refused");
	TEST_REQUIRE(startsWith(action("aequip_equip"), "ERR"), "missing argument must be refused");
	TEST_REQUIRE(weaponsOn(soldier) == 0 && base->inventoryAgentEquipment[gun.id] == 1,
	             "refusals must not move anything");

	const auto selected = action("aequip_select " + soldierId);
	TEST_REQUIRE(startsWith(selected, "OK") && selected.find("mode=Base") != UString::npos &&
	                 selected.find("base=1") != UString::npos,
	             "selecting the soldier: {0}", selected);

	// With the player's Shift+click shortcut switched off the action must refuse, not pretend.
	config().set("OpenApoc.NewFeature.AdvancedInventoryControls", false);
	const auto off = action("aequip_equip " + gun.id);
	config().set("OpenApoc.NewFeature.AdvancedInventoryControls", true);
	TEST_REQUIRE(startsWith(off, "ERR") && weaponsOn(soldier) == 0,
	             "must refuse when Shift+click is off: {0}", off);

	// The real thing: the weapon leaves stores and lands on the agent.
	const auto reply = action("aequip_equip " + gun.id);
	TEST_REQUIRE(startsWith(reply, "OK equipped=" + gun.id), "equip reply: {0}", reply);
	TEST_REQUIRE(weaponsOn(soldier) == 1, "the soldier must now carry the weapon, has {0}",
	             weaponsOn(soldier));
	TEST_REQUIRE(base->inventoryAgentEquipment[gun.id] == 0,
	             "the weapon must have left base stores, {0} left",
	             base->inventoryAgentEquipment[gun.id]);
	TEST_REQUIRE(query("aequip_agents").find(soldierId + ":base=1:weapons=1") != UString::npos,
	             "aequip_agents must now show the soldier armed: {0}", query("aequip_agents"));

	// Nothing left to give: refused, and the agent is untouched.
	const auto empty = action("aequip_equip " + gun.id);
	TEST_REQUIRE(startsWith(empty, "ERR") && weaponsOn(soldier) == 1, "stock is gone: {0}", empty);

	// Keep going on a full agent: the screen must say ERR rather than OK when nothing fits, and
	// hand the item back to stores.
	base->inventoryAgentEquipment[gun.id] = 64;
	screen->selectAgent(soldier);
	size_t carried = soldier->equipment.size();
	bool refused = false;
	for (int i = 0; i < 64 && !refused; i++)
	{
		const auto r = action("aequip_equip " + gun.id);
		if (startsWith(r, "OK"))
		{
			TEST_REQUIRE(soldier->equipment.size() == carried + 1,
			             "OK must mean one more item on the agent");
			carried++;
		}
		else
		{
			refused = true;
			TEST_REQUIRE(soldier->equipment.size() == carried, "ERR must mean nothing moved: {0}",
			             r);
		}
	}
	TEST_REQUIRE(refused, "an agent has finite slots; the action must eventually refuse");
	TEST_REQUIRE(base->inventoryAgentEquipment[gun.id] == 64 - (carried - 1),
	             "refused equip must leave stock intact: {0} in stores, {1} carried",
	             base->inventoryAgentEquipment[gun.id], carried);

	screen->finish();
	return true;
}

static bool test_named_refit_and_roster()
{
	auto &state = *g_state;
	StateRef<Base> base{&state, state.player_bases.begin()->first};
	sp<Agent> soldier;
	UString id;
	for (const auto &a : state.agents)
	{
		if (a.second->owner == state.getPlayer() &&
		    a.second->type->role == AgentType::Role::Soldier)
		{
			soldier = a.second;
			id = a.first;
			break;
		}
	}
	TEST_REQUIRE(soldier, "player soldier");
	soldier->enterBuilding(state, base->building);
	for (auto e : std::list<sp<AEquipment>>(soldier->equipment))
		soldier->removeEquipment(state, e);
	base->inventoryAgentEquipment.clear();
	const UString gunId = "AEQUIPMENTTYPE_MEGAPOL_AUTO_CANNON";
	const UString apId = "AEQUIPMENTTYPE_AUTO_CANNON_AP_CLIP";
	const UString heId = "AEQUIPMENTTYPE_AUTO_CANNON_HE_CLIP";
	const UString armorId = "AEQUIPMENTTYPE_MARSEC_BODY_UNIT";
	const int capacity = state.agent_equipment[apId]->max_ammo;
	base->inventoryAgentEquipment[gunId] = 1;
	base->inventoryAgentEquipment[apId] = capacity * 2;
	base->inventoryAgentEquipment[heId] = capacity * 2;
	base->inventoryAgentEquipment[armorId] = 1;
	const auto screen = mksp<AEquipScreen>(g_state, soldier);
	screen->begin();
	screen->selectAgent(soldier);
	TEST_REQUIRE(startsWith(action("aequip_equip " + gunId), "OK"), "issue firearm");
	auto gun = soldier->equipment.front();
	TEST_REQUIRE(startsWith(action("aequip_reload " + gunId + " " + apId), "OK"), "select AP ammo");
	TEST_REQUIRE(gun->payloadType.id == apId && gun->ammo == capacity, "AP loaded from stores");
	const int beforeAP = base->inventoryAgentEquipment[apId];
	const int beforeHE = base->inventoryAgentEquipment[heId];
	TEST_REQUIRE(startsWith(action("aequip_reload " + gunId + " " + heId), "OK"), "swap ammo");
	TEST_REQUIRE(gun->payloadType.id == heId && gun->ammo == capacity &&
	                 base->inventoryAgentEquipment[apId] == beforeAP + capacity &&
	                 base->inventoryAgentEquipment[heId] == beforeHE - capacity,
	             "swapping conserves both payload types");
	TEST_REQUIRE(startsWith(action("aequip_reload " + gunId + " " + armorId), "ERR"),
	             "incompatible reload refused");
	base->inventoryAgentEquipment[apId] = 0;
	TEST_REQUIRE(startsWith(action("aequip_reload " + gunId + " " + apId), "ERR") &&
	                 gun->payloadType.id == heId && gun->ammo == capacity,
	             "empty stock cannot unload a working gun");
	config().set("OpenApoc.NewFeature.AdvancedInventoryControls", false);
	TEST_REQUIRE(startsWith(action("aequip_unequip " + gunId), "ERR"), "shortcut disabled");
	config().set("OpenApoc.NewFeature.AdvancedInventoryControls", true);
	TEST_REQUIRE(query("loadout_agents").find(gunId + "~RightHand~" + heId) != UString::npos,
	             "roster reports actual hand and payload");
	TEST_REQUIRE(startsWith(action("aequip_unequip " + gunId), "OK"), "return firearm to stores");
	TEST_REQUIRE(soldier->equipment.empty() && base->inventoryAgentEquipment[gunId] == 1 &&
	                 base->inventoryAgentEquipment[heId] == beforeHE,
	             "returning loaded gun conserves weapon and ammo");
	base->inventoryAgentEquipment[apId] = capacity;
	screen->selectAgent(soldier);
	TEST_REQUIRE(startsWith(action("aequip_equip " + gunId), "OK"), "reissue firearm");
	gun = soldier->equipment.front();
	const auto payload = gun->payloadType;
	const int rounds = gun->ammo;
	screen->update(); // Register live controls just as a frame does, without rendering or a window.
	// setChecked is the radio button's player event path. The headless test has no loop to
	// deliver the queued CheckBoxChange, so refresh through the existing portrait action.
	const auto armorTab = control("BUTTON_SHOW_ARMOUR set 1");
	TEST_REQUIRE(startsWith(armorTab, "OK"), "armor tab: {0}", armorTab);
	screen->selectAgent(soldier);
	TEST_REQUIRE(startsWith(action("aequip_reload " + gunId + " " + apId), "ERR") &&
	                 gun->payloadType == payload && gun->ammo == rounds,
	             "wrong tab cannot unload a working gun");
	TEST_REQUIRE(startsWith(action("aequip_equip " + armorId), "OK"), "issue flight body piece");
	TEST_REQUIRE(query("loadout_agents").find(armorId + "~Body~") != UString::npos,
	             "roster reports armor body slot");
	TEST_REQUIRE(startsWith(action("aequip_unequip " + armorId), "OK") &&
	                 base->inventoryAgentEquipment[armorId] == 1,
	             "return body armor");
	// Selected-vehicle scope uses stable ids and still resolves a base for parked passengers.
	StateRef<Vehicle> parked;
	for (const auto &v : state.vehicles)
	{
		if (v.second->owner == state.getPlayer() && v.second->currentBuilding == base->building)
		{
			parked = {&state, v.first};
			break;
		}
	}
	TEST_REQUIRE(parked, "starting parked vehicle");
	soldier->currentVehicle = parked;
	soldier->currentBuilding.clear();
	state.current_city->cityViewSelectedOwnedVehicles = {parked};
	const auto roster = query("loadout_agents");
	TEST_REQUIRE(roster.find("selected_vehicle=" + parked.id) != UString::npos &&
	                 roster.find(id + ":base=" + base.id) != UString::npos,
	             "selected parked transport and passenger base: {0}", roster);
	state.current_city->cityViewSelectedOwnedVehicles.clear();
	soldier->currentVehicle.clear();
	soldier->currentBuilding = base->building;
	screen->finish();
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
	applyDeterministicTestConfig();

	const auto common = config().getString("common");
	const auto gamestate = config().getString("gamestate");
	if (common.empty() || gamestate.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}
	const int rc = runTestSuite({{"select and equip by name", test_select_and_equip_by_name},
	                             {"named refit and roster", test_named_refit_and_roster}});
	g_state.reset();
	UI::unload();
	return rc;
}
