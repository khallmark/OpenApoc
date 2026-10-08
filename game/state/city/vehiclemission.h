#pragma once

#include "game/state/rules/city/vehicletype.h"
#include "game/state/stateobject.h"

#include "game/state/tilemap/tilemap.h"
#include "library/strings.h"
#include "library/vec.h"
#include <algorithm>
#include <deque>
#include <list>
#include <vector>

namespace OpenApoc
{

static const int TELEPORTER_SPREAD = 10;
static const int DIMENSION_GATE_DELAY = TICKS_PER_SECOND / 2;
static const int FOLLOW_BOUNDS_XY = 4;
static const int FOLLOW_BOUNDS_Z = 1;
static const int TARGET_BUILDING_DISTANCE_LIMIT = 30;
static const float FOLLOW_RANGE =
    sqrtf(2 * FOLLOW_BOUNDS_XY * FOLLOW_BOUNDS_XY + FOLLOW_BOUNDS_Z * FOLLOW_BOUNDS_Z);

class Vehicle;
class Tile;
class TileMap;
class Building;
class City;

class FlyingVehicleTileHelper : public CanEnterTileHelper
{
  private:
	TileMap &map;
	const Vehicle &v;
	Vec2<int> size;
	bool large;
	int altitude;

  public:
	FlyingVehicleTileHelper(TileMap &map, const Vehicle &v);

	bool canEnterTile(Tile *from, Tile *to, bool ignoreStaticUnits = false,
	                  bool ignoreMovingUnits = true, bool ignoreAllUnits = false) const override;

	float pathOverheadAlloawnce() const override;

	// Support 'from' being nullptr for if a vehicle is being spawned in the map
	bool canEnterTile(Tile *from, Tile *to, bool, bool &, float &cost, bool &, bool, bool,
	                  bool) const override;

	float adjustCost(Vec3<int> nextPosition, int z) const override;

	float getDistance(Vec3<float> from, Vec3<float> to) const override;
	float getDistance(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd) const override;

	static float getDistanceStatic(Vec3<float> from, Vec3<float> to);
	static float getDistanceStatic(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd);

	bool canLandOnTile(Tile *to) const;

	Vec3<int> findTileToLandOn(GameState &, sp<TileObjectVehicle> vTile) const;

	Vec3<float> findSidestep(GameState &state, sp<TileObjectVehicle> vTile,
	                         sp<TileObjectVehicle> targetTile, float distancePref) const;
};

class GroundVehicleTileHelper : public CanEnterTileHelper
{
  private:
	TileMap &map;
	VehicleType::Type type;
	const Vehicle *v = nullptr;

  public:
	GroundVehicleTileHelper(TileMap &map, Vehicle &v);
	GroundVehicleTileHelper(TileMap &map, VehicleType::Type type);

	bool canEnterTile(Tile *from, Tile *to, bool ignoreStaticUnits = false,
	                  bool ignoreMovingUnits = true, bool ignoreAllUnits = false) const override;

	float pathOverheadAlloawnce() const override;

	// Support 'from' being nullptr for if a vehicle is being spawned in the map
	bool canEnterTile(Tile *from, Tile *to, bool, bool &, float &cost, bool &, bool, bool,
	                  bool) const override;

	float getDistance(Vec3<float> from, Vec3<float> to) const override;
	float getDistance(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd) const override;

	static float getDistanceStatic(Vec3<float> from, Vec3<float> to);
	static float getDistanceStatic(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd);

	// The vehicle_data size_x/size_y tiles covered when the origin is `origin`, row by row.
	// Computed on the fly: canEnterTile asks for it on every step of every ground route.
	class Footprint
	{
	  public:
		class iterator
		{
		  public:
			iterator(const Footprint &footprint, int index) : footprint(footprint), index(index) {}
			Vec3<int> operator*() const { return footprint[index]; }
			iterator &operator++()
			{
				++index;
				return *this;
			}
			bool operator!=(const iterator &other) const { return index != other.index; }

