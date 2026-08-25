#include "game/state/battle/ai/unitaihelper.h"
#include "game/state/battle/ai/aidecision.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"
#include "game/state/tilemap/tileobject_battleunit.h"
#include "game/state/shared/aequipment.h"
#include <cfloat>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

namespace OpenApoc
{

sp<AIMovement> UnitAIHelper::getFallbackMovement(GameState &state, BattleUnit &u, bool forced)
{
	StateRef<BattleUnit> closestEnemy;
	for (auto &unit : state.current_battle->visibleEnemies[u.owner])
	{
		if (!closestEnemy || glm::length(unit->position - u.position) <
		                         glm::length(closestEnemy->position - u.position))
		{
			closestEnemy = unit;
		}
	}

	// Chance to fall back is:
	// +1% per each morale missing
	// +1% per 1/100th of lost health
	// -20% per every tile enemy is closer to us than 6
	int chance =
	    100 - u.agent->modified_stats.morale +
	    (u.agent->current_stats.health - u.agent->modified_stats.health) * 100 /
	        u.agent->current_stats.health -
	    (closestEnemy ? 20 * std::min(0, 6 - (int)glm::length(closestEnemy->position - u.position))
	                  : 0);
	if (!forced && randBoundsExclusive(state.rng, 0, 100) < chance)
	{
		return nullptr;
	}

	// Rate allies based (crudely) on distance to unit pos and from closest enemy
	auto &map = *state.current_battle->map;
	std::list<Vec3<int>> allyPos;
	float closestDistance = FLT_MAX;
	for (auto &unit : state.current_battle->units)
	{
		if (unit.second->owner != u.owner || !unit.second->isConscious() || unit.first == u.id)
		{
			continue;
		}
		auto dist = BattleUnitTileHelper::getDistanceStatic(u.position, unit.second->position) -
		            (closestEnemy ? BattleUnitTileHelper::getDistanceStatic(closestEnemy->position,
		                                                                    unit.second->position)
		                          : 0.0f);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			allyPos.push_front(unit.second->position);
		}
		else
		{
			allyPos.push_back(unit.second->position);
		}
	}
	for (auto &pos : allyPos)
	{
		if (state.current_battle->findShortestPath(u.position, pos, {map, u}).back() != pos)
		{
			continue;
		}
		auto result = mksp<AIMovement>();
		result->type = AIMovement::Type::Retreat;
		result->movementMode = MovementMode::Running;
		result->targetLocation = pos;
		return result;
	}

	return nullptr;
}

sp<AIMovement> UnitAIHelper::getRetreatMovement(GameState &state, BattleUnit &u, bool forced)
{
	// Chance to take retreat is 1% per each morale missing
	if (!forced && randBoundsExclusive(state.rng, 0, 100) >= u.agent->modified_stats.morale)
	{
		return nullptr;
	}

	StateRef<BattleUnit> closestEnemy;
	for (auto &unit : state.current_battle->visibleEnemies[u.owner])
	{
		if (!closestEnemy || glm::length(unit->position - u.position) <
		                         glm::length(closestEnemy->position - u.position))
		{
			closestEnemy = unit;
		}
	}

	// Rate exits based (crudely) on distance to unit pos and from closest enemy
	auto &map = *state.current_battle->map;
	std::set<Vec3<int>> badExits;
	std::list<Vec3<int>> goodExits;
	float closestDistance = FLT_MAX;
	for (auto &pos : state.current_battle->exits)
	{
		if (!map.getTile(pos)->hasExit)
		{
			badExits.insert(pos);
			continue;
		}
		auto dist =
		    BattleUnitTileHelper::getDistanceStatic(u.position, pos) -
		    (closestEnemy ? BattleUnitTileHelper::getDistanceStatic(closestEnemy->position, pos)
		                  : 0.0f);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			goodExits.push_front(pos);
		}
		else
		{
			goodExits.push_back(pos);
		}
	}
	for (auto &pos : badExits)
	{
		state.current_battle->exits.erase(pos);
	}
	for (auto &pos : goodExits)
	{
		if (state.current_battle->findShortestPath(u.position, pos, {map, u}).back() != pos)
		{
			continue;
		}
		auto result = mksp<AIMovement>();
		result->type = AIMovement::Type::Retreat;
		result->movementMode = MovementMode::Running;
		result->targetLocation = pos;
		return result;
	}
	return nullptr;
}

