// Manual census: traffic_census SAVE [TICKS=1000000] [STEP=6] [MODE=update] --Framework.Data=...
// --Framework.CD=... Resumes the saved city without startGame(), reseeding, changing missions, or
// deleting vehicles. Map entries are observed departures/spawns, not a count of
// dispatchAmbientTraffic calls.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tileobject_vehicle.h"
#include "library/strings_format.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <map>

using namespace OpenApoc;
namespace
{
constexpr uint64_t INTERVAL = 12000;
constexpr float EPSILON = 0.00001f;
bool onMap(const GameState &s, const Vehicle &v)
{
	return !v.isDead() && v.tileObject && v.city == s.current_city;
}
bool ground(const Vehicle &v)
{
	return v.type->type == VehicleType::Type::Road || v.type->type == VehicleType::Type::ATV;
}
bool civilian(const GameState &s, const Vehicle &v)
{
	return v.owner != s.getPlayer() && v.owner != s.getAliens() && v.owner.id != "ORG_MEGAPOL";
}
bool driveIntent(const Vehicle &v)
{
	if (v.missions.empty() || v.crashed)
		return false;
	const auto &m = v.missions.front();
	return m.type != VehicleMission::MissionType::Snooze &&
	       m.type != VehicleMission::MissionType::SelfDestruct;
}
float distance(Vec3<float> a, Vec3<float> b) { return glm::length(a - b); }
UString destinationBuilding(const Vehicle &v)
{
	for (const auto &m : v.missions)
		if (m.targetBuilding && (m.type == VehicleMission::MissionType::GotoBuilding ||
		                         m.type == VehicleMission::MissionType::OfferService))
			return m.targetBuilding.id;
	return {};
}
struct Track
{
	sp<Vehicle> vehicle; // Retain handles so removal cannot be mistaken for a successful arrival.
	Vec3<float> initialPosition, lastPosition;
	Vec3<int> tile, previousTile;
	UString tripBuilding;
	bool mapped = false, initialGround = false, moved = false, hasPreviousTile = false,
	     retired = false;
	uint64_t stationary = 0, changes = 0, reversals = 0;
	double travelled = 0;
};
struct Events
{
	uint64_t entries = 0, roadEntries = 0, flyerEntries = 0, civilianEntries = 0, policeEntries = 0;
	uint64_t buildingEntries = 0, targetArrivals = 0, deadExits = 0, otherExits = 0, created = 0;
	uint64_t retirements = 0;
};
using Tracks = std::map<UString, Track>;

void observe(GameState &s, Tracks &tracks, Events &events, unsigned ticks, bool initial = false)
{
	for (const auto &[id, v] : s.vehicles)
	{
		if (tracks.count(id))
			continue;
		Track t;
		t.vehicle = v;
		t.initialPosition = t.lastPosition = v->position;
		t.mapped = initial && onMap(s, *v);
		t.initialGround = t.mapped && ground(*v);
		if (v->tileObject)
			t.tile = v->tileObject->getOwningTile()->position;
		t.tripBuilding = destinationBuilding(*v);
		tracks.emplace(id, std::move(t));
		events.created += !initial;
	}
	for (auto &[id, t] : tracks)
	{
		auto &v = *t.vehicle;
		const bool mapped = onMap(s, v) && s.vehicles.count(id);
		if (!initial && mapped && !t.mapped)
		{
			events.entries++;
			events.roadEntries += v.type->type == VehicleType::Type::Road;
			events.flyerEntries += v.type->type == VehicleType::Type::Flying;
			events.civilianEntries += civilian(s, v);
			events.policeEntries += v.owner.id == "ORG_MEGAPOL";
			t.tripBuilding = destinationBuilding(v);
			t.tile = v.tileObject->getOwningTile()->position;
			t.hasPreviousTile = false;
		}
		if (!initial && !mapped && t.mapped)
		{
			const bool atDestination = v.currentBuilding && !t.tripBuilding.empty() &&
			                           t.tripBuilding == v.currentBuilding.id;
			t.retired = v.ambientTraffic && v.isDead() && atDestination;
			if (t.retired || (!v.isDead() && v.currentBuilding))
			{
				events.buildingEntries++;
				events.targetArrivals += atDestination;
				events.retirements += t.retired;
			}
			else if (v.isDead())
				events.deadExits++;
			else
				events.otherExits++;
		}
		t.moved = mapped && t.mapped && distance(v.position, t.lastPosition) > EPSILON;
		if (mapped && t.mapped && !initial)
		{
			t.travelled += distance(v.position, t.lastPosition);
			const auto tile = v.tileObject->getOwningTile()->position;
			if (tile != t.tile)
			{
				t.changes++;
				t.reversals += t.hasPreviousTile && tile == t.previousTile;
				t.previousTile = t.tile;
				t.tile = tile;
				t.hasPreviousTile = true;
			}
		}
		t.stationary = mapped && t.mapped && !t.moved && driveIntent(v) && ground(v)
		                   ? t.stationary + ticks
		                   : 0;
		if (mapped)
		{
			// A return leg can start in the same update as landing, without an observed map exit.
			const auto destination = destinationBuilding(v);
			if (!destination.empty())
				t.tripBuilding = destination;
		}
		t.lastPosition = v.position;
		t.mapped = mapped;
	}
}

void report(GameState &s, const Tracks &tracks, const Events &e, uint64_t elapsed)
{
	int roads = 0, atvs = 0, flyers = 0, civilians = 0, police = 0, parked = 0;
	int aliens = 0, crashedAliens = 0, playerFleet = 0, otherMap = 0, crashed = 0;
	int moving = 0, measuredMoving = 0, bearing = 0, stuck = 0, initialAlive = 0, initialMoved = 0;
	int initialDead = 0, initialParked = 0, initialAbsent = 0;
	unsigned maxRoadStoppedTicks = 0;
	int roadStopped12000 = 0;
	uint64_t changes = 0, reversals = 0;
	double travelled = 0;
	for (const auto &[id, t] : tracks)
	{
		const auto &v = *t.vehicle;
		parked += !v.isDead() && v.city == s.current_city && bool(v.currentBuilding);
		playerFleet += !v.isDead() && v.owner == s.getPlayer();
		changes += ground(v) ? t.changes : 0;
		reversals += ground(v) ? t.reversals : 0;
		travelled += ground(v) ? t.travelled : 0;
		if (t.initialGround)
		{
			initialAlive += t.mapped;
			initialMoved += t.travelled > EPSILON;
			initialDead += v.isDead() && !t.retired;
			initialParked += !v.isDead() && bool(v.currentBuilding);
			initialAbsent += !v.isDead() && !t.mapped && !v.currentBuilding;
		}
		if (!t.mapped)
			continue;
		roads += v.type->type == VehicleType::Type::Road;
		atvs += v.type->type == VehicleType::Type::ATV;
		flyers += v.type->type == VehicleType::Type::Flying;
		civilians += civilian(s, v);
		police += v.owner.id == "ORG_MEGAPOL";
		aliens += v.owner == s.getAliens();
		crashedAliens += v.owner == s.getAliens() && v.crashed;
		otherMap += v.owner != s.getPlayer();
		crashed += v.crashed;
		moving += ground(v) && glm::length(v.velocity) > EPSILON;
		measuredMoving += ground(v) && t.moved;
		bearing += ground(v) && !v.missions.empty();
		stuck += t.stationary >= INTERVAL;
		if (ground(v) && !v.missions.empty())
		{
			const auto stopped = v.missions.front().roadStoppedTicks;
			maxRoadStoppedTicks = std::max(maxRoadStoppedTicks, stopped);
			roadStopped12000 += stopped >= INTERVAL;
		}
	}
	std::cout << format(
	    "CENSUS elapsed={0} clock={1} road={2} atv={3} flying={4} civilian={5} "
	    "police={6} parked={7} ground_velocity_moving={8} ground_moved_last_step={9} "
	    "ground_mission_bearing={10} stationary_12000={11} max_road_stopped_ticks={12} "
	    "road_stopped_12000={13}\n",
	    elapsed, s.gameTime.getTicks(), roads, atvs, flyers, civilians, police, parked, moving,
	    measuredMoving, bearing, stuck, maxRoadStoppedTicks, roadStopped12000);
	std::cout << format("DISPATCH_CONTEXT player_fleet={0} nonplayer_onmap={1} aliens_onmap={2} "
	                    "crashed_aliens_onmap={3} crashed_onmap={4}\n",
	                    playerFleet, otherMap, aliens, crashedAliens, crashed);
	std::cout << format(
	    "EVENTS entries={0} road_entries={1} flyer_entries={2} civilian_entries={3} "
	    "police_entries={4} building_entries={5} confirmed_target_arrivals={6} "
	    "dead_exits={7} other_exits={8} newly_seen={9} ground_tile_changes={10} "
	    "ground_reversals={11} ground_distance={12} successful_retirements={13}\n",
	    e.entries, e.roadEntries, e.flyerEntries, e.civilianEntries, e.policeEntries,
	    e.buildingEntries, e.targetArrivals, e.deadExits, e.otherExits, e.created, changes,
	    reversals, travelled, e.retirements);
	std::cout << format("INITIAL_GROUND on_map_now={0} ever_changed_position={1} dead={2} "
	                    "in_building={3} elsewhere={4}\n",
	                    initialAlive, initialMoved, initialDead, initialParked, initialAbsent);
	std::cout.flush();
}

void reportBlockers(const GameState &state, const Tracks &tracks)
{
	for (const auto &[id, t] : tracks)
	{
		auto &v = *t.vehicle;
		if (!t.mapped || t.stationary < INTERVAL || v.missions.empty())
			continue;
		auto &m = v.missions.front();
		auto from = v.tileObject->getOwningTile();
		auto next = std::find_if(m.currentPlannedPath.begin(), m.currentPlannedPath.end(),
		                         [&](Vec3<int> p) { return p != from->position; });
		Vec3<int> nextPosition{-1, -1, -1};
		sp<Vehicle> blocker;
		int connections = 0;
		if (from->presentScenery)
			connections = std::count(from->presentScenery->type->connection.begin(),
			                         from->presentScenery->type->connection.end(), true);
		if (next != m.currentPlannedPath.end() && from->map.tileIsValid(*next))
		{
			nextPosition = *next;
			int exit = VehicleMission::roadHeading(from->position, *next);
			if (std::next(next) != m.currentPlannedPath.end())
			{
				const int onward = VehicleMission::roadHeading(*next, *std::next(next));
				if (onward >= 0)
					exit = onward;
			}
			blocker = m.roadBlocker(v, from, from->map.getTile(*next), exit);
		}
		std::cout << format("BLOCKER id={0} name={1} stationary={2} roadStoppedTicks={3} "
		                    "mission={4} missions={5} path={6} wait={7} reroutes={8} from={9} "
		                    "next={10} connections={11} goal={12} velocity={13} facing={14} "
		                    "position={15} initial_position={16} blocker={17}\n",
		                    id, v.name, t.stationary, m.roadStoppedTicks, m.getName(),
		                    v.missions.size(), m.currentPlannedPath.size(), m.blockedWaitTicks,
		                    m.reRouteAttempts, from->position, nextPosition, connections,
		                    v.goalPosition, v.velocity, v.facing, v.position, t.initialPosition,
		                    blocker ? Vehicle::getId(state, blocker) : UString("none"));
	}
}
void reportInventory(const Tracks &tracks, const char *phase)
{
	std::map<std::pair<UString, UString>, std::array<unsigned, 5>> groups;
	for (const auto &[id, t] : tracks)
	{
		const auto &v = *t.vehicle;
		auto &n = groups[{v.owner.id, v.type.id}];
		n[0] += t.mapped;
		n[1] += !v.isDead() && bool(v.currentBuilding);
		n[2] += !v.isDead() && v.crashed;
		n[3] += v.isDead() && !t.retired;
		n[4] += t.retired;
	}
	for (const auto &[key, n] : groups)
		std::cout << format("INVENTORY phase={0} owner={1} type={2} onmap={3} parked={4} "
		                    "crashed_alive={5} observed_dead={6} successful_retirements={7}\n",
		                    phase, key.first, key.second, n[0], n[1], n[2], n[3], n[4]);
}
} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("save", "Saved city to resume without starting a new game");
	config().addPositionalArgument("ticks", "Ticks to simulate (default 1000000)");
	config().addPositionalArgument("step", "Ticks per update (default 6)");
	config().addPositionalArgument("mode", "update, turbo, or turbo-unchecked (default update)");
	if (config().parseOptions(argc, argv))
		return EXIT_FAILURE;
	config().set("Config.Save", false);
	uint64_t total;
	unsigned step;
	const auto mode = config().getString("mode");
	const bool uncheckedTurbo = mode == "turbo-unchecked";
	const bool turbo = mode == "turbo" || uncheckedTurbo;
	try
	{
		total = config().getString("ticks").empty() ? 1000000
		                                            : std::stoull(config().getString("ticks"));
		const auto parsed =
		    config().getString("step").empty() ? 6 : std::stoul(config().getString("step"));
		if (!total || !parsed || parsed > TURBO_TICKS || config().getString("save").empty() ||
		    (!mode.empty() && mode != "update" && !turbo) ||
		    (turbo && (parsed != TURBO_TICKS || total % TURBO_TICKS)))
			throw std::invalid_argument(
			    "provide SAVE, positive TICKS, STEP between 1 and 43200, and MODE update, turbo, "
			    "or turbo-unchecked; "
			    "turbo requires STEP 43200 and a TICKS multiple of 43200");
		step = static_cast<unsigned>(parsed);
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		return EXIT_FAILURE;
	}
	Framework fw("OpenApoc", false);
	auto state = mksp<GameState>();
	if (!state->loadGame(config().getString("save")))
		return EXIT_FAILURE;
	state->initState();
	if (!state->current_city || !state->current_city->map || state->current_battle ||
	    state->gameTimeBeforeBattle.getTicks())
	{
		std::cerr << "A resumable city save without an active or pending battle is required\n";
		return EXIT_FAILURE;
	}
	if (turbo && state->gameTime.getTicks() % TURBO_TICKS)
	{
		std::cerr << "Turbo observation requires a save on a five-minute boundary\n";
		return EXIT_FAILURE;
	}
	uint64_t rng[2];
	state->rng.getState(rng);
	raiseLogMaxEnabledLevel(
	    LogLevel::Info); // Dispatch logs include attempts and reasons for zero sent.
	std::cout << format("RUN save={0} requested_ticks={1} step={2} rng={3},{4} city={5} mode={6}\n",
	                    config().getString("save"), total, step, rng[0], rng[1],
	                    state->current_city.id, mode.empty() ? "update" : mode);
	Tracks tracks;
	Events events;
	observe(*state, tracks, events, 0, true);
	report(*state, tracks, events, 0);
	reportInventory(tracks, "initial");
	const auto startClock = state->gameTime.getTicks();
	const auto start = std::chrono::steady_clock::now();
	unsigned unavailableTurboFrames = 0;
	for (uint64_t elapsed = 0, checkpoint = INTERVAL; elapsed < total;)
	{
		const unsigned ticks = static_cast<unsigned>(std::min<uint64_t>(step, total - elapsed));
		const auto before = state->gameTime.getTicks();
		if (turbo)
		{
			if (!state->canTurbo())
			{
				unavailableTurboFrames++;
				if (!uncheckedTurbo)
				{
					std::cerr << format("STOPPED turbo unavailable elapsed={0} clock={1}\n",
					                    elapsed, state->gameTime.getTicks());
					return EXIT_FAILURE;
				}
			}
			state->updateTurbo();
		}
		else
			state->update(ticks);
		if (state->gameTime.getTicks() != before + ticks)
		{
			std::cerr << "Simulation clock failed to advance by the requested ticks\n";
			return EXIT_FAILURE;
		}
		elapsed += ticks;
		observe(*state, tracks, events, ticks);
		if (elapsed >= checkpoint || elapsed == total)
		{
			report(*state, tracks, events, elapsed);
			checkpoint = (elapsed / INTERVAL + 1) * INTERVAL;
		}
	}
	reportBlockers(*state, tracks);
	reportInventory(tracks, "final");
	if (turbo)
		std::cout << format("TURBO_ELIGIBILITY unchecked={0} unavailable_frames={1}\n",
		                    uncheckedTurbo, unavailableTurboFrames);
	const double wall =
	    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::cout << format("FINISHED actual_clock_delta={0} wall_seconds={1}\n",
	                    state->gameTime.getTicks() - startClock, wall);
	return EXIT_SUCCESS;
}