		  private:
			const Footprint &footprint;
			int index;
		};

		Footprint(Vec3<int> origin, Vec2<int> size)
		    : origin(origin), width(std::max(1, size.x)), height(std::max(1, size.y))
		{
		}
		size_t size() const { return static_cast<size_t>(width * height); }
		Vec3<int> operator[](size_t index) const
		{
			const int i = static_cast<int>(index);
			return {origin.x + i % width, origin.y + i / width, origin.z};
		}
		iterator begin() const { return {*this, 0}; }
		iterator end() const { return {*this, width * height}; }

	  private:
		Vec3<int> origin;
		int width;
		int height;
	};
	static Footprint footprintTiles(Vec3<int> origin, Vec2<int> size)
	{
		return Footprint(origin, size);
	}

	// Convert vector direction into index for tube array
	int convertDirection(Vec3<int> dir) const;

	bool isMoveAllowedRoad(Scenery &scenery, int dir) const;
	bool isMoveAllowedATV(Scenery &scenery, int dir) const;
};

class VehicleTargetHelper
{
  public:
	// Is a given tile reachable by a given vehicle type?
	enum class Reachability
	{
		Reachable,
		BlockedByVehicle, // We only check for this if VehicleAvoidance is not Ignore.
		BlockedByScenery,
		BlockedByBuilding // Specificaly so AI can recover vehicle at any position of the map which
		                  // is not inside a building
	};

	// Desired target adjustment behavior when the given target tile is BlockedByVehicle.
	enum class VehicleAvoidance
	{
		Ignore,          // Use given target anyway.
		PickNearbyPoint, // Go to a nearby point instead and allow mission to finish.
		Sidestep,        // Go to a nearby point and try to re-route to the original target.
	};

	struct AdjustTargetResult
	{
		// Original target point's reachability.
		Reachability reachability;

		// Whether we were able to find a suitable target point. This is false only in extreme cases
		// where there was no room in the sky, no roads in the city, etc.
		bool foundSuitableTarget;
	};

	// Potentially modifies the given target location such that it is reachable for the given
	// vehicle. Args:
	//   state: Game state.
	//   vehicle: Vehicle, only used for looking up the type.
	//   target: Reference to target location, may be modified by this method.
	//   vehicleAvoidance: Vehicle avoidance strategy. Only relevant for flyers and UFOs. See enum
	//   above. adjustForFlying: If false, don't do any checks or adjustment for flyers and UFOs.
	// Returns: AdjustTargetResult, see above for details.
	static AdjustTargetResult adjustTargetToClosest(GameState &state, Vehicle &v, Vec3<int> &target,
	                                                const VehicleAvoidance vehicleAvoidance,
	                                                bool adjustForFlying);

	// Checks reachability of the target location for the given vehicle.
	// Args:
	//   vehicle: Vehicle, only used for looking up the type.
	//   target: Location to be checked for reachability.
	// Returns: Reachability, see above for details.
	static Reachability isReachableTarget(const Vehicle &v, Vec3<int> target);

	// Checks for reachability to recover AI vehicles, specificaly as seperate function since Target
	// Tile is will always be blocked by vehicle to be rescued and mostly has no landing pad Returns
	// reachable for recovery as long as tgt tile is not part of building
	static Reachability isReachableForRecovery(const Vehicle &v, Vec3<int> target);

  private:
	static AdjustTargetResult adjustTargetToClosestFlying(GameState &state, Vehicle &v,
	                                                      Vec3<int> &target,
	                                                      const VehicleAvoidance vehicleAvoidance);
	static AdjustTargetResult adjustTargetToClosestRoad(Vehicle &v, Vec3<int> &target);
	static AdjustTargetResult adjustTargetToClosestGround(Vehicle &v, Vec3<int> &target);