int UnitAIHelper::exposureScore(const std::vector<Vec3<float>> &threats, Vec3<float> candidate,
                                int boxXY, int boxZ)
{
	// TACP FUN_0007e600: accumulator starts at 0 and only ever decreases, so 0 means "no
	// qualifying threat found in the box" and more negative means more exposed. The caller keeps
	// the maximum (`if (best < score)`), i.e. the least-exposed candidate.
	int score = 0;
	const float halfXY = boxXY / 2.0f;
	const float halfZ = boxZ / 2.0f;
	for (const auto &t : threats)
	{
		if (std::abs(t.x - candidate.x) > halfXY || std::abs(t.y - candidate.y) > halfXY ||
		    std::abs(t.z - candidate.z) > halfZ)
		{
			continue;
		}
		score -= 1;
	}
	return score;
}

sp<AIMovement> UnitAIHelper::getTakeCoverMovement(GameState &state, BattleUnit &u, bool forced)
{
	// Chance to take cover is 33% * sqrt(num_enemies_seen), if no one is seen then assume 3
	if (!forced)
	{
		if (randBoundsExclusive(state.rng, 0, 100) >=
		    33.0f * sqrtf(u.visibleEnemies.empty() ? 3 : (int)u.visibleEnemies.size()))
		{
			return nullptr;
		}
		// If already prone - ignore (for now, until we implement proper take cover)
		if (u.movement_mode == MovementMode::Prone)
		{
			return nullptr;
		}
	}

	// B1, recovered: the original does NOT sweep neighbouring tiles looking for solidity. It
	// scores a short, fixed MENU of named candidate destinations by threat exposure and moves to
	// the least-exposed one (TACP FUN_0008c1fc, ~9 candidates, max-select over FUN_0007e600).
	// That distinction matters -- "search adjacent tiles for the best cover" is the mechanic the
	// parity guide originally guessed at, and it is not what the binary does.
	//
	// The candidate menu here is the subset this engine can express: the unit's own position
	// (staying put is always a candidate, and often the right one), and the positions of allied
	// units, which stand in for the original's rally/home points. The original also reads up to
	// four pre-authored per-map "AI waypoints" from a table this engine does not extract; that
	// is a named, disclosed gap rather than something invented here.
	auto &map = u.tileObject->map;
	std::vector<Vec3<float>> threats;
	for (auto &e : state.current_battle->visibleEnemies[u.owner])
	{
		if (e && e->isConscious())
		{
			threats.push_back(e->position);
		}
	}
	if (threats.empty())
	{
		return nullptr;
	}

	std::vector<Vec3<float>> candidates{u.position};
	for (auto &other : state.current_battle->units)
	{
		if (other.second && other.second->owner == u.owner && other.second->isConscious() &&
		    other.second->id != u.id)
		{
			candidates.push_back(other.second->position);
		}
	}

	Vec3<float> best = u.position;
	int bestScore = exposureScore(threats, u.position);
	for (const auto &c : candidates)
	{
		const int score = exposureScore(threats, c);
		if (score > bestScore)
		{
			bestScore = score;
			best = c;
		}
	}
	// Staying put won: nothing on the menu is safer, so fall through to kneel/prone as ai.txt
	// describes for that case rather than moving for the sake of moving.
	if (best == u.position)
	{
		return nullptr;
	}
	// findShortestPath works in tile coordinates; compare in the same space as the other helpers
	// in this file do, rather than against the float position.
	const Vec3<int> bestTile{(int)best.x, (int)best.y, (int)best.z};
	if (state.current_battle->findShortestPath(u.position, bestTile, {map, u}).back() != bestTile)
	{
		return nullptr;
	}
	auto result = mksp<AIMovement>();
	result->type = AIMovement::Type::Advance;
	result->targetLocation = best;
	// B2 (PRIOR-ART, not recovered). ai.txt distinguishes the two cover modes only by what they
	// do when no better cover exists -- Normal kneels, Cautious prones -- and there is no
	// printable `evasive` in TACP, so even the mode name is prior art. Nothing in the recovered
	// FUN_0008c1fc/FUN_0007e600 chain says what stance a unit adopts on ARRIVAL at cover.
	//
	// Extending the same distinction to the approach is the smallest consistent reading: a unit
	// that would go prone rather than kneel when caught in the open is not one that sprints
	// upright across it. Labelled prior-art at the point of use so nobody later cites this as
	// recovered behaviour.
	if (forced && u.canProne(u.position, u.facing))
	{
		result->movementMode = MovementMode::Prone;
		result->kneelingMode = KneelingMode::None;
	}
	else
	{
		result->movementMode = MovementMode::Running;
		result->kneelingMode = forced ? KneelingMode::Kneeling : KneelingMode::None;
	}
	return result;
}

