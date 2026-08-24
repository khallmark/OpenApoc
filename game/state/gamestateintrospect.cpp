#include "game/state/gamestateintrospect.h"
#include "framework/harness.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/base.h"
#include "game/state/city/facility.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/shared/aequipment.h"
#include <set>
#include "game/state/city/city.h"
#include "game/state/city/research.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/agenttype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "library/strings_format.h"
#include <memory>

namespace OpenApoc
{
namespace
{

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
	return format("balance={0} income={1} score_total={2} score_week={3}", player->balance,
	              player->income, state.totalScore.getTotal(), state.weekScore.getTotal());
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
		const char *kind = l.second->type == ResearchTopic::Type::BioChem     ? "biochem"
		                   : l.second->type == ResearchTopic::Type::Physics   ? "physics"
		                                                                      : "engineering";
		labDetail += (labDetail.empty() ? "" : "|") +
		             format("{0}:{1}:{2}:{3}", l.first, kind, built ? "built" : "unbuilt",
		                    l.second->current_project ? l.second->current_project.id : "idle");
	}
	// Topics that are unlocked, unfinished and not already running somewhere. Dependency
	// satisfaction is evaluated against the first player base, which is where the labs are.
	size_t startable = 0;
	if (!state.player_bases.empty())
	{
		const StateRef<Base> base{&state, state.player_bases.begin()->first};
		for (const auto &t : state.research.topics)
		{
			if (!t.second || t.second->isComplete() || t.second->current_lab ||
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
	return format("orgs={0} hostile={1} allied={2} infiltration_sum={3}", state.organisations.size(),
	              hostile, allied, infiltration);
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
	size_t mine = 0, soldiers = 0, soldiersFit = 0, armed = 0;
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
		for (const auto &e : agent->equipment)
		{
			if (e && e->type && e->type->type == AEquipmentType::Type::Weapon)
			{
				armed++;
				break;
			}
		}
	}
	return format("agents_total={0} agents_player={1} soldiers={2} soldiers_fit={3} armed={4}",
	              state.agents.size(), mine, soldiers, soldiersFit, armed);
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
	return format("player_won={0} ", battle.playerWon ? 1 : 0) + format("in_battle=1 mode={0} units={1} mine={2} mine_alive={3} mine_retreated={4} "
	              "foes={5} foes_alive={6} hazards={7}",
	              battle.mode == Battle::Mode::RealTime ? "rt" : "tb", battle.units.size(), mine,
	              mineAlive, retreated, hostiles, hostilesAlive, battle.hazards.size());
}

UString describeStage(GameState &state)
{
	const bool inBattle = state.current_battle != nullptr;
	return format("in_battle={0} city={1} defeated={2}", inBattle ? 1 : 0,
	              state.current_city ? state.current_city.id : UString("none"),
	              state.player_bases.empty() ? 1 : 0);
}

} // namespace

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
			out += (out.empty() ? "" : "|") +
			       format("{0}:items={1},weapons={2}", i, t.equipment.size(), weapons);
		}
		return format("templates={0} detail={1}", state.agentEquipmentTemplates.size(),
		              out.empty() ? UString("-") : out);
	}
	if (q == "stores")
	{
		size_t kinds = 0, total = 0;
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
				if (kinds <= 8)
				{
					top += (top.empty() ? "" : "|") + format("{0}x{1}", e.first, e.second);
				}
			}
		}
		return format("agent_equipment_kinds={0} agent_equipment_total={1} top={2}", kinds, total,
		              top.empty() ? UString("-") : top);
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
			out += (out.empty() ? "" : "|") +
			       format("{0}:tile={1},here={2},falling={3},sliding={4},pos={5},{6},{7}", v.first,
			              vehicle->tileObject ? 1 : 0,
			              vehicle->city == state.current_city ? 1 : 0, vehicle->falling ? 1 : 0,
			              vehicle->sliding ? 1 : 0, (int)vehicle->getPosition().x,
			              (int)vehicle->getPosition().y, (int)vehicle->getPosition().z);
		}
		return format("crashes={0} detail={1}", n, out.empty() ? UString("-") : out);
	}
	if (q == "vehicles")
	{
		return describeVehicles(state);
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