	static Reachability isReachableTargetFlying(const Vehicle &v, Vec3<int> target);
	static Reachability isReachableTargetRoad(const Vehicle &v, Vec3<int> target);
	static Reachability isReachableTargetGround(const Vehicle &v, Vec3<int> target);
};

class VehicleMission
{
  private:
	// INTERNAL: Not to be used directly (Only works when in building)
	static VehicleMission takeOff(Vehicle &v);
	// INTERNAL: Not to be used directly (Only works if directly above a pad)
	static VehicleMission land(Vehicle &v, StateRef<Building> b);
	// INTERNAL: This checks if mission is actually finished. Called by isFinished.
	// If it is finished, update() is called by isFinished so that any remaining work could be done
	bool isFinishedInternal(GameState &state, Vehicle &v);

	bool takeOffCheck(GameState &state, Vehicle &v);
	bool teleportCheck(GameState &state, Vehicle &v);

  public:
	VehicleMission() = default;

	// Methods used in pathfinding etc.
	bool getNextDestination(GameState &state, Vehicle &v, Vec3<float> &destPos, float &destFacing,
	                        int &turboTiles);
	void update(GameState &state, Vehicle &v, unsigned int ticks, bool finished = false);
	bool isFinished(GameState &state, Vehicle &v, bool callUpdateIfFinished = true);
	void start(GameState &state, Vehicle &v);
	void setPathTo(GameState &state, Vehicle &v, Vec3<int> target, int maxIterations,
	               bool checkValidity = true, bool giveUpIfInvalid = false);
	void setFollowPath(GameState &state, Vehicle &v);
	bool advanceAlongPath(GameState &state, Vehicle &v, Vec3<float> &destPos, float &destFacing,
	                      int &turboTiles);
	// An ATV blocked by another vehicle: plan a short walk around it, treating vehicles as walls
	// (UFO2P FUN_0003f704, the planner of the kind-2 vehicles FUN_000395E4 moves). Replaces the
	// planned path and returns true, or returns false when every way out is blocked.
	bool planAroundVehicles(Vehicle &v, Vec3<int> target);
	// UFO2P's road-vehicle rules (kind 0, FUN_000303e4 -> FUN_00033818 / FUN_00032428): two lanes,
	// right-hand traffic. The road vehicle that stops v stepping from `from` to `to` and then out
	// of `to` heading `exitHeading`: one in `to` leaving it the same way (queue behind it), or, on
	// a junction tile only, one whose way across it crosses v's. Cars going other ways pass.
	sp<Vehicle> roadBlocker(const Vehicle &v, const Tile *from, Tile *to, int exitHeading) const;
	// Stopped long enough on a straight road: turn round where it stands (FUN_00032428), then drive
	// back to the last junction and on from it by any way but back into the jam. Replaces the
	// planned path; returns false if it cannot turn here.
	bool roadUTurn(Vehicle &v, Tile *from, int heading);
	// NESW index of a one-tile step, -1 if it is not one.
	static int roadHeading(Vec3<int> from, Vec3<int> to);
	// How far right of the road's centre line a road vehicle drives: UFO2P's straight in-tile
	// trajectories (table 0xD6A00) run at 20 and 11 of a 32-unit tile, ±4.5 about 15.5.
	static constexpr float ROAD_LANE_OFFSET = 4.5f / 32.0f;
	// FUN_00032428 turns a stopped car round when its stopped count reaches 0x3d. The count gains
	// half the speed setting a frame while the clock gains the whole setting: 122 vanilla ticks,
	// which is 488 OpenApoc ticks.
	static constexpr unsigned int ROAD_UTURN_TICKS = 488;
	// The vehicle, other than v, that stops v stepping onto the tile.
	sp<Vehicle> blockingVehicle(const Vehicle &v, Tile *to) const;
	// What a ground vehicle does about the vehicle blocking its step from a tile.
	enum class Blocked
	{
		Wait,        // for it to move on
		WalkRound,   // it is not going anywhere: get round it
		DriveThrough // it is coming the other way: pass it
	};
	Blocked respondToBlocker(const Vehicle &v, const Tile *from, const Vehicle &blocker) const;
	// Whether a ground vehicle could take the next step of its route now.
	bool nextStepIsFree(Vehicle &v) const;
	// How long a car waits behind one that is moving on before it tries the step again.
	static constexpr unsigned int QUEUE_WAIT_TICKS = 8;
	// UFO2P's 12-count wait (0x3a290) drops by half the speed setting each frame while the clock
	// gains the whole setting: 24 vanilla ticks at any speed, which is 96 OpenApoc ticks.
	static constexpr unsigned int BLOCKED_WAIT_TICKS = 96;
	// Walks round vehicles, without getting past any of them, after which a ground vehicle drives
	// through.
	static constexpr unsigned int PASS_THROUGH_AFTER_BLOCKS = 4;
	bool isTakingOff(Vehicle &v);
	int getDefaultIterationCount(Vehicle &v);
	static Vec3<float> getRandomMapEdgeCoordinates(GameState &state, StateRef<City> city);
	bool acquireTargetBuilding(GameState &state, Vehicle &v);
	// UFO2P FUN_0003a910 @ object-page file 0x2A90F: vehicle +0x171 decrements every
	// time the UFO reaches a mission destination and, at zero, either retargets or
	// clears the target. See docs/original-game/findings/U1-U2-V1-incursion.md U1(a).
	// Returns false if the mission was cancelled for lack of a new target (the
	// caller should stop without pathing further).
	bool advanceMissionCounterOnArrival(GameState &state, Vehicle &v);
	void updateTimer(unsigned ticks);
	void takePositionNearPortal(GameState &state, Vehicle &v);
	// UFO2P FUN_0003b724 @ VA 0x3B724 / file 0x2B723: zone_mode + scatter → tile XY.
	static Vec2<int> computeIncursionSpawnXY(GameState &state, int baseX, int baseY, int zoneMode,
	                                         int scatter, bool alienCity = false);
	// FUN_0006da88 @ file 0x5DB80: type_percent > 50 and scatter == 15 → scatter 10.
	static int clampIncursionScatter(int scatter, int typePercent);
	// FUN_0006da88 @ file 0x5DBBF: (constitution * type_percent) / 100 → instance +0x168.
	static int incursionTypeThreshold(int constitution, int typePercent);
	void takeIncursionSpawnPosition(GameState &state, Vehicle &v, int zoneMode, int scatter);
	static bool canRecoverVehicle(const GameState &state, const Vehicle &v, const Vehicle &target);