sp<AIMovement> UnitAIHelper::getKneelMovement(GameState &state, BattleUnit &u, bool forced)
{
	if (!u.agent->isBodyStateAllowed(BodyState::Kneeling))
	{
		return nullptr;
	}

	if (!forced)
	{
		// Chance to kneel is 33% * sqrt(num_enemies_seen), if no one is seen then assume 3
		if (randBoundsExclusive(state.rng, 0, 100) >=
		    33.0f * sqrtf(u.visibleEnemies.empty() ? 3 : (int)u.visibleEnemies.size()))
		{
			return nullptr;
		}
		// If already kneeling or prone - ignore
		if (u.movement_mode == MovementMode::Prone || u.kneeling_mode == KneelingMode::Kneeling)
		{
			return nullptr;
		}
	}

	auto result = mksp<AIMovement>();
	result->type = AIMovement::Type::ChangeStance;
	result->kneelingMode = KneelingMode::Kneeling;
	result->movementMode = MovementMode::Walking;

	return result;
}

sp<AIMovement> UnitAIHelper::getTurnMovement(GameState &, BattleUnit &, Vec3<int> target)
{
	auto result = mksp<AIMovement>();

	result->type = AIMovement::Type::Turn;
	result->targetLocation = target;

	return result;
}

void UnitAIHelper::ensureItemInSlot(GameState &state, sp<AEquipment> item, EquipmentSlotType slot)
{
	auto u = item->ownerAgent->unit;
	auto curItem = u->agent->getFirstItemInSlot(slot);
	if (curItem != item)
	{
		// Remove item in the desired slot
		if (curItem)
		{
			u->agent->removeEquipment(state, curItem);
		}

		// Remove item we will use and equip it in the desired slot
		u->agent->removeEquipment(state, item);
		u->agent->addEquipment(state, item, slot);

		// Equip back the item removed earlier
		if (curItem)
		{
			for (auto &s : u->agent->type->equipment_layout->slots)
			{
				if (u->agent->canAddEquipment(s.bounds.p0, curItem->type))
				{
					u->agent->addEquipment(state, s.bounds.p0, curItem);
					curItem = nullptr;
					break;
				}
			}
		}

		// Drop the item if we couldn't equip it
		if (curItem)
		{
			u->addMission(state, BattleUnitMission::dropItem(*u, curItem));
		}
	}
}

sp<AIMovement> UnitAIHelper::getPursueMovement(GameState &state, BattleUnit &u, Vec3<int> target,
                                               bool forced)
{
	// Chance to pursuit is 1% per morale point above 20
	if (!forced &&
	    randBoundsExclusive(state.rng, 0, 100) >= std::max(0, u.agent->modified_stats.morale - 20))
	{
		return nullptr;
	}

	auto result = mksp<AIMovement>();

	result->type = AIMovement::Type::Pursue;
	result->targetLocation = target;

	return result;
}
} // namespace OpenApoc