#include "framework/framework.h"
#include "game/state/gamestateintrospect.h"
#include "framework/harness.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/facility.h"
#include "game/state/city/research.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/city/vequipment.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/damage.h"
#include "game/state/rules/city/facilitytype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/shared/aequipment.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "library/strings_format.h"
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace OpenApoc
{
namespace
{

const char *loadoutSlot(EquipmentSlotType slot)
{
	switch (slot)
	{
		case EquipmentSlotType::ArmorBody:
			return "Body";
		case EquipmentSlotType::ArmorLegs:
			return "Legs";
		case EquipmentSlotType::ArmorHelmet:
			return "Helmet";
		case EquipmentSlotType::ArmorLeftHand:
			return "LeftArm";
		case EquipmentSlotType::ArmorRightHand:
			return "RightArm";
		case EquipmentSlotType::LeftHand:
			return "LeftHand";
		case EquipmentSlotType::RightHand:
			return "RightHand";
		default:
			return "General";
	}
}

const char *loadoutKind(AEquipmentType::Type type)
{
	switch (type)
	{
		case AEquipmentType::Type::Armor:
			return "Armor";
		case AEquipmentType::Type::Weapon:
			return "Weapon";
		case AEquipmentType::Type::Ammo:
			return "Ammo";
		case AEquipmentType::Type::Grenade:
			return "Grenade";
		case AEquipmentType::Type::MotionScanner:
			return "MotionScanner";
		case AEquipmentType::Type::MediKit:
			return "MediKit";
		default:
			return "Other";
	}
}

UString loadoutToken(UString text)
{
	for (auto &ch : text)
	{
		if (ch == ' ' || ch == ':' || ch == '|' || ch == '+' || ch == '~' || ch == '=')
		{
			ch = '_';
		}
	}
	return text;
}

UString describeLoadoutAgents(GameState &state)
{
	UString out;
	int n = 0;
	for (const auto &a : state.agents)
	{
		const auto &agent = a.second;
		if (!agent || agent->owner != state.getPlayer() || !agent->type ||
		    agent->type->role != AgentType::Role::Soldier || !agent->type->inventory ||
		    !agent->type->allowsDirectControl || agent->isDead())
		{
			continue;
		}
		const auto building = agent->currentBuilding
		                          ? agent->currentBuilding
		                          : (agent->currentVehicle ? agent->currentVehicle->currentBuilding
		                                                   : StateRef<Building>{});
		UString kit;
		for (const auto &e : agent->equipment)
		{
			if (!e || !e->type)
				continue;
			EquipmentSlotType slot = EquipmentSlotType::General;
			for (const auto &s : agent->getSlots())
			{
				if (s.bounds.within(e->equippedPosition))
				{
					slot = s.type;
					break;
				}
			}
			kit += (kit.empty() ? "" : "+") +
			       format("{0}~{1}~{2}~{3}~{4}", e->type.id, loadoutSlot(slot),
			              e->payloadType ? e->payloadType.id : UString("-"), e->ammo, e->armor);
		}
		out +=
		    (out.empty() ? "" : "|") +
		    format("{0}:base={1}:home={2}:vehicle={3}:speed={4}:strength={5}:accuracy={6}:kit={7}",
		           a.first, building && building->base ? building->base.id : UString("-"),
		           agent->homeBuilding && agent->homeBuilding->base ? agent->homeBuilding->base.id
		                                                            : UString("-"),
		           agent->currentVehicle ? agent->currentVehicle.id : UString("-"),
		           agent->current_stats.speed, agent->modified_stats.strength,
		           agent->modified_stats.accuracy, kit.empty() ? UString("-") : kit);
		n++;
	}
	UString selectedVehicle = "-";
	if (state.current_city && state.current_city->cityViewSelectedOwnedVehicles.size() == 1)
	{
		const auto &vehicle = state.current_city->cityViewSelectedOwnedVehicles.front();
		if (vehicle && vehicle->owner == state.getPlayer())
			selectedVehicle = vehicle.id;
	}
	return format("count={0} selected_vehicle={1} detail={2}", n, selectedVehicle,
	              out.empty() ? UString("-") : out);
}

UString describeEquipmentCatalog(GameState &state)
{
	// Player knowledge only: researched equipment present in the market, our stores or our kit.
	// No alien loadout/score tables, unseen crews, or unrevealed research statistics.
	std::set<UString> owned, knownModifiers;
	for (const auto &a : state.agents)
	{
		if (!a.second || a.second->owner != state.getPlayer())
			continue;
		for (const auto &e : a.second->equipment)
		{
			if (!e || !e->type)
				continue;
			owned.insert(e->type.id);
			if (e->payloadType)
				owned.insert(e->payloadType.id);
		}
	}
	for (const auto &a : state.agent_types)
	{
		if (!a.second || !a.second->damage_modifier)
			continue;
		for (const auto &topic : state.research.topics)
		{
			if (topic.second && !topic.second->hidden && topic.second->isComplete() &&
			    topic.second->name == a.second->name)
			{
				knownModifiers.insert(a.second->damage_modifier.id);
			}
		}
	}
	std::map<UString, std::map<UString, int>> pending;
	auto cargo = [&pending](const Cargo &c)
	{
		if (c.type == Cargo::Type::Agent && c.count > 0 && c.destination && c.destination->base)
		{
			pending[c.id][c.destination->base.id] += c.count;
		}
	};
	for (const auto &b : state.buildings)
		if (b.second)
			for (const auto &c : b.second->cargo)
				cargo(c);
	for (const auto &v : state.vehicles)
		if (v.second)
			for (const auto &c : v.second->cargo)
				cargo(c);
	UString out;
	int n = 0;
	for (const auto &entry : state.agent_equipment)
	{
		const auto &e = entry.second;
		if (!e || e->bioStorage || !e->isResearched() || e->equipscreen_size.x <= 0 ||
		    e->equipscreen_size.y <= 0)
			continue;
		UString stores, incoming;
		bool stocked = false;
		for (const auto &b : state.player_bases)
		{
			if (!b.second)
				continue;
			const auto found = b.second->inventoryAgentEquipment.find(entry.first);
			const int count = found == b.second->inventoryAgentEquipment.end() ? 0 : found->second;
			stocked |= count > 0;
			stores += (stores.empty() ? "" : "+") + format("{0}~{1}", b.first, count);
			incoming += (incoming.empty() ? "" : "+") +
			            format("{0}~{1}", b.first, pending[entry.first][b.first]);
		}
		const bool listed = e->isMarketListed(state);
		if (!listed && !stocked && !owned.count(entry.first))
			continue;
		const auto economy = state.economy.find(entry.first);
		UString ammo, modifiers;
		for (const auto &a : e->ammo_types)
		{
			if (a && a->isResearched())
				ammo += (ammo.empty() ? "" : "+") + a.id;
		}
		if (e->damage_type)
		{
			for (const auto &m : e->damage_type->modifiers)
			{
				// Terrain susceptibility and researched species are rules, not hidden enemies.
				if (knownModifiers.count(m.first.id) || m.first.id.find("TERRAIN") != UString::npos)
					modifiers +=
					    (modifiers.empty() ? "" : "+") + format("{0}~{1}", m.first.id, m.second);
			}
		}
		out += (out.empty() ? "" : "|") +
		       format("{0}:name={1}:kind={2}:slot={3}:armor={4}:weight={5}:flight={6}:damage={7}:"
		              "range={8}:"
		              "accuracy={9}:fire_ticks={10}:capacity={11}:recharge={12}:burst={13}:damage_"
		              "type={14}:"
		              "explosive={15}:ammo={16}:research=1:usable={17}:price={18}:market={19}:"
		              "store_space={20}:"
		              "stores={21}:incoming={22}:modifiers={23}:damage_modifier={24}",
		              entry.first, loadoutToken(e->name), loadoutKind(e->type),
		              loadoutSlot(AgentType::getArmorSlotType(e->body_part)), e->armor, e->weight,
		              e->provides_flight ? 1 : 0, e->damage, e->getRangeInTiles(), e->accuracy,
		              e->fire_delay, e->max_ammo, e->recharge, e->burst,
		              e->damage_type ? e->damage_type.id : UString("-"),
		              e->damage_type && e->damage_type->explosive ? 1 : 0,
		              ammo.empty() ? UString("-") : ammo,
		              e->canBeUsed(state, state.getPlayer()) ? 1 : 0,
		              listed && economy != state.economy.end() ? economy->second.currentPrice : 0,
		              listed && economy != state.economy.end() ? economy->second.currentStock : 0,
		              e->store_space, stores.empty() ? UString("-") : stores,
		              incoming.empty() ? UString("-") : incoming,
		              modifiers.empty() ? UString("-") : modifiers,
		              e->damage_modifier ? e->damage_modifier.id : UString("-"));
		n++;
	}
	return format("count={0} purchase_base={1} difficulty={2} detail={3}", n,
	              state.current_base ? state.current_base.id : UString("-"), state.difficulty,
	              out.empty() ? UString("-") : out);
}

UString describeTime(GameState &state)
{
	const auto &t = state.gameTime;
	return format("ticks={0} day={1} week={2} time={3} date={4}", t.getTicks(), t.getDay(),
	              t.getWeek(), t.getShortTimeString(), t.getShortDateString());
}

UString describeFunds(GameState &state)
{
	const auto player = state.getPlayer();
	if (!player)
	{
		return "balance=? income=? (no player organisation)";
	}
	// The actual game-over condition, and it is a one-way latch: weeklyPlayerUpdate sets
	// fundingTerminated the first week that lifetime totalScore drops below -2400 (or the
	// government turns Hostile), zeroes income, and nothing anywhere resets it
	// (gamestate.cpp:1668-1680). So this is a countdown, not a dip to recover from -- a driver
	// that only watches its bank balance sees nothing wrong until the money has already stopped
	// for good. margin is how much lifetime score is left before that happens.
	const int total = state.totalScore.getTotal();
	return format("balance={0} income={1} score_total={2} score_week={3} funding_terminated={4} "
	              "margin_to_cutoff={5} tactical={6} research={7} incidents={8} ufos_downed={9} "
	              "craft_lost={10} incursions={11} city_damage={12}",
	              player->balance, player->income, total, state.weekScore.getTotal(),
	              state.fundingTerminated ? 1 : 0, total - (-2400),
	              state.totalScore.tacticalMissions, state.totalScore.researchCompleted,
	              state.totalScore.alienIncidents, state.totalScore.craftShotDownUFO,
	              state.totalScore.craftShotDownXCom, state.totalScore.incursions,
	              state.totalScore.cityDamage);
}

UString describeBases(GameState &state)
{
	size_t facilities = 0;
	for (const auto &b : state.player_bases)
	{
		if (b.second)
		{
			facilities += b.second->facilities.size();
		}
	}
	return format("bases={0} facilities={1}", state.player_bases.size(), facilities);
}

UString describeResearch(GameState &state)
{
	size_t complete = 0;
	size_t total = 0;
	for (const auto &t : state.research.topics)
	{
		if (!t.second)
		{
			continue;
		}
		total++;
		if (t.second->isComplete())
		{
			complete++;
		}
	}
	size_t labs = 0;
	size_t busyLabs = 0;
	for (const auto &l : state.research.labs)
	{
		if (!l.second)
		{
			continue;
		}
		labs++;
		if (l.second->current_project)
		{
			busyLabs++;
		}
	}
	// Per-lab detail, and how many topics could be started right now. Research throughput is the
	// gate on everything after the early game -- dimension travel, the alien-building chain, the
	// victory raid -- and "labs_busy=1 of 5" is invisible in a bare completion count. This is the
	// campaign's progress meter: without it there is no way to tell a campaign that is advancing
	// from one that is quietly spinning.
	// Only labs backed by a *built* facility at a player base can be given a project at all:
	// ResearchScreen lists facilities with buildTime == 0 (researchscreen.cpp:72-88), not the
	// global research.labs map. Counting the global map made "labs_busy=2 of 5" look like a
	// stuck driver when two of those five had no built facility behind them and a third was a
	// Workshop, which takes manufacturing, not research. Report what is actually assignable.
	std::set<UString> builtLabs;
	for (const auto &b : state.player_bases)
	{
		if (!b.second)
		{
			continue;
		}
		for (const auto &f : b.second->facilities)
		{
			if (f && f->lab && f->buildTime == 0)
			{
				builtLabs.insert(f->lab.id);
			}
		}
	}
	size_t assignable = 0, assignableBusy = 0;
	UString labDetail;
	for (const auto &l : state.research.labs)
	{
		if (!l.second)
		{
			continue;
		}
		const bool built = builtLabs.count(l.first) > 0;
		if (built)
		{
			assignable++;
			if (l.second->current_project)
			{
				assignableBusy++;
			}
		}
		const char *kind = l.second->type == ResearchTopic::Type::BioChem   ? "biochem"
		                   : l.second->type == ResearchTopic::Type::Physics ? "physics"
		                                                                    : "engineering";
		// Staffing and progress, not just "is something assigned". Lab::update returns
		// immediately when getTotalSkill() is zero (research.cpp:445-449), so a lab with a
		// project and no scientists in it looks busy and advances nothing, for ever.
		const int skill = l.second->getTotalSkill();
		const auto &proj = l.second->current_project;
		labDetail +=
		    (labDetail.empty() ? "" : "|") +
		    format("{0}:{1}:{2}:staff={3}:skill={4}:size={5}:{6}", l.first, kind,
		           built ? "built" : "unbuilt", l.second->assigned_agents.size(), skill,
		           l.second->size == ResearchTopic::LabSize::Large ? "large" : "small",
		           proj ? format("{0}({1}/{2})", proj.id, proj->man_hours_progress, proj->man_hours)
		                : UString("idle"));
	}
	// Topics that are unlocked, unfinished and not already running somewhere. Dependency
	// satisfaction is evaluated against the first player base, which is where the labs are.
	size_t startable = 0;
	if (!state.player_bases.empty())
	{
		const StateRef<Base> base{&state, state.player_bases.begin()->first};
		for (const auto &t : state.research.topics)
		{
			// Hidden topics are permanently excluded from the research selection UI
			// (researchselect.cpp:234), so counting them made startable wildly misleading --
			// it read 28 while every lab genuinely had nothing left to be offered.
			if (!t.second || t.second->isComplete() || t.second->current_lab || t.second->hidden ||
			    !t.second->dependencies.satisfied(base))
			{
				continue;
			}
			startable++;
		}
	}
	return format("topics={0} complete={1} labs={2} labs_busy={3} assignable={4} "
	              "assignable_busy={5} startable={6} labs_detail={7}",
	              total, complete, labs, busyLabs, assignable, assignableBusy, startable,
	              labDetail.empty() ? UString("-") : labDetail);
}

UString describeOrgs(GameState &state)
{
	const auto player = state.getPlayer();
	size_t hostile = 0;
	size_t allied = 0;
	int infiltration = 0;
	for (const auto &o : state.organisations)
	{
		if (!o.second || o.first == player.id)
		{
			continue;
		}
		infiltration += o.second->infiltrationValue;
		const auto rel = o.second->isRelatedTo(player);
		if (rel == Organisation::Relation::Hostile)
		{
			hostile++;
		}
		else if (rel == Organisation::Relation::Allied)
		{
			allied++;
		}
	}
	return format("orgs={0} hostile={1} allied={2} infiltration_sum={3}",
	              state.organisations.size(), hostile, allied, infiltration);
}

UString describeVehicles(GameState &state)
{
	const auto player = state.getPlayer();
	const auto aliens = state.getAliens();
	size_t mine = 0;
	size_t ufos = 0;
	size_t ufosHere = 0;
	size_t crashed = 0;
	for (const auto &v : state.vehicles)
	{
		const auto &vehicle = v.second;
		if (!vehicle || !vehicle->owner)
		{
			continue;
		}
		if (vehicle->owner.id == player.id)
		{
			mine++;
		}
		else if (vehicle->owner.id == aliens.id)
		{
			ufos++;
			// Only craft with a tileObject on the current city are actually on the map and
			// therefore targetable; the rest are in the other dimension or not yet spawned.
			// Reporting only the total made "11 UFOs" look targetable when none were.
			if (vehicle->tileObject && vehicle->city == state.current_city)
			{
				ufosHere++;
			}
			if (vehicle->crashed)
			{
				crashed++;
			}
		}
	}
	// Recovering a wreck needs a Soldier aboard (cityview.cpp:1069-1090), so "do we have a
	// crewed craft" decides whether the whole artifact/research chain can start at all.
	size_t crewed = 0;
	for (const auto &v : state.vehicles)
	{
		const auto &vehicle = v.second;
		if (!vehicle || !vehicle->owner || vehicle->owner.id != player.id)
		{
			continue;
		}
		for (const auto &a : vehicle->currentAgents)
		{
			if (a && a->type && a->type->role == AgentType::Role::Soldier)
			{
				crewed++;
				break;
			}
		}
	}
	return format("player_vehicles={0} crewed={1} ufos={2} ufos_in_city={3} ufos_crashed={4} "
	              "next_invasion={5}",
	              mine, crewed, ufos, ufosHere, crashed, state.nextInvasion);
}

// Turbo (city Speed5) is silently downgraded to Speed1 whenever canTurbo() is false, which is the
// dominant reason an automated run stops making progress. Surface the gate and its causes so a
// driver can react instead of stalling.
UString describeTurbo(GameState &state)
{
	size_t hostileAggressive = 0;
	size_t attackMissions = 0;
	const auto player = state.getPlayer();
	if (state.current_city)
	{
		for (const auto &v : state.vehicles)
		{
			const auto &vehicle = v.second;
			if (!vehicle || vehicle->city != state.current_city || !vehicle->owner ||
			    !vehicle->type)
			{
				continue;
			}
			if (vehicle->isDead() || vehicle->crashed)
			{
				continue;
			}
			// Mirror GameState::canTurbo exactly: only aggressive hostile craft block turbo,
			// so counting every hostile-owned vehicle would mislead the driver.
			if (vehicle->type->aggressiveness > 0 &&
			    vehicle->owner->isRelatedTo(player) == Organisation::Relation::Hostile)
			{
				hostileAggressive++;
			}
			for (const auto &m : vehicle->missions)
			{
				if (m.type == VehicleMission::MissionType::AttackBuilding ||
				    m.type == VehicleMission::MissionType::AttackVehicle)
				{
					attackMissions++;
					break;
				}
			}
		}
	}
	const size_t projectiles = state.current_city ? state.current_city->projectiles.size() : 0;
	return format("can_turbo={0} hostiles={1} attack_missions={2} projectiles={3}",
	              state.canTurbo() ? 1 : 0, hostileAggressive, attackMissions, projectiles);
}

UString describeAgents(GameState &state)
{
	const auto player = state.getPlayer();
	size_t mine = 0, soldiers = 0, soldiersFit = 0, armed = 0, atBase = 0, unarmedAtBase = 0;
	for (const auto &a : state.agents)
	{
		const auto &agent = a.second;
		if (!agent || !agent->owner || agent->owner.id != player.id)
		{
			continue;
		}
		mine++;
		if (!agent->type || agent->type->role != AgentType::Role::Soldier)
		{
			continue;
		}
		soldiers++;
		// Soldiers are lost permanently, and a campaign that cannot replace them runs out of
		// people to send. A wounded soldier still exists but cannot be dispatched, so "how many
		// can actually go on a mission" is the number that matters.
		if (!agent->isDead() && agent->modified_stats.health > 0)
		{
			soldiersFit++;
		}
		bool hasWeapon = false;
		for (const auto &e : agent->equipment)
		{
			if (e && e->type && e->type->type == AEquipmentType::Type::Weapon)
			{
				hasWeapon = true;
				break;
			}
		}
		if (hasWeapon)
		{
			armed++;
		}
		// Only a soldier standing in a base (itself, or aboard a craft parked in one) can be
		// handed equipment from that base's stores: the equip screen builds its inventory from
		// the FRONT agent's base, so a soldier out on a mission or still in transit shows an
		// empty inventory and every attempt to arm them silently changes nothing. Same chain as
		// AEquipScreen::getAgentBase.
		const auto &building = agent->currentBuilding
		                           ? agent->currentBuilding
		                           : (agent->currentVehicle ? agent->currentVehicle->currentBuilding
		                                                    : agent->currentBuilding);
		if (building && building->base && !agent->isDead())
		{
			atBase++;
			if (!hasWeapon)
			{
				unarmedAtBase++;
			}
		}
	}
	return format("agents_total={0} agents_player={1} soldiers={2} soldiers_fit={3} armed={4} "
	              "soldiers_at_base={5} unarmed_at_base={6}",
	              state.agents.size(), mine, soldiers, soldiersFit, armed, atBase, unarmedAtBase);
}

UString describeBattle(GameState &state)
{
	if (!state.current_battle)
	{
		return "in_battle=0";
	}
	const auto &battle = *state.current_battle;
	const auto player = state.getPlayer();
	size_t mine = 0, mineAlive = 0, hostiles = 0, hostilesAlive = 0, retreated = 0;
	for (const auto &u : battle.units)
	{
		const auto &unit = u.second;
		if (!unit || !unit->owner)
		{
			continue;
		}
		const bool isMine = unit->owner.id == player.id;
		if (isMine)
		{
			mine++;
			if (!unit->isDead())
			{
				mineAlive++;
			}
			if (unit->retreated)
			{
				retreated++;
			}
		}
		else
		{
			hostiles++;
			if (!unit->isDead())
			{
				hostilesAlive++;
			}
		}
	}
	// playerWon is the engine's own verdict (Battle::checkMissionEnd). Inferring a win from
	// "a debriefing appeared" counted a total squad wipe as a victory.
	// The mission type decides whether leaving is survivable. Withdrawing from a base defence
	// forfeits the base itself -- every facility goes "unbuilt", the labs and stores go with it,
	// and the campaign is over shortly after. A driver that cannot tell a base defence from an
	// ordinary crash site will eventually retreat from one and lose the game to a rule it never
	// saw.
	UString missionType = "unknown";
	switch (battle.mission_type)
	{
		case Battle::MissionType::AlienExtermination:
			missionType = "extermination";
			break;
		case Battle::MissionType::RaidAliens:
			missionType = "raid_aliens";
			break;
		case Battle::MissionType::BaseDefense:
			missionType = "base_defense";
			break;
		case Battle::MissionType::RaidHumans:
			missionType = "raid_humans";
			break;
		case Battle::MissionType::UfoRecovery:
			missionType = "ufo_recovery";
			break;
	}
	return format("player_won={0} ", battle.playerWon ? 1 : 0) +
	       format("in_battle=1 mode={0} units={1} mine={2} mine_alive={3} mine_retreated={4} "
	              "foes={5} foes_alive={6} hazards={7} mission_type={8} bases={9}",
	              battle.mode == Battle::Mode::RealTime ? "rt" : "tb", battle.units.size(), mine,
	              mineAlive, retreated, hostiles, hostilesAlive, battle.hazards.size(), missionType,
	              state.player_bases.size());
}

UString describeStage(GameState &state)
{
	const bool inBattle = state.current_battle != nullptr;
	return format("in_battle={0} city={1} defeated={2}", inBattle ? 1 : 0,
	              state.current_city ? state.current_city.id : UString("none"),
	              state.player_bases.empty() ? 1 : 0);
}

} // namespace

StateRef<Building> nextRaidableAlienBuilding(GameState &state)
{
	const auto city = state.cities.find("CITYMAP_ALIEN");
	if (city == state.cities.end() || !city->second)
	{
		return {};
	}
	// The vector's storage order and the building names are not the campaign order.
	for (int number = 0; number < 10; number++)
	{
		const auto topic = format("RESEARCH_ALIEN_BUILDING_{0}", number);
		for (const auto &ref : city->second->buildings)
		{
			const auto b = ref.getSp();
			if (b && b->owner == state.getAliens() && b->accessTopic &&
			    b->accessTopic.id == topic && b->accessTopic->isComplete() && b->isAlive())
			{
				return ref;
			}
		}
	}
	return {};
}

UString introspectGameState(GameState &state, const UString &query)
{
	// Checkpointing for long unattended runs: a multi-day campaign needs to survive a crash or a
	// restart. Uses the synchronous low-level serializer rather than SaveManager, which is async
	// and needs a LoadingScreen stage; resume by relaunching with --Game.Load=<path>.
	if (query.size() > 5 && to_lower(query.substr(0, 5)) == "save ")
	{
		const auto path = query.substr(5);
		if (!state.saveGame(path))
		{
			return "";
		}
		return format("saved={0}", path);
	}
	const auto q = to_lower(query);
	if (q == "time")
	{
		return describeTime(state);
	}
	if (q == "funds")
	{
		return describeFunds(state);
	}
	if (q == "bases")
	{
		return describeBases(state);
	}
	if (q == "research")
	{
		return describeResearch(state);
	}
	if (q == "orgs")
	{
		return describeOrgs(state);
	}
	// Per-wreck detail. ufos_crashed counts vehicle->crashed alone, while centre_on_crash also
	// demands a live tileObject in the current city -- so a wreck can be counted and still be
	// unfindable, which is exactly how recovery failed silently with wrecks on the map.
	// What the armoury actually holds. Applying an equipment template re-equips an agent from
	// base stores, so "the template did nothing" and "the stores are empty" look identical from
	// outside -- and buying is not instantaneous, purchases arrive as cargo.
	// Equipment template slots. Applying a template strips the agent first and re-equips from
	// stores, so applying an *empty* one disarms everybody -- which is exactly what happened
	// when the template was captured from a row that turned out not to be an armed soldier
	// (armed went 10 -> 4). The driver has to be able to check the template took before it
	// applies it down the roster.
	if (q == "templates")
	{
		UString out;
		for (size_t i = 0; i < state.agentEquipmentTemplates.size(); i++)
		{
			const auto &t = state.agentEquipmentTemplates[i];
			size_t weapons = 0;
			for (const auto &e : t.equipment)
			{
				if (e.type && e.type->type == AEquipmentType::Type::Weapon)
				{
					weapons++;
				}
			}
			// The item ids matter, not just the counts: applying a template re-equips those
			// exact types from stores, so the driver has to be able to buy the very things the
			// template names. Without this it buys plausible weapons, applies the template, and
			// arms nobody.
			UString names;
			for (const auto &e : t.equipment)
			{
				if (e.type)
				{
					names += (names.empty() ? "" : "+") + e.type.id;
				}
			}
			out += (out.empty() ? "" : "|") + format("{0}:items={1},weapons={2},types={3}", i,
			                                         t.equipment.size(), weapons,
			                                         names.empty() ? UString("-") : names);
		}
		return format("templates={0} detail={1}", state.agentEquipmentTemplates.size(),
		              out.empty() ? UString("-") : out);
	}
	// The alien-dimension buildings, which are the whole endgame. Each one is gated on its own
	// accessTopic being researched (buildingscreen.cpp:112-122); winning its raid force-completes
	// the unlock for the next. The last one carries victory=true, and beating it is the only
	// thing in the game that fires GameWon (battle.cpp:3506-3592). A driver needs to know
	// which link of that chain it is standing on.
	// The topics ResearchSelect would offer for the lab currently being viewed, in the same
	// order and with the same filtering (researchselect.cpp:222-240), so the driver can pick a
	// specific project by name instead of guessing a row. That matters because the route to
	// victory runs through particular topics -- RESEARCH_ADVANCED_WORKSHOP to unlock the large
	// workshop, then RESEARCH_ALIEN_BUILDING_0..9 -- and picking row 0 gets whatever happens to
	// be first.
	// Everything the engine knows about one topic, by id. The XML in data/common_patch is only a
	// patch over data extracted from the player's original game, so questions like "does
	// MANUFACTURE_BIO-TRANSPORT actually have prerequisites in the merged data" cannot be
	// answered by reading the repo -- only by asking the running game.
	if (q.size() > 6 && q.substr(0, 6) == "topic ")
	{
		const auto id = query.substr(6);
		const auto it = state.research.topics.find(id);
		if (it == state.research.topics.end() || !it->second)
		{
			return format("found=0 id={0}", id);
		}
		const auto &t = it->second;
		const char *kind = t->type == ResearchTopic::Type::BioChem   ? "biochem"
		                   : t->type == ResearchTopic::Type::Physics ? "physics"
		                                                             : "engineering";
		bool satisfied = false;
		if (!state.player_bases.empty())
		{
			const StateRef<Base> base{&state, state.player_bases.begin()->first};
			satisfied = t->dependencies.satisfied(base);
		}
		return format("found=1 id={0} type={1} complete={2} hidden={3} started={4} "
		              "large={5} deps_satisfied={6} man_hours={7}/{8} cost={9}",
		              id, kind, t->isComplete() ? 1 : 0, t->hidden ? 1 : 0, t->started ? 1 : 0,
		              t->required_lab_size == ResearchTopic::LabSize::Large ? 1 : 0,
		              satisfied ? 1 : 0, t->man_hours_progress, t->man_hours, t->cost);
	}
	if (q == "research_options")
	{
		if (!state.current_base)
		{
			return UString("options=0 detail=-");
		}
		const auto facility = state.current_base->selectedLab.lock();
		if (!facility || !facility->lab)
		{
			return UString("options=0 lab=none detail=-");
		}
		const auto lab = facility->lab;
		const StateRef<Base> base{&state, state.player_bases.begin()->first};
		// topic_list holds bare shared_ptrs; ids live in the topics map, so index back by pointer.
		std::map<const ResearchTopic *, UString> ids;
		for (const auto &kv : state.research.topics)
		{
			if (kv.second)
			{
				ids[kv.second.get()] = kv.first;
			}
		}
		UString out;
		size_t idx = 0;
		for (const auto &t : state.research.topic_list)
		{
			if (!t || t->type != lab->type)
			{
				continue;
			}
			if ((!t->dependencies.satisfied(base) && !t->started) || t->hidden)
			{
				continue;
			}
			const bool tooLarge = t->required_lab_size == ResearchTopic::LabSize::Large &&
			                      lab->size == ResearchTopic::LabSize::Small;
			const auto found = ids.find(t.get());
			bool running = false;
			for (const auto &entry : state.research.labs)
			{
				if (entry.second && entry.second->current_project.getSp() == t)
				{
					running = true;
					break;
				}
			}
			out += (out.empty() ? "" : "|") +
			       format("{0}={1},done={2},big={3},running={4},affordable={5}", idx,
			              found == ids.end() ? UString("?") : found->second,
			              t->isComplete() ? 1 : 0, tooLarge ? 1 : 0, running ? 1 : 0,
			              t->type != ResearchTopic::Type::Engineering ||
			                      state.getPlayer()->balance >= t->cost
			                  ? 1
			                  : 0);
			idx++;
		}
		const char *kind = lab->type == ResearchTopic::Type::Engineering ? "engineering"
		                   : lab->type == ResearchTopic::Type::Physics   ? "physics"
		                                                                 : "biochem";
		return format("options={0} lab={1} type={2} current={3} detail={4}", idx, lab.id, kind,
		              lab->current_project ? lab->current_project.id : UString("-"),
		              out.empty() ? UString("-") : out);
	}
	// The facility types BaseScreen would offer, in the order it builds its list
	// (basescreen.cpp:80-93: state.facility_types, filtered by isVisible()). Placement is a
	// hover-and-drag onto the base grid and every row is an identically-named Graphic, so
	// position in this list is the only way to know which row is which -- and the driver needs
	// the advanced physics lab and workshop specifically: gate craft require Large labs, while
	// the starting base has only small ones.
	if (q == "facilities")
	{
		UString out;
		UString costs;
		size_t idx = 0;
		for (const auto &f : state.facility_types)
		{
			if (!f.second || !f.second->isVisible())
			{
				continue;
			}
			out += (out.empty() ? "" : "|") + format("{0}={1}", idx, f.first);
			costs += (costs.empty() ? "" : "|") + format("{0}={1}", f.first, f.second->buildCost);
			idx++;
		}
		UString built;
		size_t pending = 0;
		if (state.current_base)
		{
			for (const auto &fac : state.current_base->facilities)
			{
				if (!fac || !fac->type)
				{
					continue;
				}
				if (fac->buildTime > 0)
				{
					pending++;
				}
				built +=
				    (built.empty() ? "" : ",") + format("{0}:{1}", fac->type.id, fac->buildTime);
			}
		}
		return format("buildable={0} pending={1} offer={2} base={3} costs={4}", idx, pending,
		              out.empty() ? UString("-") : out, built.empty() ? UString("-") : built,
		              costs.empty() ? UString("-") : costs);
	}
	// Which craft unlock what when recovered. Recovering a UFO force-completes its type's
	// researchUnlock list (cityview.cpp:4376-4379), and that is the *only* way hidden UFO unlock
	// topics complete -- they are excluded from the manual research
	// list for good. None of this is in data/: vehicle types come from the extractor, so the repo
	// cannot answer "which UFO do I need to shoot down", only the running game can.
	if (q == "ufo_types")
	{
		UString out;
		size_t n = 0;
		for (const auto &v : state.vehicle_types)
		{
			if (!v.second || v.second->researchUnlock.empty())
			{
				continue;
			}
			UString unlocks;
			for (const auto &u : v.second->researchUnlock)
			{
				unlocks += (unlocks.empty() ? "" : "+") + u.id;
			}
			// battle_map is what decides whether recovering this craft costs a battle at all:
			// UfoRecoveryBegin only starts a mission when the type has one, and otherwise fires
			// UfoRecoveryUnmanned and force-completes the same research for free
			// (cityview.cpp:4363-4380). Recovering the battle-free types is therefore a way to
			// advance the alien-craft tech chain without feeding soldiers into a fight.
			n++;
			out += (out.empty() ? "" : "|") +
			       format("{0}:battle={1}=>{2}", v.first, v.second->battle_map ? 1 : 0, unlocks);
		}
		return format("types_with_unlocks={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	// Human-city buildings that actually contain aliens right now, plus how the player stands
	// with the government. Investigating a building and finding nothing costs the owner
	// -5 - difficulty relation every time (buildingscreen.cpp:154-166), and alien crews relocate
	// between buildings on a timer -- so answering stale alerts steadily angers organisations.
	// If that organisation is the government, weeklyPlayerUpdate latches fundingTerminated and
	// income is gone permanently, which is how a campaign died at score -1312, nowhere near the
	// -2400 score cutoff.
	// Which of our craft can actually engage a UFO: flying, alive, and carrying a weapon. Sending
	// a Stormdog -- a road vehicle -- after an airborne UFO is not interception, it is a craft
	// wandering the streets while the UFO bombs the city. Reported in list order so the driver
	// can pick the right icons out of OWNED_VEHICLE_LIST.
	if (q == "equipment_catalog")
	{
		return describeEquipmentCatalog(state);
	}
	if (q == "loadout_agents")
	{
		return describeLoadoutAgents(state);
	}
	if (q == "buyable_guns")
	{
		// Which agent weapons the market will actually sell, and how hard they hit. The driver
		// had been stocking the armoury from its own equipment template, which names the exact
		// rifles the starting squad happened to carry -- and those cannot always be re-bought, so
		// the armoury stayed empty and recruits went unarmed while a campaign fielded six armed
		// soldiers of fifteen. Buying by capability instead of by name fixes that at the source.
		UString out;
		int n = 0;
		for (auto &et : state.agent_equipment)
		{
			const auto econ = state.economy.find(et.first);
			if (econ == state.economy.end() || !et.second)
			{
				continue;
			}
			if (econ->second.currentStock <= 0)
			{
				continue;
			}
			if (et.second->type != AEquipmentType::Type::Weapon)
			{
				continue;
			}
			if (!et.second->research_dependency.satisfied())
			{
				continue;
			}
			UString name = et.second->name;
			std::replace(name.begin(), name.end(), ' ', '_');
			int damage = et.second->damage;
			// A weapon that fires ammunition reports its damage on the ammo, not the gun.
			if (damage == 0 && !et.second->ammo_types.empty())
			{
				const auto &ammo = *et.second->ammo_types.begin();
				if (ammo)
				{
					damage = ammo->damage;
				}
			}
			if (n++ > 0)
			{
				out += "|";
			}
			out += format("{0}:damage={1}:price={2}:stock={3}", name, damage,
			              econ->second.currentPrice, econ->second.currentStock);
		}
		return format("count={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	if (q == "buyable_craft")
	{
		// Which craft can actually be bought, and crucially which of them FLY. Every campaign
		// starts with road vehicles only -- Stormdog, Wolfhound APC, a road bike -- so a driver
		// that buys "a vehicle" buys another thing that cannot reach a UFO, and the city is left
		// undefended while incursions and city damage bleed the score to death. The buy screen
		// offers exactly the types present in the economy (transactionscreen.cpp:217-220).
		UString out;
		int n = 0;
		for (auto &vt : state.vehicle_types)
		{
			const auto econ = state.economy.find(vt.first);
			if (econ == state.economy.end() || !vt.second)
			{
				continue;
			}
			if (econ->second.currentStock <= 0)
			{
				continue;
			}
			int weaponSlots = 0;
			for (auto &slot : vt.second->equipment_layout_slots)
			{
				if (slot.type == EquipmentSlotType::VehicleWeapon)
				{
					weaponSlots++;
				}
			}
			UString name = vt.second->name;
			std::replace(name.begin(), name.end(), ' ', '_');
			// Seats the craft will have when delivered: the type's own plus whatever its default
			// loadout adds. A driver sizing a squad has to know this BEFORE buying, because the
			// assignment screen silently seats only as many agents as there are seats.
			std::vector<sp<VEquipmentType>> loadout;
			for (const auto &pair : vt.second->initial_equipment_list)
			{
				if (pair.second)
				{
					loadout.push_back(pair.second.getSp());
				}
			}
			const int pax = vt.second->getMaxPassengers(loadout.begin(), loadout.end());
			if (n++ > 0)
			{
				out += "|";
			}
			out += format("{0}:flying={1}:weapons={2}:price={3}:stock={4}:pax={5}", name,
			              vt.second->type == VehicleType::Type::Flying ? 1 : 0, weaponSlots,
			              econ->second.currentPrice, econ->second.currentStock, pax);
		}
		return format("count={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	if (q == "interceptors")
	{
		const auto player = state.getPlayer();
		UString out;
		size_t idx = 0, usable = 0;
		for (const auto &v : state.vehicles)
		{
			const auto &veh = v.second;
			if (!veh || !veh->owner || veh->owner.id != player.id || veh->isDead())
			{
				continue;
			}
			const bool flying = veh->type && veh->type->type == VehicleType::Type::Flying;
			bool armed = false;
			for (const auto &e : veh->equipment)
			{
				if (e && e->type && e->type->type == EquipmentSlotType::VehicleWeapon)
				{
					armed = true;
					break;
				}
			}
			if (flying && armed)
			{
				usable++;
			}
			// Craft names contain spaces ("Valkyrie Interceptor 1") and the reply parser splits
			// fields on whitespace, so an unescaped name silently truncates the record and every
			// flag after it is lost -- which read as "no armed flying craft available" while
			// three were sitting on the pad.
			UString safeName = veh->name;
			std::replace(safeName.begin(), safeName.end(), ' ', '_');
			// Soldiers aboard too: recovering a wreck needs a *flying* craft carrying troops.
			// Picking the first crewed craft regardless of type selected a Stormdog -- a road
			// vehicle -- and every recovery was refused, which stalls the entire research chain
			// since UFO recovery is what unlocks it.
			size_t crew = 0;
			for (const auto &a : veh->currentAgents)
			{
				if (a && a->type && a->type->role == AgentType::Role::Soldier)
				{
					crew++;
				}
			}
			// hasDimensionShifter is the sole gate on crossing into the alien city
			// (vehiclemission.cpp GotoPortal: without it the craft is stranded or crashes on
			// arrival), so the driver has to be able to tell which craft can make the trip.
			// Passenger CAPACITY, not passengers aboard. crew= reports who is currently
			// carried, which is a different question: an empty troop transport reports crew=0
			// and looks indistinguishable from a pure fighter. A driver picking gate sentries on
			// crew=0 will happily send the APC that carries the squad, and losing it is what
			// left a whole campaign unable to fly a single ground mission.
			const int pax = veh->getMaxPassengers();
			// Where the craft sits in the base's assignment screen, or -1 when it is not parked
			// at the current base. BuildingScreen lists the player's vehicles that are in that
			// building in state.vehicles order (agentassignment.cpp updateLocation), so a craft's
			// row is its rank among those -- which is NOT its index in this list, because this
			// list also holds craft that are out on a mission. Driving "row 0" meant whatever
			// happened to be parked first: a road bike, a hoverbike, anything but the transport.
			int row = -1;
			const auto curBase = state.current_base;
			if (curBase && curBase->building && veh->currentBuilding == curBase->building)
			{
				row = 0;
				for (const auto &other : state.vehicles)
				{
					if (other.second == veh)
					{
						break;
					}
					if (other.second && other.second->owner &&
					    other.second->owner.id == player.id &&
					    other.second->currentBuilding == curBase->building)
					{
						row++;
					}
				}
			}
			bool portal = false;
			for (const auto &mission : veh->missions)
			{
				portal = portal || mission.type == VehicleMission::MissionType::GotoPortal;
			}
			out += (out.empty() ? "" : "|") +
			       format("{0}:{1}:flying={2},armed={3},crew={4},shifter={5},pax={6},city={7},"
			              "transit={8},home={9},id={10},portal={11},row={12}",
			              idx, safeName, flying ? 1 : 0, armed ? 1 : 0, crew,
			              veh->hasDimensionShifter() ? 1 : 0, pax, veh->city.id,
			              veh->betweenDimensions ? 1 : 0,
			              veh->currentBuilding && veh->currentBuilding == veh->homeBuilding ? 1 : 0,
			              v.first, portal ? 1 : 0, row);
			idx++;
		}
		return format("craft={0} interceptors={1} detail={2}", idx, usable,
		              out.empty() ? UString("-") : out);
	}
	if (q == "infiltrated")
	{
		const auto player = state.getPlayer();
		size_t n = 0;
		UString out;
		if (state.current_city)
		{
			for (const auto &ref : state.current_city->buildings)
			{
				const auto b = ref.getSp();
				if (!b || b->current_crew.empty())
				{
					continue;
				}
				size_t crew = 0;
				for (const auto &c : b->current_crew)
				{
					crew += c.second;
				}
				if (crew == 0)
				{
					continue;
				}
				n++;
				if (n <= 12)
				{
					out += (out.empty() ? "" : "|") + format("{0}:crew={1}", ref.id, crew);
				}
			}
		}
		int govRelation = 0;
		if (state.government && player)
		{
			govRelation = (int)state.government->getRelationTo(player);
		}
		return format("infiltrated={0} gov_relation={1} detail={2}", n, govRelation,
		              out.empty() ? UString("-") : out);
	}
	if (q == "alien_buildings")
	{
		const auto it = state.cities.find("CITYMAP_ALIEN");
		if (it == state.cities.end() || !it->second)
		{
			return UString("alien_buildings=0 detail=-");
		}
		const auto aliens = state.getAliens();
		size_t n = 0, raidable = 0;
		UString out;
		// City::buildings is a vector of StateRef<Building>, not a map.
		for (const auto &ref : it->second->buildings)
		{
			const auto bld = ref.getSp();
			if (!bld)
			{
				continue;
			}
			n++;
			const bool mine = bld->owner && bld->owner.id == aliens.id;
			const bool open = bld->accessTopic && bld->accessTopic->isComplete();
			const bool alive = bld->isAlive();
			if (mine && open && alive)
			{
				raidable++;
			}
			out += (out.empty() ? "" : "|") +
			       format("{0}:topic={1},open={2},alien={3},victory={4},alive={5}", ref.id,
			              bld->accessTopic ? bld->accessTopic.id : UString("-"), open ? 1 : 0,
			              mine ? 1 : 0, bld->victory ? 1 : 0, alive ? 1 : 0);
		}
		return format("alien_buildings={0} raidable={1} current_city={2} detail={3}", n, raidable,
		              state.current_city ? state.current_city.id : UString("none"),
		              out.empty() ? UString("-") : out);
	}
	if (q == "loot")
	{
		// The whole armoury, with the one fact that decides whether an item can be sold: has its
		// research been done. AllOutWar's guide is emphatic that captured gear is the campaign's
		// main income -- disruptors at $2500, boomeroids around $900 -- and equally emphatic that
		// it must be RESEARCHED first. So report the count and the research state per item, and
		// let the driver keep one of anything still unresearched rather than selling the only
		// specimen and stalling its own tech tree.
		UString out;
		int n = 0;
		std::map<UString, std::pair<int, int>> merged; // id -> (count, researched)
		for (const auto &b : state.player_bases)
		{
			if (!b.second)
			{
				continue;
			}
			for (const auto &e : b.second->inventoryAgentEquipment)
			{
				if (e.second == 0)
				{
					continue;
				}
				const auto type = StateRef<AEquipmentType>{&state, e.first};
				const int done = (type && type->research_dependency.satisfied()) ? 1 : 0;
				auto &slot = merged[e.first];
				slot.first += e.second;
				slot.second = done;
			}
		}
		for (const auto &m : merged)
		{
			if (n++ > 0)
			{
				out += "|";
			}
			out += format("{0}:have={1}:researched={2}", m.first, m.second.first, m.second.second);
		}
		return format("count={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	if (q == "stores")
	{
		size_t kinds = 0, total = 0, weapons = 0;
		UString top;
		for (const auto &b : state.player_bases)
		{
			if (!b.second)
			{
				continue;
			}
			for (const auto &e : b.second->inventoryAgentEquipment)
			{
				if (e.second == 0)
				{
					continue;
				}
				kinds++;
				total += e.second;
				// Weapons specifically: applying an equipment template strips an agent and
				// re-equips from stores, so doing it with no weapons in stock disarms people
				// instead of arming them.
				const auto type = StateRef<AEquipmentType>{&state, e.first};
				if (type && type->type == AEquipmentType::Type::Weapon)
				{
					weapons += e.second;
				}
				if (kinds <= 8)
				{
					top += (top.empty() ? "" : "|") + format("{0}x{1}", e.first, e.second);
				}
			}
		}
		// Vehicle equipment produced by workshops lands in inventoryVehicleEquipment.
		UString vehTop;
		size_t vehKinds = 0, vehTotal = 0;
		for (const auto &b : state.player_bases)
		{
			if (!b.second)
			{
				continue;
			}
			for (const auto &e : b.second->inventoryVehicleEquipment)
			{
				if (e.second == 0)
				{
					continue;
				}
				vehKinds++;
				vehTotal += e.second;
				if (vehKinds <= 8)
				{
					vehTop += (vehTop.empty() ? "" : "|") + format("{0}x{1}", e.first, e.second);
				}
			}
		}
		return format("agent_equipment_kinds={0} agent_equipment_total={1} weapons={2} "
		              "vehicle_kinds={3} vehicle_total={4} vehicle_top={5} top={6}",
		              kinds, total, weapons, vehKinds, vehTotal,
		              vehTop.empty() ? UString("-") : vehTop, top.empty() ? UString("-") : top);
	}
	if (q == "crashes")
	{
		const auto aliens = state.getAliens();
		UString out;
		int n = 0;
		for (const auto &v : state.vehicles)
		{
			const auto &vehicle = v.second;
			if (!vehicle || !vehicle->owner || vehicle->owner.id != aliens.id || !vehicle->crashed)
			{
				continue;
			}
			n++;
			// Report the craft type and whether recovering it costs a battle. A wreck whose type
			// has no battle_map is recovered unmanned and force-completes the same alien-craft
			// research for free (cityview.cpp:4363-4380), so a driver short of soldiers should
			// go for those first rather than take a crash-site fight it will lose.
			out += (out.empty() ? "" : "|") +
			       format("{0}:type={1},battle={2},tile={3},here={4},falling={5},sliding={6},"
			              "pos={7},{8},{9}",
			              v.first, vehicle->type ? vehicle->type.id : UString("-"),
			              (vehicle->type && vehicle->type->battle_map) ? 1 : 0,
			              vehicle->tileObject ? 1 : 0, vehicle->city == state.current_city ? 1 : 0,
			              vehicle->falling ? 1 : 0, vehicle->sliding ? 1 : 0,
			              (int)vehicle->getPosition().x, (int)vehicle->getPosition().y,
			              (int)vehicle->getPosition().z);
		}
		return format("crashes={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	if (q == "vehicles")
	{
		return describeVehicles(state);
	}
	// Performance diagnosis: how many vehicles the world holds, and whose. Every one is updated
	// every tick, so unbounded growth is CPU and memory the game never gives back.
	if (q == "vehicle_census")
	{
		std::map<UString, int> byOwner;
		int total = 0, dead = 0, crashed = 0, inCity = 0;
		for (const auto &v : state.vehicles)
		{
			total++;
			const auto &veh = v.second;
			if (!veh)
			{
				continue;
			}
			byOwner[veh->owner ? veh->owner.id : UString("none")]++;
			dead += veh->isDead() ? 1 : 0;
			crashed += veh->crashed ? 1 : 0;
			inCity += veh->city == state.current_city && veh->tileObject ? 1 : 0;
		}
		UString owners;
		for (const auto &o : byOwner)
		{
			owners += format("{0}{1}={2}", owners.empty() ? "" : "|", o.first, o.second);
		}
		extern uint64_t cityPathCalls, cityPathIterations, cityPathFailures, cityPathCacheHits;
		return format("total={0} dead={1} crashed={2} on_map={3} route_calls={4} "
		              "route_cache_hits={5} route_iterations={6} route_failures={7} sim_steps={8} "
		              "owners={9}",
		              total, dead, crashed, inCity, cityPathCalls, cityPathCacheHits,
		              cityPathIterations, cityPathFailures, fw().getFrameNumber(),
		              owners.empty() ? UString("-") : owners);
	}
	if (q == "agents")
	{
		return describeAgents(state);
	}
	if (q == "turbo")
	{
		return describeTurbo(state);
	}
	if (q == "battle")
	{
		return describeBattle(state);
	}
	if (q == "stage")
	{
		return describeStage(state);
	}
	if (q == "all")
	{
		return describeTime(state) + " " + describeFunds(state) + " " + describeBases(state) + " " +
		       describeResearch(state) + " " + describeOrgs(state) + " " + describeVehicles(state) +
		       " " + describeAgents(state) + " " + describeTurbo(state) + " " +
		       describeStage(state);
	}
	return "";
}

void registerGameStateIntrospection(const sp<GameState> &state)
{
	std::weak_ptr<GameState> weak = state;
	setHarnessQueryHandler(
	    [weak](const UString &query) -> UString
	    {
		    auto locked = weak.lock();
		    if (!locked)
		    {
			    return "";
		    }
		    return introspectGameState(*locked, query);
	    });
}

} // namespace OpenApoc