	// Methods to create new missions

	static VehicleMission gotoLocation(GameState &state, Vehicle &v, Vec3<int> target,
	                                   bool allowTeleporter = false, bool pickNearest = false,
	                                   int attemptsToGiveUpAfter = 20);
	static VehicleMission gotoPortal(GameState &state, Vehicle &v);
	static VehicleMission gotoPortal(GameState &state, Vehicle &v, Vec3<int> target);
	static VehicleMission departToSpace(GameState &state, Vehicle &v);
	// With now building goes home
	static VehicleMission gotoBuilding(GameState &state, Vehicle &v,
	                                   StateRef<Building> target = nullptr,
	                                   bool allowTeleporter = false);
	static VehicleMission infiltrateOrSubvertBuilding(GameState &state, Vehicle &v,
	                                                  bool subvert = false,
	                                                  StateRef<Building> target = nullptr);
	static VehicleMission attackVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	// missionCounter: UFO_mission_data +0x1B, copied to vehicle +0x171 by FUN_0006da88.
	// See advanceMissionCounterOnArrival() for the zero-transition it drives.
	static VehicleMission attackBuilding(GameState &state, Vehicle &v,
	                                     StateRef<Building> target = nullptr,
	                                     unsigned int missionCounter = 0);
	static VehicleMission followVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	static VehicleMission followVehicle(GameState &state, Vehicle &v,
	                                    std::list<StateRef<Vehicle>> &targets);
	static VehicleMission recoverVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	static VehicleMission offerService(GameState &state, Vehicle &v,
	                                   StateRef<Building> target = nullptr);
	static VehicleMission snooze(GameState &state, Vehicle &v, unsigned int ticks);
	static VehicleMission selfDestruct(GameState &state, Vehicle &v);
	static VehicleMission arriveFromDimensionGate(GameState &state, Vehicle &v, int ticks = 0,
	                                              int zoneMode = -1, int scatter = 0);
	static VehicleMission restartNextMission(GameState &state, Vehicle &v);
	static VehicleMission crashLand(GameState &state, Vehicle &v);
	static VehicleMission patrol(GameState &state, Vehicle &v, bool home = false,
	                             unsigned int counter = 10);
	static VehicleMission teleport(GameState &state, Vehicle &v, Vec3<int> target = {-1, -1, -1});
	static VehicleMission investigateBuilding(GameState &state, Vehicle &v,
	                                          StateRef<Building> target,
	                                          bool allowTeleporter = false);
	UString getName();

