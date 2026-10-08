#include "game/state/gamestate.h"
#include "framework/configfile.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "framework/modinfo.h"
#include "framework/options.h"
#include "game/state/battle/battle.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/facility.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/city/vequipment.h"
#include "game/state/gameevent.h"
#include "game/state/gametime.h"
#include "game/state/message.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/battle/battlecommonsamplelist.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/battlemapparttype.h"
#include "game/state/rules/battle/battleunitanimationpack.h"
#include "game/state/rules/battle/battleunitimagepack.h"
#include "game/state/rules/city/baselayout.h"
#include "game/state/rules/city/citycommonimagelist.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/rules/city/ufogrowth.h"
#include "game/state/rules/city/ufoincursion.h"
#include "game/state/rules/city/ufomissionpreference.h"
#include "game/state/rules/city/ufopaedia.h"
#include "game/state/rules/city/vammotype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/doodadtype.h"
#include "game/state/shared/organisation.h"
#include "game/state/shared/projectile.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_vehicle.h"
#include "library/strings_format.h"
#include <algorithm>
#include <ctime>
#include <random>

namespace OpenApoc
{

GameState::GameState() : player(this) {}

GameState::~GameState()
{
	if (this->current_battle)
	{
		Battle::finishBattle(*this);
		Battle::exitBattle(*this);
	}
	for (auto &a : this->agents)
	{
		a.second->destroy();
	}
	for (auto &v : this->vehicles)
	{
		auto vehicle = v.second;
		vehicle->removeFromMap(*this);
		// Detach some back-pointers otherwise we get circular sp<> dependencies and leak
		// FIXME: This is not a 'good' way of doing this, maybe add a destroyVehicle() function? Or
		// make StateRefWeak<> or something?
		//
		vehicle->city.clear();
		vehicle->homeBuilding.clear();
		vehicle->currentBuilding.clear();
		vehicle->missions.clear();
		vehicle->equipment.clear();
		vehicle->mover = nullptr;
	}
	for (auto &b : this->player_bases)
	{
		for (auto &f : b.second->facilities)
		{
			if (f->lab)
			{
				f->lab->assigned_agents.clear();
				f->lab->current_project.clear();
				f->lab.clear();
			}
		}
		b.second->building.clear();
	}
	for (auto &org : this->organisations)
	{
		org.second->current_relations.clear();
	}
	for (auto &t : this->scenery_tile_types)
	{
		// Some damaged tile links can loop, causing a leak if they're not broken
		t.second->damagedTile.clear();
	}
	for (auto &building : this->buildings)
	{
		auto &bld = building.second;
		bld->city.clear();
		bld->function.clear();
		bld->owner.clear();
		bld->base_layout.clear();
		bld->base.clear();
		bld->battle_map.clear();
		bld->preset_crew.clear();
		bld->current_crew.clear();
		bld->currentVehicles.clear();
		bld->currentAgents.clear();
		bld->researchUnlock.clear();
		bld->accessTopic.clear();
	}
}

// Just a handy shortcut since it's shown on every single screen
UString GameState::getPlayerBalance() const { return formatCurrency(this->getPlayer()->balance); }

UString GameState::formatCurrency(int64_t amount) const
{
	auto formatted = Strings::fromInteger(amount);

	if (config().getBool("OpenApoc.NewFeature.formatAsCurrency"))
		formatted = Strings::formatTextAsCurrency(formatted);

	return formatted;
}

StateRef<Organisation> GameState::getOrganisation(const UString &orgID)
{
	return StateRef<Organisation>(this, orgID);
}

const StateRef<Organisation> &GameState::getPlayer() const { return this->player; }
StateRef<Organisation> GameState::getPlayer() { return this->player; }
const StateRef<Organisation> &GameState::getAliens() const { return this->aliens; }
StateRef<Organisation> GameState::getAliens() { return this->aliens; }
const StateRef<Organisation> &GameState::getGovernment() const { return this->government; }
StateRef<Organisation> GameState::getGovernment() { return this->government; }
const StateRef<Organisation> &GameState::getCivilian() const { return this->civilian; }
StateRef<Organisation> GameState::getCivilian() { return this->civilian; }

void GameState::initState()
{
	if (current_battle)
	{
		current_battle->initBattle(*this);
	}

	// Populate persistent UFO2P base slots for older saves and repair duplicates.
	// Everything below walks references that a *loaded* save may not have resolved. A save whose
	// object graph is even slightly incomplete -- a city listing a building id that is not in the
	// state, a scenery tile whose type went missing -- turns into a null dereference at a tiny
	// offset on a thread-pool worker, which is what the crash reports from resumed campaigns all
	// look like: SIGSEGV in GameState::initState via BootUp::update. Loading a bad save should
	// fail visibly, not take the process down.
	std::array<bool, UFO2P_BASE_SLOT_COUNT> usedBaseSlots{};
	for (auto &entry : player_bases)
	{
		if (!entry.second)
		{
			LogWarning("Base \"{0}\" did not resolve while initialising state", entry.first);
			continue;
		}
		auto &slot = entry.second->ufo2pSlot;
		if (slot >= 0 && slot < UFO2P_BASE_SLOT_COUNT && !usedBaseSlots[slot])
		{
			usedBaseSlots[slot] = true;
		}
		else
		{
			slot = -1;
		}
	}
	for (auto &entry : player_bases)
	{
		if (!entry.second || entry.second->ufo2pSlot >= 0)
		{
			continue;
		}
		for (int slot = 0; slot < UFO2P_BASE_SLOT_COUNT; slot++)
		{
			if (!usedBaseSlots[slot])
			{
				entry.second->ufo2pSlot = slot;
				usedBaseSlots[slot] = true;
				break;
			}
		}
	}

	// Civilian trips are City::dispatchAmbientTraffic's. Drop the hand-written recurring patterns
	// that used to send them -- cars and bikes from every organisation, which every older save
	// carries, one of them naming a type that does not exist (VEHICLETYPE_AIRRANS).
	const auto &ambientTypes = City::ambientTrafficTypes();
	auto isAmbientPattern = [&](const Organisation::RecurringMission &m)
	{
		for (const auto &t : m.pattern.allowedTypes)
		{
			if (t.id != "VEHICLETYPE_AIRRANS" &&
			    std::find(ambientTypes.begin(), ambientTypes.end(), t.id) == ambientTypes.end())
			{
				return false;
			}
		}
		return !m.pattern.allowedTypes.empty();
	};
	for (auto &o : organisations)
	{
		if (!o.second)
		{
			continue;
		}
		for (auto &cityMissions : o.second->recurring_missions)
		{
			cityMissions.second.remove_if(isAmbientPattern);
		}
	}

	for (auto &c : this->cities)
	{
		auto &city = c.second;
		if (!city)
		{
			LogWarning("City \"{0}\" did not resolve while initialising state", c.first);
			continue;
		}
		for (auto &s : city->scenery)
		{
			if (!s || !s->type)
			{
				continue;
			}
			for (auto &b : city->buildings)
			{
				if (!b)
				{
					continue;
				}
				Vec2<int> pos2d{s->initialPosition.x, s->initialPosition.y};
				if (b->bounds.within(pos2d))
				{
					s->building = b;
					if (s->isAlive() && !s->type->commonProperty)
					{
						s->building->buildingParts.insert(s->initialPosition);
					}
					break;
				}
			}
		}
	}
	for (auto &c : this->cities)
	{
		auto &city = c.second;
		city->initCity(*this);
		if (newGame)
		{
			// if (c.first == "CITYMAP_HUMAN")
			{
				city->fillRoadSegmentMap(*this);
				city->initialSceneryLinkUp();

				// Use values provided with original maps for now
				// Uncomment this if algoritm improves
				// for (auto &b : c.second->buildings)
				//{
				//	b->initBuilding(*this);
				//}
			}
		}
		// Add vehicles to map
		for (auto &v : this->vehicles)
		{
			auto vehicle = v.second;
			if (vehicle->city == city && !vehicle->currentBuilding && !vehicle->betweenDimensions)
			{

				city->map->addObjectToMap(*this, vehicle);
			}
			vehicle->strategyImages = city_common_image_list->strategyImages;
			vehicle->setupMover();
		}
		for (auto &p : c.second->projectiles)
		{
			if (p->trackedVehicle)
				p->trackedObject = p->trackedVehicle->tileObject;
		}
		if (city->portals.empty())
		{
			city->generatePortals(*this);
		}
	}
	// Fill links for weapon's ammo
	for (auto &t : this->agent_equipment)
	{
		for (auto &w : t.second->weapon_types)
		{
			w->ammo_types.emplace(this, t.first);
		}

		// Fixing brainsucker pod store space for older saves
		if (t.first == "AEQUIPMENTTYPE_BRAINSUCKER_POD")
		{
			t.second->store_space = 1;
		}
	}

	for (auto &a : this->agent_types)
	{
		a.second->gravLiftSfx = battle_common_sample_list->gravlift;
	}
	for (auto &a : this->agents)
	{
		a.second->leftHandItem = a.second->getFirstItemInSlot(EquipmentSlotType::LeftHand, false);
		a.second->rightHandItem = a.second->getFirstItemInSlot(EquipmentSlotType::RightHand, false);
	}

	// In case this is an older savegame, check that all critical data is there
	if (this->agent_salary.empty())
	{
		this->agent_salary = {{AgentType::Role::Soldier, 600},
		                      {AgentType::Role::BioChemist, 800},
		                      {AgentType::Role::Physicist, 800},
		                      {AgentType::Role::Engineer, 800}};
	}
	if (this->agent_fired_penalty.empty())
	{
		this->agent_fired_penalty = {{AgentType::Role::Soldier, 0},
		                             {AgentType::Role::BioChemist, 0},
		                             {AgentType::Role::Physicist, 0},
		                             {AgentType::Role::Engineer, 0}};
	}
	if (this->weekly_rating_rules.empty())
	{
		this->weekly_rating_rules = {{-1600, -4}, {-800, -5}, {-400, -10}, {0, -15},  {12800, 4},
		                             {6400, 5},   {3200, 8},  {1600, 12},  {800, 16}, {400, 20}};
	}

	if (newGame)
	{
		// Initialize organization funding by running throught two-week funding
		// This lets workers to move around
		updateOrgFinances();
		updateOrgFinances();
	}

	// Run necessary methods for different types
	research.updateTopicList();
	// Apply mods (Stub until we actually have mods)
	applyMods();
	// Validate
	validate();

	skipTurboCalculations = config().getBool("OpenApoc.NewFeature.SkipTurboMovement");
}

void GameState::applyMods() {}

void GameState::setCurrentCity(StateRef<City> city)
{
	current_city = city;
	for (auto &u : current_city->researchUnlock)
	{
		u->forceComplete(this);
	}
}

void GameState::validate()
{
	LogInfo("Validating GameState");
	validateResearch();
	validateScenery();
	LogInfo("Validated GameState");
}

void GameState::validateResearch()
{
	for (auto &t : research.topics)
	{
		if (t.second->type == ResearchTopic::Type::Engineering)
		{
			if (t.second->itemId.length() == 0)
			{
				LogError("EMPTY REFERENCE resulting item for {0}", t.first);
			}
			else
			{
				bool fail = false;
				switch (t.second->item_type)
				{
					case ResearchTopic::ItemType::VehicleEquipment:
						if (vehicle_equipment.find(t.second->itemId) == vehicle_equipment.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::VehicleEquipmentAmmo:
						if (vehicle_ammo.find(t.second->itemId) == vehicle_ammo.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::AgentEquipment:
						if (agent_equipment.find(t.second->itemId) == agent_equipment.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::Craft:
						if (vehicle_types.find(t.second->itemId) == vehicle_types.end())
						{
							fail = true;
						}
						break;
				}
				if (fail)
				{
					LogError("{0} DOES NOT EXIST: referenced as manufactured by {1}",
					         t.second->itemId, t.first);
				}
			}
		}
		for (auto &rd : t.second->dependencies.research)
		{
			for (auto &topic : rd.topics)
			{
				if (topic.id.length() == 0)
				{
					LogError("EMPTY REFERENCE required topic for {0}", t.first);
				}
				else if (research.topics.find(topic.id) == research.topics.end())
				{
					LogError("{0} DOES NOT EXIST: referenced as required topic for {1}", topic.id,
					         t.first);
				}
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsRequired)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE required item for {0}", t.first);
			}
			else if (agent_equipment.find(entry.first.id) == agent_equipment.end())
			{
				LogError("{0} DOES NOT EXIST: referenced as required item for {1}", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsConsumed)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE consumed item for {0}", t.first);
			}
			else if (agent_equipment.find(entry.first.id) == agent_equipment.end())
			{
				LogError("{0} DOES NOT EXIST: referenced as consumed item for {1}", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsRequired)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE required item for {0}", t.first);
			}
			else if (vehicle_equipment.find(entry.first.id) == vehicle_equipment.end())
			{
				LogError("{0} DOES NOT EXIST: referenced as required item for {1}", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsConsumed)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE consumed item for {0}", t.first);
			}
			else if (vehicle_equipment.find(entry.first.id) == vehicle_equipment.end())
			{
				LogError("{0} DOES NOT EXIST: referenced as consumed item for {1}", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsConsumed)
		{
			if (t.second->dependencies.items.agentItemsRequired.find(entry.first) ==
			    t.second->dependencies.items.agentItemsRequired.end())
			{
				LogError("Consumed agent item {0} not in required list for topic {1}",
				         entry.first.id, t.first);
			}
			else if (t.second->dependencies.items.agentItemsRequired.at(entry.first) < entry.second)
			{
				LogError("Consumed agent items {0} has bigger count than required for topic {1}",
				         entry.first.id, t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsConsumed)
		{
			if (t.second->dependencies.items.vehicleItemsRequired.find(entry.first) ==
			    t.second->dependencies.items.vehicleItemsRequired.end())
			{
				LogError("Consumed vehicle item {0} not in required list for topic {1}",
				         entry.first.id, t.first);
			}
			else if (t.second->dependencies.items.vehicleItemsRequired.at(entry.first) <
			         entry.second)
			{
				LogError("Consumed vehicle item {0} has bigger count than required for topic {1}",
				         entry.first.id, t.first);
			}
		}
	}
}

void GameState::validateScenery()
{
	for (auto &sc : scenery_tile_types)
	{
		auto thisSc = StateRef<SceneryTileType>{this, sc.first};
		std::set<StateRef<SceneryTileType>> seenTypes;
		while (thisSc->damagedTile)
		{
			seenTypes.insert(thisSc);
			bool roadAlive = false;
			bool roadDead = false;
			bool newRoad = false;
			if (thisSc->tile_type != SceneryTileType::TileType::Road &&
			    thisSc->damagedTile->tile_type == SceneryTileType::TileType::Road)
			{
				newRoad = true;
			}
			else
			{
				for (int i = 0; i < 4; i++)
				{
					if (thisSc->connection[i] &&
					    thisSc->connection[i] == thisSc->damagedTile->connection[i])
					{
						roadAlive = true;
					}
					if (thisSc->connection[i] &&
					    thisSc->connection[i] != thisSc->damagedTile->connection[i])
					{
						roadDead = true;
					}
					if (!thisSc->connection[i] &&
					    thisSc->connection[i] != thisSc->damagedTile->connection[i])
					{
						newRoad = true;
					}
				}
			}
			if (newRoad || (roadAlive && roadDead))
			{
				LogError("ROAD MUTATION: In {0} when damaged from {1} to {2} roads go "
				         "[{3}{4}{5}{6}] "
				         "to [{7}{8}{9}{10}]",
				         sc.first, thisSc.id, thisSc->damagedTile.id, (int)thisSc->connection[0],
				         (int)thisSc->connection[1], (int)thisSc->connection[2],
				         (int)thisSc->connection[3], (int)thisSc->damagedTile->connection[0],
				         (int)thisSc->damagedTile->connection[1],
				         (int)thisSc->damagedTile->connection[2],
				         (int)thisSc->damagedTile->connection[3]);
			}
			if (seenTypes.find(thisSc->damagedTile) != seenTypes.end())
			{
				break;
			}
			thisSc = thisSc->damagedTile;
		}
	}
}

void GameState::validateAgentEquipment()
{
	for (auto &ae : agent_equipment)
	{
		if (ae.second->type == AEquipmentType::Type::Ammo)
		{
			if (ae.second->max_ammo == 0)
			{
				LogError(
				    "{0} ZERO MAX AMMO: equipment of type ammo must always have non-zero max ammo",
				    ae.first);
			}
			if (ae.second->max_ammo != 1 && ae.second->bioStorage)
			{
				LogError("{0} BIO AMMO CLIP: equipment stored in alien containment must never have "
				         "max ammo other than 1",
				         ae.first);
			}
		}
	}
}

void GameState::fillOrgStartingProperty()
{
	for (auto &o : this->organisations)
	{
		o.second->updateVehicleAgentPark(*this);
		o.second->updateHirableAgents(*this);
		for (auto &m : o.second->recurring_missions[{this, "CITYMAP_HUMAN"}])
		{
			m.time +=
			    gameTime.getTicks() +
			    randBoundsInclusive(rng, (uint64_t)0,
			                        m.pattern.maxIntervalRepeat - m.pattern.minIntervalRepeat) -
			    m.pattern.minIntervalRepeat / 2;
		}
	}
}

void GameState::startGame()
{
	// An explicit seed wins over everything: it is what makes a run reproducible on demand and
	// still freely variable, which comparing two AIs over the same campaign requires. Zero keeps
	// the previous behaviour exactly, so nothing that did not ask for a seed changes.
	const auto explicitSeed = config().getInt("OpenApoc.NewFeature.RngSeed");
	if (explicitSeed != 0)
	{
		LogInfo("Seeding game RNG with explicit seed {0}", explicitSeed);
		rng.seed(static_cast<uint64_t>(explicitSeed));
	}
	else if (config().getBool("OpenApoc.NewFeature.SeedRng"))
	{
		const auto seed = static_cast<uint64_t>(std::time(nullptr));
		LogInfo("Seeding game RNG with {0}", seed);
		rng.seed(seed);
	}

	agentEquipmentTemplates.resize(10);

	// Setup orgs
	for (auto &pair : this->organisations)
	{
		pair.second->ticksTakeOverAttemptAccumulated =
		    randBoundsExclusive(rng, (unsigned)0, TICKS_PER_TAKEOVER_ATTEMPT);
		// Initial relationship randomiser
		// Not for player or civilians
		if (pair.first == player.id || pair.first == civilian.id)
		{
			continue;
		}
		for (auto &entry : pair.second->current_relations)
		{
			// Not for civilians or perfect relationships
			if (entry.second == 100.0f || entry.first == civilian)
			{
				continue;
			}
			// First step: adjust based on difficulty
			// higher difficulty will produce a bigger sway
			if (difficulty > 0)
			{
				// Relationship vs player is adjusted by flat 0/5/0/-5/-10
				if (entry.first == player)
				{
					entry.second += 10 - 5 * difficulty;
				}
				// Positive relationship is improved randomly
				else if (entry.second >= 0.0f)
				{
					entry.second += randBoundsInclusive(rng, 0, 3 * difficulty);
				}
				// Negative relationship with non-aliens is worsened randomly
				else if (entry.first != aliens)
				{
					entry.second -= randBoundsInclusive(rng, 0, 5 * difficulty);
				}
			}
			// Second step: random +- 10
			entry.second += randBoundsInclusive(rng, -10, 10);

			// Finally stay in bounds
			entry.second = clamp(entry.second, -100.0f, 100.0f);
			// Sync up long-term value for initial relationships, and seed the daily snapshot so
			// day one does not read as a fall from 0 (raid pressure, the player's daily delta).
			pair.second->long_term_relations[entry.first] = entry.second;
			pair.second->previous_relations[entry.first] = entry.second;

			// Set player reverse relationships
			if (entry.first == getPlayer())
			{
				getPlayer()->current_relations[{this, pair.first}] = entry.second;
			}
		}
	}

	// Setup buildings
	for (auto &pair : this->cities)
	{
		for (auto &b : pair.second->buildings)
		{
			b->ticksDetectionAttemptAccumulated =
			    randBoundsExclusive(rng, (unsigned)0, TICKS_PER_DETECTION_ATTEMPT[difficulty]);
		}
	}
	// Setup scenery
	for (auto &pair : this->cities)
	{
		auto &city = pair.second;
		// Start the game with all buildings whole
		for (auto &tilePair : city->initial_tiles)
		{
			auto s = mksp<Scenery>();

			s->type = tilePair.second;
			s->initialPosition = tilePair.first;
			s->currentPosition = s->initialPosition;

			city->scenery.push_back(s);
		}
	}

	// Add aliens into random building
	int counter = 0;
	int giveUpCount = 100;
	auto buildingIt = this->cities["CITYMAP_HUMAN"]->buildings.begin();
	do
	{
		int buildID =
		    randBoundsExclusive(rng, 0, (int)this->cities["CITYMAP_HUMAN"]->buildings.size());
		buildingIt = this->cities["CITYMAP_HUMAN"]->buildings.begin();
		for (int i = 0; i < buildID; i++)
		{
			buildingIt++;
		}
		counter++;
	} while ((*buildingIt)->owner->current_relations[player] < 0 || counter >= giveUpCount);

	for (auto &l : initial_aliens.at(difficulty))
	{
		(*buildingIt)->current_crew[l.first] = randBoundsExclusive(rng, l.second.x, l.second.y);
	}
	(*buildingIt)->initialInfiltration = true;

	gameTime = GameTime::midday();

	updateEndOfWeek(true);

	newGame = true;
	firstDetection = true;
	nextInvasion =
	    gameTime.getTicks() +
	    vanillaInvasionDelayTicks(randBoundsInclusive(rng, 0, INVASION_DELAY_MINUTE_MAX),
	                              randBoundsInclusive(rng, 0, INVASION_DELAY_SECOND_MAX));
}

// Fills out initial player property
void GameState::fillPlayerStartingProperty()
{
	// Create the initial starting base
	// Randomly shuffle buildings until we find one with a base layout
	sp<City> humanCity = this->cities["CITYMAP_HUMAN"];
	setCurrentCity({this, humanCity});

	std::vector<sp<Building>> buildingsWithBases;
	for (auto &b : humanCity->buildings)
	{
		if (b->base_layout && !b->initialInfiltration)
			buildingsWithBases.push_back(b.getSp());
	}

	if (buildingsWithBases.empty())
	{
		LogError("City map has no buildings with valid base layouts");
	}

	std::uniform_int_distribution<int> bldDist(0, buildingsWithBases.size() - 1);

	auto bld = buildingsWithBases[bldDist(this->rng)];

	auto base = mksp<Base>(*this, StateRef<Building>{this, bld});
	base->startingBase(*this);
	base->name = "Base " + Strings::fromInteger(this->player_bases.size() + 1);
	this->player_bases[Base::getPrefix() + Strings::fromInteger(this->player_bases.size() + 1)] =
	    base;
	bld->owner = this->getPlayer();
	bld->base = {this, base};
	this->current_base = {this, base};

	// Give the player one of each equipable vehicle
	/*for (auto &it : this->vehicle_types)
	{
	    auto &type = it.second;
	    if (!type->equipment_screen)
	        continue;
	    auto v = current_city->placeVehicle(*this, {this, type}, this->getPlayer(), {this, bld});
	    v->homeBuilding = v->currentBuilding;
	}*/
	for (auto &pair : this->initial_vehicles)
	{
		auto v = current_city->createVehicle(*this, pair.first, this->getPlayer(), {this, bld});
		v->homeBuilding = v->currentBuilding;
		for (auto &eq : pair.second)
		{
			auto device = v->addEquipment(*this, eq);
			device->ammo = eq->max_ammo;
		}
	}
	// Give the player initial vehicle equipment
	for (auto &pair : this->initial_vehicle_equipment)
	{
		base->inventoryVehicleEquipment[pair.first.id] = pair.second;
	}
	// Give the player initial vehicle ammo
	for (auto &pair : this->initial_vehicle_ammo)
	{
		base->inventoryVehicleAmmo[pair.first.id] = pair.second;
	}
	// Give base starting agent equipment
	for (auto &pair : this->initial_base_agent_equipment)
	{
		auto &equipmentID = pair.first;
		base->inventoryAgentEquipment[equipmentID] = pair.second;
	}
	// Give starting agents and their equipment
	for (auto &agentTypePair : this->initial_agents)
	{
		auto type = agentTypePair.first;
		auto count = agentTypePair.second;
		auto it = initial_agent_equipment.begin();
		while (count > 0)
		{
			auto agent = this->agent_generator.createInitAgent(*this, this->getPlayer(), type);
			if (agent->type->canTrain)
			{
				agent->trainingAssignment = agent->initial_stats.psi_energy > 30
				                                ? TrainingAssignment::Psi
				                                : TrainingAssignment::Physical;
			}
			agent->homeBuilding = base->building;
			agent->city = agent->homeBuilding->city;
			agent->enterBuilding(*this, agent->homeBuilding);
			count--;
			if (type == AgentType::Role::Soldier && it != initial_agent_equipment.end())
			{
				for (auto &t : *it)
				{
					if (t->type == AEquipmentType::Type::Armor)
					{
						EquipmentSlotType slotType = EquipmentSlotType::General;
						switch (t->body_part)
						{
							case BodyPart::Body:
								slotType = EquipmentSlotType::ArmorBody;
								break;
							case BodyPart::Legs:
								slotType = EquipmentSlotType::ArmorLegs;
								break;
							case BodyPart::Helmet:
								slotType = EquipmentSlotType::ArmorHelmet;
								break;
							case BodyPart::LeftArm:
								slotType = EquipmentSlotType::ArmorLeftHand;
								break;
							case BodyPart::RightArm:
								slotType = EquipmentSlotType::ArmorRightHand;
								break;
						}
						agent->addEquipmentByType(*this, {this, t->id}, slotType, false);
					}
					else if (t->type == AEquipmentType::Type::Ammo ||
					         t->type == AEquipmentType::Type::MediKit ||
					         t->type == AEquipmentType::Type::Grenade)
					{
						agent->addEquipmentByType(*this, {this, t->id}, EquipmentSlotType::General,
						                          false);
					}
					else
					{
						agent->addEquipmentByType(*this, {this, t->id}, false);
					}
				}
				it++;
			}
		}
	}

	// Start player centered on base
	auto bldBounds = bld->bounds;

	Vec2<int> buildingCenter = (bldBounds.p0 + bldBounds.p1) / 2;
	bld->city->cityViewScreenCenter = {buildingCenter.x, buildingCenter.y, 1.0f};
}

int GameState::allocateUfo2pBaseSlot() const
{
	std::array<bool, UFO2P_BASE_SLOT_COUNT> used{};
	for (const auto &entry : player_bases)
	{
		if (entry.second && entry.second->ufo2pSlot >= 0 &&
		    entry.second->ufo2pSlot < UFO2P_BASE_SLOT_COUNT)
		{
			used[entry.second->ufo2pSlot] = true;
		}
	}
	for (int slot = 0; slot < UFO2P_BASE_SLOT_COUNT; slot++)
	{
		if (!used[slot])
		{
			return slot;
		}
	}
	return -1;
}

int GameState::selectKnownBaseSlot(const std::array<bool, UFO2P_BASE_SLOT_COUNT> &active,
                                   const std::array<bool, UFO2P_BASE_SLOT_COUNT> &knownToAliens,
                                   int startSlot)
{
	if (startSlot < 0 || startSlot >= UFO2P_BASE_SLOT_COUNT)
	{
		return -1;
	}
	for (int scanned = 0; scanned < UFO2P_BASE_SLOT_COUNT; scanned++)
	{
		const int slot = (startSlot + scanned) % UFO2P_BASE_SLOT_COUNT;
		if (active[slot] && knownToAliens[slot])
		{
			return slot;
		}
	}
	return -1;
}

void GameState::invasion()
{
	auto invadedCity = StateRef<City>{this, "CITYMAP_HUMAN"};
	if (current_city != invadedCity)
	{
		nextInvasion += TICKS_PER_MINUTE;
		return;
	}
	nextInvasion =
	    gameTime.getTicks() +
	    vanillaInvasionDelayTicks(randBoundsInclusive(rng, 0, INVASION_DELAY_MINUTE_MAX),
	                              randBoundsInclusive(rng, 0, INVASION_DELAY_SECOND_MAX));

	auto invadingCity = StateRef<City>{this, "CITYMAP_ALIEN"};
	auto invadingOrg = StateRef<Organisation>{this, "ORG_ALIEN"};

	// Set a list of possible participants
	std::map<UString, int> vehicleLimits;
	std::map<UString, std::list<sp<Vehicle>>> invaders;
	for (auto &v : vehicles)
	{
		if (v.second->owner == invadingOrg && v.second->city == invadingCity)
		{
			vehicleLimits[v.second->type.id]++;
			invaders[v.second->type.id].push_back(v.second);
		}
	}
	// Select a random mission type
	int week = this->gameTime.getWeek();
	auto preference = this->ufo_mission_preference.find(
	    format("{0}{1}", UFOMissionPreference::getPrefix(), week));
	if (preference == this->ufo_mission_preference.end())
	{
		preference = this->ufo_mission_preference.find(
		    format("{0}{1}", UFOMissionPreference::getPrefix(), "DEFAULT"));
	}
	if (preference == this->ufo_mission_preference.end() || !preference->second ||
	    preference->second->missionList.empty())
	{
		LogWarning("No UFO mission preference for week {0}; skipping invasion", week);
		return;
	}
	auto missionType = pickRandom(rng, preference->second->missionList);
	// Compile list of missions rated by priority
	std::map<int, sp<UFOIncursion>> incursions;
	for (auto &e : ufo_incursions)
	{
		if (e.second->primaryMission == missionType)
		{
			incursions[e.second->priority] = e.second;
		}
	}
	// Find first incursion by type that fits
	sp<UFOIncursion> currentIncursion;
	for (auto &inc : incursions)
	{
		auto limits = vehicleLimits;
		for (auto &v : inc.second->primaryList)
		{
			limits[v.first] -= v.second;
		}
		for (auto &v : inc.second->attackList)
		{
			limits[v.first] -= v.second;
		}
		for (auto &v : inc.second->escortList)
		{
			limits[v.first] -= v.second;
		}
		bool enoughVehicles = true;
		for (auto &v : limits)
		{
			if (v.second < 0)
			{
				enoughVehicles = false;
				break;
			}
		}
		if (enoughVehicles)
		{
			currentIncursion = inc.second;
			break;
		}
	}
	if (!currentIncursion)
	{
		return;
	}

	std::array<StateRef<Base>, UFO2P_BASE_SLOT_COUNT> baseSlots;
	std::array<bool, UFO2P_BASE_SLOT_COUNT> activeBases{};
	std::array<bool, UFO2P_BASE_SLOT_COUNT> knownBases{};
	bool preferredKnownBaseUsed = false;
	if (missionType == UFOIncursion::PrimaryMission::Subversion)
	{
		for (auto &entry : player_bases)
		{
			const int slot = entry.second ? entry.second->ufo2pSlot : -1;
			if (slot < 0 || slot >= UFO2P_BASE_SLOT_COUNT || !entry.second ||
			    !entry.second->building)
			{
				continue;
			}
			baseSlots[slot] = {this, entry.first};
			activeBases[slot] = entry.second->building->isAlive();
			knownBases[slot] = entry.second->knownToAliens;
		}
	}

	std::set<StateRef<Vehicle>> escorted;
	for (size_t primaryIdx = 0; primaryIdx < currentIncursion->primaryList.size(); primaryIdx++)
	{
		auto &v = currentIncursion->primaryList[primaryIdx];
		int zoneMode = -1;
		int scatter = 0;
		unsigned int missionCounter = 0;
		int withdrawPercent = 0;
		if (primaryIdx < currentIncursion->primarySlots.size())
		{
			const auto &slot = currentIncursion->primarySlots[primaryIdx];
			withdrawPercent = UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[slot.role & 0xF];
			zoneMode = slot.zoneMode;
			scatter = VehicleMission::clampIncursionScatter(slot.scatter, slot.typePercent);
			missionCounter = slot.missionCounter;
		}
		for (int i = 0; i < v.second; i++)
		{
			auto invader = invaders[v.first].front();
			invaders[v.first].pop_front();

			invader->withdrawHealthPercent = withdrawPercent;
			invader->enterDimensionGate(*this);
			invader->equipDefaultEquipment(*this);
			invader->city = invadedCity;
			invader->setMission(*this, VehicleMission::arriveFromDimensionGate(*this, *invader, 0,
			                                                                   zoneMode, scatter));
			switch (missionType)
			{
				case UFOIncursion::PrimaryMission::Attack:
					invader->addMission(
					    *this,
					    VehicleMission::attackBuilding(*this, *invader, nullptr, missionCounter),
					    true);
					break;
				case UFOIncursion::PrimaryMission::Infiltration:
					invader->addMission(
					    *this, VehicleMission::infiltrateOrSubvertBuilding(*this, *invader, false),
					    true);
					break;
				case UFOIncursion::PrimaryMission::Subversion:
				{
					// FUN_000702e4 consumes rand16(15) for every role-2 craft,
					// before consulting the one-preferred-target latch.
					const int startSlot = randBoundsInclusive(rng, 0, UFO2P_BASE_SLOT_COUNT - 1);
					const int slot = selectKnownBaseSlot(activeBases, knownBases, startSlot);
					StateRef<Building> preferredKnownBase;
					if (!preferredKnownBaseUsed && slot >= 0)
					{
						preferredKnownBase = baseSlots[slot]->building;
						preferredKnownBaseUsed = true;
					}
					invader->addMission(*this,
					                    VehicleMission::infiltrateOrSubvertBuilding(
					                        *this, *invader, true, preferredKnownBase),
					                    true);
					break;
				}
				case UFOIncursion::PrimaryMission::Overspawn:
					// Overspawn dumps aliens into buildings rather than bombing them.
					// Dedicated attackers still come from attackList below.
					invader->addMission(
					    *this, VehicleMission::infiltrateOrSubvertBuilding(*this, *invader, false),
					    true);
					break;
			}
			escorted.emplace(this, invader);
		}
	}
	for (size_t escortIdx = 0; escortIdx < currentIncursion->escortList.size(); escortIdx++)
	{
		auto &v = currentIncursion->escortList[escortIdx];
		UString followType;
		int withdrawPercent = 0;
		int zoneMode = -1;
		int scatter = 0;
		if (escortIdx < currentIncursion->escortSlots.size())
		{
			const auto &slot = currentIncursion->escortSlots[escortIdx];
			withdrawPercent = UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[slot.role & 0xF];
			followType = slot.followVehicleType;
			zoneMode = slot.zoneMode;
			scatter = VehicleMission::clampIncursionScatter(slot.scatter, slot.typePercent);
		}
		for (int i = 0; i < v.second; i++)
		{
			auto invader = invaders[v.first].front();
			invaders[v.first].pop_front();

			invader->withdrawHealthPercent = withdrawPercent;
			invader->enterDimensionGate(*this);
			invader->city = invadedCity;
			invader->setMission(*this, VehicleMission::arriveFromDimensionGate(*this, *invader, 0,
			                                                                   zoneMode, scatter));
			// FUN_0006da88 stores craft[follow_slot]; FUN_00059148 matches that type.
			// follow_slot 0xFFFF leaves followVehicleType empty — no FollowVehicle.
			if (followType.empty())
			{
				continue;
			}
			std::set<StateRef<Vehicle>> followCopy;
			for (auto &e : escorted)
			{
				if (e && e->type.id == followType)
				{
					followCopy.emplace(e);
				}
			}
			std::list<StateRef<Vehicle>> followRandomized;
			while (!followCopy.empty())
			{
				auto item = pickRandom(rng, followCopy);
				followCopy.erase(item);
				followRandomized.push_back(item);
			}
			if (!followRandomized.empty())
			{
				invader->addMission(
				    *this, VehicleMission::followVehicle(*this, *invader, followRandomized), true);
			}
		}
	}
	for (size_t attackIdx = 0; attackIdx < currentIncursion->attackList.size(); attackIdx++)
	{
		auto &v = currentIncursion->attackList[attackIdx];
		int zoneMode = -1;
		int scatter = 0;
		unsigned int missionCounter = 0;
		int withdrawPercent = 0;
		if (attackIdx < currentIncursion->attackSlots.size())
		{
			const auto &slot = currentIncursion->attackSlots[attackIdx];
			withdrawPercent = UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[slot.role & 0xF];
			zoneMode = slot.zoneMode;
			scatter = VehicleMission::clampIncursionScatter(slot.scatter, slot.typePercent);
			missionCounter = slot.missionCounter;
		}
		for (int i = 0; i < v.second; i++)
		{
			auto invader = invaders[v.first].front();
			invaders[v.first].pop_front();

			invader->withdrawHealthPercent = withdrawPercent;
			invader->enterDimensionGate(*this);
			invader->city = invadedCity;
			invader->setMission(*this, VehicleMission::arriveFromDimensionGate(*this, *invader, 0,
			                                                                   zoneMode, scatter));
			if (missionType == UFOIncursion::PrimaryMission::Overspawn)
			{
				invader->addMission(
				    *this, VehicleMission::infiltrateOrSubvertBuilding(*this, *invader, false),
				    true);
			}
			else
			{
				invader->addMission(
				    *this, VehicleMission::attackBuilding(*this, *invader, nullptr, missionCounter),
				    true);
			}
		}
	}
}

bool GameState::canTurbo() const
{
	if (!this->current_city->projectiles.empty())
	{
		return false;
	}
	// The city's StateRef is compared by object and hostility decided once per owner: this runs
	// every frame, and comparing StateRefs compares id strings.
	const City *thisCity = &*this->current_city;
	const auto player = this->getPlayer();
	for (const auto &group : this->getVehiclesByOwner())
	{
		const bool hostileOwner = group.vehicles.front().second->owner->isRelatedTo(player) ==
		                          Organisation::Relation::Hostile;
		for (const auto &entry : group.vehicles)
		{
			const auto &v = entry.second;
			if (v->isDead() || !v->city || &*v->city != thisCity || v->tileObject == nullptr)
			{
				continue;
			}
			if (hostileOwner && v->type->aggressiveness > 0 && !v->crashed)
			{
				return false;
			}
			for (auto &m : v->missions)
			{
				if (m.type == VehicleMission::MissionType::AttackBuilding ||
				    m.type == VehicleMission::MissionType::AttackVehicle)
				{
					return false;
				}
			}
		}
	}
	return true;
}

/**
 * Immediately remove all dead objects.
 */
void OpenApoc::GameState::cleanUpDeathNote()
{
	// Any additional death notes should processed here.
	if (!vehiclesDeathNote.empty())
	{
		vehiclesChanged();
		for (auto &name : this->vehiclesDeathNote)
		{
			vehicles.erase(name);

			// Remove vehicle from selection
			for (const auto &[cityId, city] : cities)
			{
				for (auto it = city->cityViewSelectedOwnedVehicles.begin();
				     it != city->cityViewSelectedOwnedVehicles.end();)
				{
					if (it->id == name)
					{
						it = city->cityViewSelectedOwnedVehicles.erase(it);
					}
					else
					{
						++it;
					}
				}
				for (auto it = city->cityViewSelectedOtherVehicles.begin();
				     it != city->cityViewSelectedOtherVehicles.end();)
				{
					if (it->id == name)
					{
						it = city->cityViewSelectedOtherVehicles.erase(it);
					}
					else
					{
						++it;
					}
				}
			}
		}
		vehiclesDeathNote.clear();
	}

	if (!agentsDeathNote.empty())
	{
		for (auto &name : this->agentsDeathNote)
		{
			agents.erase(name);

			// Remove from selection
			for (const auto &[cityId, city] : cities)
			{
				for (auto it = city->cityViewSelectedBios.begin();
				     it != city->cityViewSelectedBios.end();)
				{
					if (it->id == name)
					{
						it = city->cityViewSelectedBios.erase(it);
					}
					else
					{
						++it;
					}
				}
				for (auto it = city->cityViewSelectedPhysics.begin();
				     it != city->cityViewSelectedPhysics.end();)
				{
					if (it->id == name)
					{
						it = city->cityViewSelectedPhysics.erase(it);
					}
					else
					{
						++it;
					}
				}
				for (auto it = city->cityViewSelectedEngineers.begin();
				     it != city->cityViewSelectedEngineers.end();)
				{
					if (it->id == name)
					{
						it = city->cityViewSelectedEngineers.erase(it);
					}
					else
					{
						++it;
					}
				}
			}
		}
		agentsDeathNote.clear();
	}
}

const std::vector<GameState::OwnedVehicles> &GameState::getVehiclesByOwner() const
{
	if (vehiclesByOwnerCount != vehicles.size())
	{
		vehiclesByOwner.clear();
		unsigned index = 0;
		for (const auto &[id, vehicle] : vehicles)
		{
			const Organisation *owner = vehicle->owner.get();
			auto group = std::find_if(vehiclesByOwner.begin(), vehiclesByOwner.end(),
			                          [owner](const OwnedVehicles &g) { return g.owner == owner; });
			if (group == vehiclesByOwner.end())
			{
				group = vehiclesByOwner.insert(group, OwnedVehicles{owner, {}});
			}
			group->vehicles.emplace_back(index++, vehicle);
		}
		vehiclesByOwnerCount = vehicles.size();
	}
	return vehiclesByOwner;
}

void GameState::update(unsigned int ticks)
{
	if (this->current_battle)
	{
		this->current_battle->update(*this, ticks);
		gameTime.addTicks(ticks);
	}
	else
	{
		// Roll back to time before battle and stuff
		if (gameTimeBeforeBattle.getTicks() != 0)
		{
			updateAfterBattle();
		}

		if (current_city.id == "CITYMAP_HUMAN" && ticks > City::AMBIENT_TRAFFIC_TICKS)
		{
			// Speed 5 crosses ten traffic intervals in one update. Advance the city between
			// batches so completed trips release slots and each draw uses its own clock/hour.
			// Ordinary city steps and battle updates retain their existing cadence.
			while (ticks > 0)
			{
				const unsigned untilTraffic =
				    City::AMBIENT_TRAFFIC_TICKS - gameTime.getTicks() % City::AMBIENT_TRAFFIC_TICKS;
				const unsigned step = std::min(ticks, untilTraffic);
				update(step);
				ticks -= step;
			}
			return;
		}

		current_city->update(*this, ticks);

		// What a rescue craft could go for (VehicleMission::canRecoverVehicle's target test),
		// found once here rather than by every organisation walking every vehicle.
		std::vector<std::pair<UString, sp<Vehicle>>> rescueCandidates;
		for (auto &v : this->vehicles)
		{
			const auto &veh = v.second;
			if (!veh->isDead() && (veh->crashed || veh->sliding || veh->falling) &&
			    !veh->carriedByVehicle)
			{
				rescueCandidates.emplace_back(v.first, veh);
			}
		}
		for (auto &o : this->organisations)
		{
			o.second->updateMissions(*this, rescueCandidates);
		}

		for (auto &v : this->vehicles)
		{
			if (v.second->city == current_city)
			{
				// A reference, not a copy: this runs for every vehicle every tick, and copying the
				// list copied each mission's whole planned path just to look at the last one.
				const auto &vehicleMission = v.second->missions;
				if (!vehicleMission.empty() &&
				    vehicleMission.back().type == VehicleMission::MissionType::AttackVehicle &&
				    (vehicleMission.back().targetVehicle == nullptr ||
				     vehicleMission.back().targetVehicle->city != current_city))
				{
					v.second->clearMissions(*this);
					v.second->addMission(*this, VehicleMission::gotoBuilding(
					                                *this, *v.second, v.second->homeBuilding));
				}
				v.second->update(*this, ticks);
			}
		}

		for (auto &a : this->agents)
		{
			if (a.second->city == current_city)
			{
				a.second->update(*this, ticks);
			}
		}

		cleanUpDeathNote();

		// UFO2P counts its traffic down in the human city only (FUN_0006d384).
		if (current_city.id == "CITYMAP_HUMAN" &&
		    GameTime::intervalsCrossed(gameTime.getTicks(), ticks, City::AMBIENT_TRAFFIC_TICKS))
		{
			current_city->dispatchAmbientTraffic(*this);
		}

		const uint64_t secondsCrossed =
		    GameTime::intervalsCrossed(gameTime.getTicks(), ticks, TICKS_PER_SECOND);
		gameTime.addTicks(ticks);

		if (gameTime.getTicks() > nextInvasion)
		{
			invasion();
		}
		// Once per second crossed, not once per call: a turbo step crosses 300 seconds, and the
		// per-second work is fuel burn, so one call per step let craft fly turbo on 1/300th of
		// their fuel. The per-second hooks accumulate and are safe to repeat at one timestamp.
		for (uint64_t s = 0; s < secondsCrossed; s++)
		{
			this->updateEndOfSecond();
		}
		if (gameTime.fiveMinutesPassed())
		{
			this->updateEndOfFiveMinutes();
		}
		if (gameTime.hourPassed())
		{
			this->updateEndOfHour();
		}
		if (gameTime.dayPassed())
		{
			this->updateEndOfDay();
		}
		if (gameTime.weekPassed())
		{
			this->updateEndOfWeek(false);
		}
		gameTime.clearFlags();

		// Call again in case any of periodic updates added items to death note list
		// TBD: unify mark-and-sweep StateObject into singe system
		cleanUpDeathNote();
	}
}

void GameState::updateEndOfSecond()
{
	for (auto &b : current_city->buildings)
	{
		b->updateCargo(*this);
	}
	for (auto &v : vehicles)
	{
		if (v.second->city == current_city)
		{
			v.second->updateEachSecond(*this);
		}
	}
	for (auto &a : this->agents)
	{
		if (a.second->city == current_city)
		{
			a.second->updateEachSecond(*this);
		}
	}
}

void GameState::updateEndOfFiveMinutes()
{
	// TakeOver calculation stops when org is taken over
	for (auto &o : this->organisations)
	{
		if (o.second->takenOver)
		{
			continue;
		}
		o.second->updateTakeOver(*this, TICKS_PER_MINUTE * 5);
		if (o.second->takenOver)
		{
			break;
		}
	}

	for (auto &b : current_city->buildings)
	{
		if (!b->base || b->owner != getPlayer())
		{
			continue;
		}

		auto base = b->base;
		for (auto it = b->currentVehicles.begin(); it != b->currentVehicles.end();)
		{
			auto v = *it;
			if (this->vehicles.find(v.id) == this->vehicles.end())
			{
				LogWarning("{0} not found, but removal was successful..", v.id);
				v.clear();
				it = b->currentVehicles.erase(it);
				continue;
			}

			for (auto &e : v->equipment)
			{
				// We only can reload VehicleWeapon and VehicleEngine(?)
				if (e->type->type != EquipmentSlotType::VehicleWeapon &&
				    e->type->type != EquipmentSlotType::VehicleEngine) //  e->type->max_ammo == 0
				{
					continue;
				}
				// Only show events for vehicles owned by current player
				if (v->owner->name == getPlayer()->name)
				{
					if (e->reload(*this, base))
					{
						switch (e->type->type)
						{
							case EquipmentSlotType::VehicleEngine:
								fw().pushEvent(
								    new GameVehicleEvent(GameEventType::VehicleRefuelled, v));
								break;
							case EquipmentSlotType::VehicleWeapon:
								fw().pushEvent(
								    new GameVehicleEvent(GameEventType::VehicleRearmed, v));
								break;
							default:
								LogInfo("Implement the remaining messages for vehicle rearmed / "
								        "reloaded / refueled / whatever");
								break;
						}
					}
				}
			}

			++it;
		}
	}

	// Detection calculation stops when detection happens
	for (auto &b : current_city->buildings)
	{
		bool detected = b->ticksDetectionTimeOut > 0;
		b->updateDetection(*this, TICKS_PER_MINUTE * 5);
		if (b->ticksDetectionTimeOut > 0 && !detected)
		{
			break;
		}
	}
}

void GameState::updateEndOfHour()
{
	for (auto &a : this->agents)
	{
		a.second->updateHourly(*this);
	}
	for (auto &lab : this->research.labs)
	{
		Lab::update(TICKS_PER_HOUR, {this, lab.second}, shared_from_this());
	}
	for (auto &c : this->cities)
	{
		c.second->hourlyLoop(*this);
	}
	for (auto &o : this->organisations)
	{
		o.second->updateInfiltration(*this);
	}
}

void GameState::updateEndOfDay()
{
	for (auto &b : this->player_bases)
	{
		for (auto &f : b.second->facilities)
		{
			if (f->buildTime > 0)
			{
				f->buildTime--;
				if (f->buildTime == 0)
				{
					fw().pushEvent(
					    new GameFacilityEvent(GameEventType::FacilityCompleted, b.second, f));
				}
			}
		}
	}
	for (auto &o : this->organisations)
	{
		o.second->updateVehicleAgentPark(*this);
		o.second->updateHirableAgents(*this);
		o.second->updateDailyInfiltrationHistory();
		if (o.second->initiatesDiplomacy)
		{
			// Must run before updateRelations overwrites long_term with current.
			o.second->setRaidMissions(*this, current_city);
		}
		const float relationshipDelta = o.second->updateRelations(player);

		if (o.second->initiatesDiplomacy)
		{
			if (relationshipDelta < -15 && !o.second->takenOver &&
			    o.second->getRelationTo(player) < 25 &&
			    randBoundsInclusive(rng, 0, 100) > (difficulty + 1) * 10)
			{
				fw().pushEvent(new GameOrganisationEvent(GameEventType::OrganisationRequestBribe,
				                                         {this, o.first}));
			}
		}
	}
	for (auto &a : this->agents)
	{
		a.second->updateDaily(*this);
	}
	for (auto &c : this->cities)
	{
		c.second->dailyLoop(*this);
	}

	// Check if today is the first day of the week (monday).
	// In that case, do not show the daily report as it's already part of the weekly report
	// event
	if (this->gameTime.getMonthDay() != this->gameTime.getFirstDayOfCurrentWeek())
		fw().pushEvent(new GameEvent(GameEventType::DailyReport));
}

// Spawns alien reinforcements in CITYMAP_ALIEN based on the weekly UFO_GROWTH_<week> list,
// capped by UFO_GROWTH_LIMIT minus the alien fleet already present.
void GameState::updateUfoGrowth()
{
	const int week = static_cast<int>(this->gameTime.getWeek());
	const auto growth = UFOGrowth::selectForWeek(*this, week);
	if (!growth)
	{
		LogWarning("No valid UFO growth lists found");
		return;
	}
	if (!UFOGrowth::craftFactoryIntact(*this))
	{
		return;
	}

	const auto limitIt = this->ufo_growth_lists.find("UFO_GROWTH_LIMIT");
	if (limitIt == this->ufo_growth_lists.end())
	{
		return;
	}
	const auto &limit = limitIt->second;

	StateRef<City> alienCity = {this, "CITYMAP_ALIEN"};
	if (!alienCity)
	{
		LogError("updateUfoGrowth: CITYMAP_ALIEN not found");
		return;
	}
	StateRef<Organisation> alienOrg = {this, "ORG_ALIEN"};

	// Start with the per-type cap from UFO_GROWTH_LIMIT, then subtract the alien fleet
	// already present in CITYMAP_ALIEN to get the remaining spawn allowance.
	std::map<UString, int> vehicleAllowance;
	for (const auto &entry : limit->vehicleTypeList)
	{
		vehicleAllowance[entry.first] += entry.second;
	}
	for (const auto &vp : this->vehicles)
	{
		const auto &v = vp.second;
		if (v->owner == alienOrg && v->city == alienCity)
		{
			vehicleAllowance[v->type.id] -= 1;
		}
	}

	for (const auto &entry : growth->vehicleTypeList)
	{
		const UString &vtId = entry.first;
		const int requested = entry.second;

		if (this->vehicle_types.find(vtId) == this->vehicle_types.end())
		{
			continue;
		}
		const int toAdd = std::min(requested, vehicleAllowance[vtId]);
		if (toAdd <= 0)
		{
			continue;
		}

		StateRef<VehicleType> vt = {this, vtId};
		for (int i = 0; i < toAdd; i++)
		{
			const Vec3<float> pos = {
			    static_cast<float>(randBoundsExclusive(rng, 20, 120)),
			    static_cast<float>(randBoundsExclusive(rng, 20, 120)),
			    static_cast<float>(alienCity->size.z - 1),
			};
			alienCity->placeVehicle(*this, vt, alienOrg, pos, 0.0f);
		}
	}
}

// Runs the weekly market simulation across vehicle_types, vehicle_equipment, vehicle_ammo
// and agent_equipment, updating stock and price per item via EconomyInfo::update.
void GameState::updateItemMarket()
{
	std::vector<UString> newItems;

	const auto processMap = [this, &newItems](const auto &map)
	{
		for (const auto &entry : map)
		{
			const UString &id = entry.first;
			const auto &item = entry.second;
			const auto econIt = this->economy.find(id);
			if (econIt == this->economy.end())
			{
				continue;
			}
			const bool xcom = item->manufacturer == this->player;
			if (econIt->second.update(*this, xcom))
			{
				newItems.push_back(id);
			}
		}
	};

	processMap(this->vehicle_types);
	processMap(this->vehicle_equipment);
	processMap(this->vehicle_ammo);
	processMap(this->agent_equipment);

	if (!newItems.empty())
	{
		LogInfo("New items available this week:");
		for (const auto &id : newItems)
		{
			LogInfo("  {0}", id);
		}
	}
}

void GameState::updateEndOfWeek(bool gameStart)
{
	updateOrgFinances();

	updateUfoGrowth();
	updateItemMarket();

	fw().pushEvent(new GameEvent(GameEventType::WeeklyReport));
	weeklyPlayerUpdate();

	if (!gameStart)
	{
		int maxOrgTechLevel = 1;
		for (auto &es : equipment_sets)
		{
			if (es.second->type == EquipmentSet::Type::Human)
			{
				maxOrgTechLevel = std::max(maxOrgTechLevel, es.second->min_score);
			}
		}
		for (auto &[id, org] : organisations)
		{
			if (id != player.id && id != aliens.id && id != civilian.id)
			{
				org->tech_level = std::min(org->tech_level + 1, maxOrgTechLevel);
			}
		}

		for (auto &c : this->cities)
		{
			c.second->weeklyLoop(*this);
		}
	}
}

void GameState::weeklyPlayerUpdate()
{
	// Government funding, as UFO2P.EXE assesses it (non-4 file 0xF6880), after every
	// organisation, the Government included, has been paid this week (updateOrgFinances).
	// docs/original-game/findings/weekly-funding.md has the instruction-level evidence.
	const int weekTotal = weekScore.getTotal();
	// The cutoff tests the weeks before this one (file 0xF6FBF compares ECX, the previous-weeks
	// sum, never H + W). A first bad week only cuts the income; the week after it ends funding.
	const int previousWeeks = totalScore.getTotal() - weekTotal;

	fundingAssessment = {};
	fundingAssessment.week = weekScore;
	fundingAssessment.previousWeeksScore = previousWeeks;

	if (!fundingTerminated)
	{
		const int oldIncome = player->income;
		previousWeekIncome = oldIncome;
		fundingAssessment.oldIncome = oldIncome;

		// The old income F is paid first, unless the balance already holds two billion
		// (file 0xF6AA8: CMP EAX,0x77359400; JGE skips the credit, it does not saturate).
		if (player->balance < 2000000000)
		{
			player->balance += oldIncome;
		}

		const bool hostile = government->isRelatedTo(player) == Organisation::Relation::Hostile;
		if (hostile || previousWeeks < -2400)
		{
			// File 0xF7090 takes this week's payment back unconditionally; the latch is permanent.
			player->balance -= oldIncome;
			player->income = 0;
			fundingTerminated = true;
			fundingAssessment.outcome = hostile ? FundingAssessment::Outcome::CutForHostility
			                                    : FundingAssessment::Outcome::CutForScore;
		}
		else
		{
			// The score tier adjusts F; then the next income is capped at half the Government's
			// balance (file 0xF6FD5) and the Government is debited that *next* income (file
			// 0xF70B7). The cap persists: a Government that recovers does not restore the old
			// income.
			const int modifier = calculateFundingModifier();
			int scoreAdjustment = (modifier == 0) ? 0 : oldIncome / modifier;
			int capAdjustment = 0;
			const int availableGovFunds = government->balance / 2;
			if (availableGovFunds <= 0)
			{
				// File 0xF7025: no funds at all clears the score adjustment and cuts everything.
				scoreAdjustment = 0;
				capAdjustment = -oldIncome;
			}
			else if (oldIncome + scoreAdjustment > availableGovFunds)
			{
				capAdjustment = availableGovFunds - (oldIncome + scoreAdjustment);
			}
			const int nextIncome = std::max(0, oldIncome + scoreAdjustment + capAdjustment);
			player->income = nextIncome;
			government->balance -= nextIncome;

			fundingAssessment.outcome = FundingAssessment::Outcome::Assessed;
			fundingAssessment.scoreAdjustment = scoreAdjustment;
			fundingAssessment.capAdjustment = capAdjustment;
			fundingAssessment.nextIncome = nextIncome;
		}
	}

	// The week's score rolls over into the running total here, as the original does (file 0xFA234)
	// at the same Monday midnight -- also after funding has ended, when there is nothing left to
	// assess.
	weekScore.reset();

	// Player overheads: salary and base upkeep
	int totalSalary = 0;
	for (const auto &a : agents)
	{
		if (a.second->owner == player)
		{
			auto it = agent_salary.find(a.second->type->role);
			if (it != agent_salary.end())
			{
				totalSalary += it->second;
			}
		}
	}

	int basesCosts = 0;
	for (const auto &b : player_bases)
	{
		for (const auto &f : b.second->facilities)
		{
			// UFO2P non-4 file 0xFB211: a facility still under construction pays no upkeep.
			if (f->buildTime == 0)
			{
				basesCosts += f->type->weeklyCost;
			}
		}
	}
	player->balance = player->balance - totalSalary - basesCosts;
}

// Recalculates AI organization and civilian finances, updating budgets and salaries
void GameState::updateOrgFinances()
{
	// TODO: remove hardcoded references
	auto humanCity = cities["CITYMAP_HUMAN"];

	humanCity->populationWorking = 0;
	// Game resets only Government income, it's not right logically but will keep it to match OG
	government->income = 0;

	// Step 1. Everybody gets paid according to the current rates
	int totalCivilianIncome = 0;
	for (auto &[id, org] : organisations)
	{
		if (id != player.id && id != aliens.id)
		{
			for (auto &b : org->buildings)
			{
				// validate the original data
				if (b->currentWage < 0)
					b->currentWage = 0;

				org->income += b->calculateIncome();
				humanCity->populationWorking += b->currentWorkforce;
				totalCivilianIncome += b->currentWage * b->currentWorkforce;
			}
			org->balance += org->income;
		}
	}
	humanCity->averageWage =
	    (humanCity->populationWorking) ? totalCivilianIncome / humanCity->populationWorking : 1;

	// Step 2. Government additionally gets 10% of civilian income as taxes
	government->balance += totalCivilianIncome / 10;

	// Step 3. Calculate civilians leaving work because of the low wage
	const int minimumWage = std::max(humanCity->averageWage, 30);
	for (auto &b : humanCity->buildings)
	{
		if (b->currentWage < minimumWage)
		{
			const int satisfiedWorkers = b->currentWorkforce * b->currentWage / minimumWage;
			const int workersLeaving = b->currentWorkforce - satisfiedWorkers;
			b->currentWorkforce = satisfiedWorkers;
			humanCity->populationWorking -= workersLeaving;
			humanCity->populationUnemployed += workersLeaving;
		}
	}

	// Step 4. Civilians will try to apply for a new job (up to 5 times)
	// Workforce initially expect 20% higher wages to be re-hired, but will reduce demands
	int expectedWage = humanCity->averageWage * 12 / 10;
	const int defaultSalary = humanCity->civilianSalary;
	for (int attempt = 0; attempt < 5; ++attempt)
	{
		// Check if there's still civilians without work
		if (humanCity->populationUnemployed <= 0)
			break;

		for (auto &b : humanCity->buildings)
		{
			if (b->currentWage > expectedWage)
			{
				int workersJoining = humanCity->populationUnemployed;
				if (b->currentWage < defaultSalary * 30 / 100)
				{
					workersJoining = 0;
				}
				else if (b->currentWage < defaultSalary * 75 / 100)
				{
					// std::min so we can't overflow here
					workersJoining = workersJoining * std::min(b->currentWage, 100) / 100;
					// fall-through was intended
					if (b->currentWage < defaultSalary * 60 / 100)
						workersJoining /= 10;
					if (b->currentWage < defaultSalary * 45 / 100)
						workersJoining /= 20;
				}

				// make sure there's room for everybody
				workersJoining =
				    std::min(workersJoining, b->maximumWorkforce - b->currentWorkforce);

				if (workersJoining)
				{
					b->currentWorkforce += workersJoining;
					humanCity->populationWorking += workersJoining;
					humanCity->populationUnemployed -= workersJoining;
				}
			}
		}
		// Reduce demands by 10%
		expectedWage -= humanCity->averageWage / 10;
	}

	// Step 5. Adjust the building wages to attract new workers
	for (auto &b : humanCity->buildings)
	{
		// Skip calculations if building has no space for workers (e.g. destroyed)
		if (b->maximumWorkforce == 0)
		{
			continue;
		}

		const int maximum = b->maximumWorkforce;
		const int current = b->currentWorkforce;
		const int profitabilityLimit = b->incomePerCapita - b->maintenanceCosts / maximum;
		double wage = b->currentWage;

		if (current < maximum * 60 / 100)
		{
			// severely understaffed, biggest salary bump
			wage *= 1.2;
		}
		else if (current < maximum * 80 / 100)
		{
			wage *= 1.1;
		}
		else if (current < maximum * 90 / 100)
		{
			wage *= 1.05;
		}
		else if (current == maximum)
		{
			// if we're at 100% capacity reduce the salary
			wage *= 0.95;
		}

		// make sure we're not losing money
		b->currentWage = (wage < profitabilityLimit) ? wage : profitabilityLimit;
	}
}

int GameState::calculateFundingModifier() const
{
	// UFO2P non-4 ~0xF6EC7: cmp week-score then idiv funding. Matching bands overwrite;
	// the tightest (largest |threshold|) wins. 10000 uses the 6400/5 band, not 400/20.
	int fundingModifier = 0;
	int bestAbs = -1;
	const int totalRating = weekScore.getTotal();
	for (const auto &threshold : weekly_rating_rules)
	{
		const int scoreThreshold = threshold.first;
		const int modifier = threshold.second;
		if ((scoreThreshold <= 0 && totalRating < scoreThreshold) ||
		    (scoreThreshold > 0 && totalRating > scoreThreshold))
		{
			const int tightness = scoreThreshold < 0 ? -scoreThreshold : scoreThreshold;
			if (tightness > bestAbs)
			{
				bestAbs = tightness;
				fundingModifier = modifier;
			}
		}
	}
	return fundingModifier;
}

void GameState::updateTurbo()
{
	if (!this->canTurbo())
	{
		LogError("Called when canTurbo() is false");
	}
	unsigned ticksToUpdate = TURBO_TICKS;
	// Turbo always re-aligns to TURBO_TICKS (5 minutes)
	unsigned int align = this->gameTime.getTicks() % TURBO_TICKS;
	if (align != 0)
	{
		ticksToUpdate -= align;
	}
	this->update(ticksToUpdate);
	this->updateAfterTurbo();
}

void GameState::updateAfterTurbo()
{
	for (auto &v : this->vehicles)
	{
		if (v.second->city != current_city)
		{
			continue;
		}
		if (v.second->type->aggressiveness > 0)
		{
			continue;
		}
		v.second->update(*this, randBoundsExclusive(rng, (unsigned)0, 20 * TICKS_PER_SECOND));
	}
}

void GameState::updateBeforeBattle()
{
	// Save time to roll back to
	gameTimeBeforeBattle = GameTime(gameTime.getTicks());
	// Some useless event just to know if something was reported
	eventFromBattle = GameEventType::None;
}

void GameState::updateAfterBattle()
{
	gameTime = GameTime(gameTimeBeforeBattle.getTicks());
	gameTimeBeforeBattle = GameTime(0);

	switch (eventFromBattle)
	{
		case GameEventType::MissionCompletedBuildingAlien:
		{
			fw().pushEvent(new GameEvent(eventFromBattle));
			break;
		}
		case GameEventType::MissionCompletedBuildingNormal:
		{
			fw().pushEvent(new GameBuildingEvent(eventFromBattle, missionLocationBattleBuilding));
			break;
		}
		case GameEventType::MissionCompletedBase:
		{
			fw().pushEvent(new GameBaseEvent(eventFromBattle, missionLocationBattleBuilding->base));
			break;
		}
		case GameEventType::BaseDestroyed:
		{
			auto building = missionLocationBattleBuilding;
			fw().pushEvent(new GameSomethingDiedEvent(eventFromBattle, eventFromBattleText,
			                                          "bySomeone", building->crewQuarters));
			break;
		}
		case GameEventType::MissionCompletedBuildingRaid:
		{
			fw().pushEvent(new GameBuildingEvent(eventFromBattle, missionLocationBattleBuilding));
			break;
		}
		case GameEventType::MissionCompletedVehicle:
		{
			fw().pushEvent(new GameEvent(eventFromBattle));
			break;
		}
		case GameEventType::GameWon:
		case GameEventType::GameLost:
		{
			fw().pushEvent(new GameEvent(eventFromBattle));
			break;
		}
		default:
			break;
	}
}

void GameState::logEvent(GameEvent *ev)
{
	if (messages.size() == MAX_MESSAGES)
	{
		messages.pop_front();
	}
	Vec3<int> location = EventMessage::NO_LOCATION;
	if (GameVehicleEvent *gve = dynamic_cast<GameVehicleEvent *>(ev))
	{
		location = gve->vehicle->position;
	}
	else if (GameBuildingEvent *gve = dynamic_cast<GameBuildingEvent *>(ev))
	{
		location = {gve->building->bounds.p0.x, gve->building->bounds.p0.y, 1};
	}
	else if (GameAgentEvent *gae = dynamic_cast<GameAgentEvent *>(ev))
	{
		if (gae->agent->unit)
		{
			location = gae->agent->unit->position;
		}
		else
		{
			location = gae->agent->position;
		}
	}
	else if (GameBaseEvent *gbe = dynamic_cast<GameBaseEvent *>(ev))
	{
		location =
		    Vec3<int>(gbe->base->building->bounds.p0.x + gbe->base->building->bounds.p1.x,
		              gbe->base->building->bounds.p0.y + gbe->base->building->bounds.p1.y, 1) /
		    2;
	}
	else if (GameSomethingDiedEvent *gsde = dynamic_cast<GameSomethingDiedEvent *>(ev))
	{
		location = gsde->location;
	}
	// TODO: Other event types
	messages.emplace_back(EventMessage{gameTime, ev->message(), location});
}

uint64_t getNextObjectID(GameState &state, const UString &objectPrefix)
{
	std::lock_guard<std::mutex> l(state.objectIdCountLock);
	return state.objectIdCount[objectPrefix]++;
}

int GameScore::getTotal() const
{
	return tacticalMissions + researchCompleted + alienIncidents + craftShotDownUFO +
	       craftShotDownXCom + incursions + cityDamage + alienBuildingsDestroyed;
}

void GameScore::reset()
{
	tacticalMissions = 0;
	researchCompleted = 0;
	alienIncidents = 0;
	craftShotDownUFO = 0;
	craftShotDownXCom = 0;
	incursions = 0;
	cityDamage = 0;
	alienBuildingsDestroyed = 0;
}

void GameState::loadMods()
{
	auto mods = split(Options::modList.get(), ":");
	for (const auto &modString : mods)
	{
		LogInfo("loading mod \"{0}\"", modString);
		auto modPath = Options::modPath.get() + "/" + modString;
		auto _modInfo = ModInfo::getInfo(modPath);
		if (!_modInfo)
		{
			LogError("Failed to load ModInfo for mod \"{0}\"", modString);
			continue;
		}
		const auto &modInfo = *_modInfo;
		LogInfo("Loaded modinfo for mod ID \"{0}\"", modInfo.getID());
		if (modInfo.getStatePath() != "")
		{
			auto modStatePath = modPath + "/" + modInfo.getStatePath();
			LogInfo("Loading mod gamestate \"{0}\"", modStatePath);

			if (!this->loadGame(modStatePath))
			{
				LogError("Failed to load mod ID \"{0}\"", modInfo.getID());
			}
		}

		auto _language = getModLanguageInfo(modInfo);
		LogInfo("Loading mod language");
		if (_language)
		{
			const auto language = *_language;
			LogWarning("Loading mod language {0}", language.ID);
			if (!language.patch.empty())
			{
				const auto patchPath = modPath + "/" + language.patch;
				LogInfo("Loading mod language patch \"{0}\"", patchPath);
				if (!this->loadGame(patchPath))
				{
					LogError("Failed to load mod language patch \"{0}\"", patchPath);
				}
			}
		}
		LogInfo("Loading mod language complete");

		const auto &difficultySubmods = modInfo.getDifficultySubmods();
		const auto difficultySubmodIt = difficultySubmods.find(this->difficulty);
		if (difficultySubmodIt != difficultySubmods.end())
		{
			const auto &difficultySubmodPath = difficultySubmodIt->second;
			LogInfo("Loading difficulty-{0} submod \"{1}\" for mod \"{2}\"", this->difficulty,
			        difficultySubmodPath, modInfo.getID());
			if (!this->appendGameState(difficultySubmodPath))
			{
				LogError("Failed to load difficulty-{0} submod \"{1}\" for mod \"{2}\"",
				         this->difficulty, difficultySubmodPath, modInfo.getID());
			}
		}
	}
}

bool GameState::appendGameState(const UString &gamestatePath)
{
	LogInfo("Appending gamestate \"{0}\"", gamestatePath);
	auto systemPath = fw().data->fs.resolvePath(gamestatePath);
	return this->loadGame(systemPath);
}
}; // namespace OpenApoc