	enum class MissionType
	{
		GotoLocation,
		GotoBuilding,
		FollowVehicle,
		RecoverVehicle,
		AttackVehicle,
		AttackBuilding,
		RestartNextMission,
		Snooze,
		TakeOff,
		Land,
		Crash,
		Patrol,
		GotoPortal,
		InfiltrateSubvert,
		OfferService,
		Teleport,
		SelfDestruct,
		DepartToSpace,
		ArriveFromDimensionGate,
		InvestigateBuilding,
	};

	MissionType type = MissionType::GotoLocation;

	// GotoLocation InfiltrateSubvert TakeOff GotoPortal Patrol
	Vec3<int> targetLocation = {0, 0, 0};
	// GotoLocation GotoBuilding
	bool allowTeleporter = false;
	// How many times will vehicle try to re-route until it gives up
	int reRouteAttempts = 0;
	// GotoLocation - should it pick nearest point or random point if destination unreachable
	bool pickNearest = false;
	// Patrol - should patrol around home building only
	bool patrolHome = false;
	// GotoLocation - picked nearest (allows finishing mission without reaching destination)
	bool pickedNearest = false;
	// GotoBuilding AttackBuilding Land Infiltrate
	StateRef<Building> targetBuilding;
	// FollowVehicle AttackVehicle
	StateRef<Vehicle> targetVehicle;
	// FollowVehicle
	std::list<StateRef<Vehicle>> targets;
	// ArriveFromDimensionGate: UFO_mission_data tail (FUN_0006da88 → FUN_0003b724). -1 = legacy.
	int incursionZoneMode = -1;
	int incursionScatter = 0;
	// Snooze, SelfDestruct
	unsigned int timeToSnooze = 0;
	// RecoverVehicle, InfiltrateSubvert, Patrol: waypoints
	// AttackBuilding: UFO_mission_data +0x1B / vehicle +0x171 (see attackBuilding()
	// and advanceMissionCounterOnArrival()); 0 keeps the pre-existing unlimited
	// behavior at the current target.
	unsigned int missionCounter = 0;
	// InfiltrateSubvert: mode
	bool subvert = false;
	// AttackVehicle
	bool attackCrashed = false;

	bool cancelled = false;

	// Ground vehicle boxed in by other vehicles: ticks left to wait before trying the step again
	// (UFO2P's 12-count at 0x3a290). Neither re-plans meanwhile, nor moves unless the next tile of
	// its route comes free.
	unsigned int blockedWaitTicks = 0;
	// The wait above is for being boxed in, not for queueing behind a car that is moving on.
	bool boxedIn = false;
	// Road vehicle held up by another: ticks since it stopped, plus one (0 while it is moving).
	unsigned int roadStoppedTicks = 0;
	// Times blocked by a vehicle (other than queueing) since last getting past one, and the tile
	// it was last stopped from entering.
	unsigned int vehicleBlocks = 0;
	Vec3<int> blockedStep = {-1, -1, -1};

	// A deque, not a list: every mission start copies a whole route into it and every step pops
	// one tile off the front, and a list allocated and freed a node for each tile.
	std::deque<Vec3<int>> currentPlannedPath;
};
} // namespace OpenApoc
