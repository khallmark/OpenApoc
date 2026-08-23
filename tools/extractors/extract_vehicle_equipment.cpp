#include "framework/data.h"
#include "framework/framework.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/city/vammotype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "library/strings_format.h"
#include "tools/extractors/common/doodads.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

// Alexey Andronov (Istrebitel):
// It has been observed that vehicle weapons reload too quickly when compared to vanilla
// Introducing a multiplier of 2 to their reload time seems to bring them to
// comparable times. However, this may be wrong.
#define VEQUIPMENT_RELOAD_TIME_MULTIPLIER 2

namespace OpenApoc
{

void InitialGameStateExtractor::extractVehicleEquipment(GameState &state) const
{
	auto &data = this->ufo2p;

	// FIXME: Track these as some things (the weapon icon?) seem to be ordered by when they're
	// defined
	int weapon_count = 0;
	int engine_count = 0;
	int general_count = 0;

	for (unsigned i = 0; i < data.vehicle_equipment->count(); i++)
	{
		auto e = mksp<VEquipmentType>();
		auto edata = data.vehicle_equipment->get(i);

		e->name = data.vehicle_equipment_names->get(i);
		UString id = format("{0}{1}", VEquipmentType::getPrefix(), canon_string(e->name));

		UString research_id = format("{0}{1}", ResearchTopic::getPrefix(), canon_string(e->name));

		auto research_it = state.research.topics.find(research_id);
		if (research_it != state.research.topics.end())
		{
			e->research_dependency.topics.emplace(&state, research_it->second);
		}

		e->id = id;

		// Some data fields are common for all equipment types
		switch (edata.usable_by)
		{
			case VEHICLE_EQUIPMENT_USABLE_GROUND:
				e->users.insert(VEquipmentType::User::Ground);
				break;
			case VEHICLE_EQUIPMENT_USABLE_AIR:
				e->users.insert(VEquipmentType::User::Air);
				break;
			case VEHICLE_EQUIPMENT_USABLE_GROUND_AIR:
				e->users.insert(VEquipmentType::User::Ground);
				e->users.insert(VEquipmentType::User::Air);
				break;
			case VEHICLE_EQUIPMENT_USABLE_AMMO:
				// FIXME: Not sure what 'AMMO' usable is used for?
				e->users.insert(VEquipmentType::User::Ammo);
				break;
			default:
				LogWarning("Unexpected 'usable_by' {0} for ID {1}", (int)edata.usable_by, id);
				continue;
		}
		e->weight = edata.weight;
		// FIXME: max_ammo 0xffff is used for 'no ammo' (IE automatically-recharging stuff)

		e->max_ammo = edata.max_ammo;
		// ammo_type is a craft_ammo_names index (0xffff = none). Bind after
		// common_patch in applyVehicleEquipmentAmmoTypes so the live VAmmoType
		// key is used (patch IDs can fold '-' to '_').
		// Force all sprites into the correct palette by using A_RANDOM_VEHICLES_BACKGROUND pcx
		//(I assume the parts of the palette used for this are the same on all?)
		e->equipscreen_sprite = fw().data->loadImage(format(
		    "PCK:xcom3/ufodata/vehequip.pck:xcom3/ufodata/vehequip.tab:{0}:xcom3/ufodata/vhawk.pcx",
		    (int)edata.sprite_idx));
		e->equipscreen_size = {edata.size_x, edata.size_y};
		e->manufacturer = {&state, data.getOrgId(edata.manufacturer)};
		e->store_space = edata.store_space;

		switch (edata.type)
		{
			case VEHICLE_EQUIPMENT_TYPE_ENGINE:
			{
				auto engData = data.vehicle_engines->get(edata.data_idx);
				e->type = EquipmentSlotType::VehicleEngine;
				e->power = engData.power;
				e->top_speed = engData.top_speed;
				engine_count++;
				break;
			}
			case VEHICLE_EQUIPMENT_TYPE_WEAPON:
			{
				auto wData = data.vehicle_weapons->get(edata.data_idx);
				e->type = EquipmentSlotType::VehicleWeapon;
				e->speed = wData.speed;
				e->damage = wData.damage;
				e->accuracy = 100 - wData.accuracy;
				e->fire_delay =
				    wData.fire_delay * VEQUIPMENT_RELOAD_TIME_MULTIPLIER * TICKS_MULTIPLIER;
				e->tail_size = wData.tail_size;
				e->guided = wData.guided != 0 ? true : false;
				e->turn_rate = wData.turn_rate;
				e->range = wData.range;
				e->ttl = wData.ttl;
				e->firing_arc_1 = wData.firing_arc_1;
				e->firing_arc_2 = wData.firing_arc_2;
				e->point_defence = wData.point_defence != 0 ? true : false;
				UString sfx_path = "";
				switch (wData.fire_sfx)
				{
					case 71:
						sfx_path = "strategc/weapons/airguard";
						break;
					case 72:
						sfx_path = "strategc/weapons/bolter";
						break;
					case 73:
						sfx_path = "strategc/weapons/dinvrsn1";
						break;
					case 74:
						sfx_path = "strategc/weapons/disrupt1";
						break;
					case 75:
						sfx_path = "strategc/weapons/disrupt2";
						break;
					case 76:
						sfx_path = "strategc/weapons/disrupt3";
						break;
					case 77:
						sfx_path = "strategc/weapons/gr_missl";
						break;
					case 78:
						sfx_path = "strategc/weapons/hellfire";
						break;
					case 79:
						sfx_path = "strategc/weapons/janitor";
						break;
					case 80:
						sfx_path = "strategc/weapons/justice";
						break;
					case 81:
						sfx_path = "strategc/weapons/lancer";
						break;
					case 82:
						sfx_path = "strategc/weapons/mars_glm";
						break;
					case 83:
						sfx_path = "strategc/weapons/marsdef";
						break;
					case 84:
						sfx_path = "strategc/weapons/marsplas";
						break;
					case 85:
						sfx_path = "strategc/weapons/mcannon";
						break;
					case 86:
						sfx_path = "strategc/weapons/meglaser";
						break;
					case 87:
						sfx_path = "strategc/weapons/mplasma1";
						break;
					case 88:
						sfx_path = "strategc/weapons/mplasma2";
						break;
					case 89:
						sfx_path = "strategc/weapons/multplas";
						break;
					case 90:
						sfx_path = "strategc/weapons/repeater";
						break;
					case 91:
						sfx_path = "strategc/weapons/retrib";
						break;
					case 92:
						sfx_path = "strategc/weapons/ruptmult";
						break;
					case 93:
						sfx_path = "strategc/weapons/stasis";
						break;
					case 94:
						sfx_path = "strategc/weapons/tnkcanon";
						break;
				}
				if (sfx_path != "")
					e->fire_sfx =
					    fw().data->loadSample("RAWSOUND:xcom3/rawsound/" + sfx_path + ".raw:22050");
				// FIXME: I think this is correct? All non-guided attacks sound like expl1, all
				// missiles as expl2? Confirm!
				if (wData.guided == 0)
				{
					e->impact_sfx = fw().data->loadSample(
					    "RAWSOUND:xcom3/rawsound/strategc/explosns/explosn1.raw:22050");
				}
				else
				{
					e->impact_sfx = fw().data->loadSample(
					    "RAWSOUND:xcom3/rawsound/strategc/explosns/explosn2.raw:22050");
				}

				UString doodad_id = "";
				switch (wData.explosion_graphic)
				{
					case UFO_DOODAD_1:
						doodad_id = "DOODAD_1_AUTOCANNON";
						break;
					case UFO_DOODAD_2:
						doodad_id = "DOODAD_2_AIRGUARD";
						break;
					case UFO_DOODAD_0: // same as 3
					case UFO_DOODAD_3:
						doodad_id = "DOODAD_3_EXPLOSION";
						break;
					case UFO_DOODAD_4:
						doodad_id = "DOODAD_4_BLUEDOT";
						break;
					case UFO_DOODAD_5:
						doodad_id = "DOODAD_5_SMOKE_EXPLOSION";
						break;
					case UFO_DOODAD_6:
						doodad_id = "DOODAD_6_DIMENSION_GATE";
						break;
					case UFO_DOODAD_7:
						doodad_id = "DOODAD_7_JANITOR";
						break;
					case UFO_DOODAD_8:
						doodad_id = "DOODAD_8_LASER";
						break;
					case UFO_DOODAD_9:
						doodad_id = "DOODAD_9_PLASMA";
						break;
					case UFO_DOODAD_10:
						doodad_id = "DOODAD_10_DISRUPTOR";
						break;
					case UFO_DOODAD_11:
						doodad_id = "DOODAD_11_SUBVERSION_BIG";
						break;
					case UFO_DOODAD_12:
						doodad_id = "DOODAD_12_SUBVERSION_SMALL";
						break;
					case UFO_DOODAD_13:
						doodad_id = "DOODAD_13_SMOKE_FUME";
						break;
					case UFO_DOODAD_14:
						doodad_id = "DOODAD_14_INFILTRATION_BIG";
						break;
					case UFO_DOODAD_15:
						doodad_id = "DOODAD_15_INFILTRATION_SMALL";
						break;
				}
				if (doodad_id != "")
				{
					e->explosion_graphic = {&state, doodad_id};
				}

				e->icon = fw().data->loadImage(format(
				    "PCK:xcom3/ufodata/vs_obs.pck:xcom3/ufodata/vs_obs.tab:{0}", weapon_count));

				auto projectile_sprites = data.projectile_sprites->get(wData.projectile_image);
				for (int i = 0; i < e->tail_size; i++)
				{
					UString sprite_path = "";
					if (projectile_sprites.sprites[i] != 255)
					{
						sprite_path = format("bulletsprites/city/{0:02}.png",
						                     (unsigned)projectile_sprites.sprites[i]);
					}
					else
					{
						sprite_path = ""; // a 'gap' in the projectile trail
					}
					e->projectile_sprites.push_back(fw().data->loadImage(sprite_path));
				}
				if (wData.split_idx != -1)
				{
					for (int j = 0; j < 4; j++)
					{
						e->splitIntoTypes.push_back(StateRef<VEquipmentType>{
						    &state, "VEQUIPMENTTYPE_DISRUPTOR_MULTI-BOMB_FRAGMENT"});
					}
				}

				weapon_count++;
				break;
			}
			case VEHICLE_EQUIPMENT_TYPE_GENERAL:
			{
				auto gData = data.vehicle_general_equipment->get(edata.data_idx);
				e->type = EquipmentSlotType::VehicleGeneral;
				e->accuracy_modifier = gData.accuracy_modifier;
				e->cargo_space = gData.cargo_space;
				e->passengers = gData.passengers;
				e->alien_space = gData.alien_space;
				e->missile_jamming = gData.missile_jamming;
				e->shielding = gData.shielding;
				e->cloaking = gData.cloaking != 0;
				e->teleporting = gData.teleporting != 0;
				e->dimensionShifting = gData.dimension_shifting != 0;
				general_count++;
				break;
			}
			default:
				// FIXME:
				// We should never reach here because we "continue" before in "usable_by" field
				// If we do reach here, however, should we not just log a warning and go on?
				// Or log an error that we actually got here (which is the actual bug, and
				// not the fact that we encountered an expected and known id for empty item)
				LogError("Unexpected vequipment type {0} for ID {1}", (int)e->type, id);
		}

		state.vehicle_equipment[id] = e;
	}

	// UFO2P non-4 cequip_score_req_data at 0x1421C4. Do not apply row 4 (no named item).
	if (!data.cequip_score_req || data.cequip_score_req->count() != 1)
	{
		LogError("cequip_score_req_data missing or wrong size: {0}",
		         data.cequip_score_req ? (unsigned)data.cequip_score_req->count() : 0);
		return;
	}
	const auto table = data.cequip_score_req->get(0);
	for (int row = 0; row < CEQUIP_SCORE_REQ_NAMED_ROWS; row++)
	{
		const auto id = data.getVequipmentId(CEQUIP_SCORE_REQ_FIRST_VEQUIP + row);
		auto it = state.vehicle_equipment.find(id);
		if (it == state.vehicle_equipment.end() || !it->second)
		{
			LogError("cequip_score_req row {0} missing type {1}", row, id);
			continue;
		}
		it->second->scoreRequirementByDifficulty.clear();
		it->second->scoreRequirementByDifficulty.reserve(CEQUIP_SCORE_REQ_DIFFICULTY_COUNT);
		for (int d = 0; d < CEQUIP_SCORE_REQ_DIFFICULTY_COUNT; d++)
		{
			it->second->scoreRequirementByDifficulty.push_back((int)table.score[row][d]);
		}
		it->second->scoreRequirement = (int)table.score[row][0];
	}
}

void InitialGameStateExtractor::applyVehicleEquipmentAmmoTypes(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.vehicle_equipment || !data.vehicle_equipment_names || !data.craft_ammo_names)
	{
		return;
	}
	for (unsigned i = 0; i < data.vehicle_equipment->count(); i++)
	{
		if (i >= data.vehicle_equipment_names->count())
		{
			break;
		}
		const auto edata = data.vehicle_equipment->get(i);
		const auto name = data.vehicle_equipment_names->get(i);
		sp<VEquipmentType> equipment;
		for (auto &pair : state.vehicle_equipment)
		{
			if (pair.second && pair.second->name == name)
			{
				equipment = pair.second;
				break;
			}
		}
		if (!equipment)
		{
			continue;
		}
		if (edata.ammo_type == 0xffff)
		{
			equipment->ammo_type = {};
			continue;
		}
		if (edata.ammo_type >= data.craft_ammo_names->count())
		{
			LogError("vehicle equipment {0} ammo_type {1} >= craft_ammo_names {2}", name,
			         edata.ammo_type, data.craft_ammo_names->count());
			continue;
		}
		const auto ammoName = data.craft_ammo_names->get(edata.ammo_type);
		sp<VAmmoType> ammo;
		for (auto &pair : state.vehicle_ammo)
		{
			if (pair.second && pair.second->name == ammoName)
			{
				ammo = pair.second;
				break;
			}
		}
		if (!ammo)
		{
			LogError("No vehicle_ammo entry for {0} ammo {1} ({2})", name, edata.ammo_type,
			         ammoName);
			continue;
		}
		equipment->ammo_type = {&state, ammo->id};
	}
}

static void applyWeaponCombatStats(sp<VEquipmentType> e, const VehicleWeaponData &wData)
{
	e->speed = wData.speed;
	e->damage = wData.damage;
	e->accuracy = 100 - wData.accuracy;
	e->fire_delay = wData.fire_delay * VEQUIPMENT_RELOAD_TIME_MULTIPLIER * TICKS_MULTIPLIER;
	e->tail_size = wData.tail_size;
	e->guided = wData.guided != 0;
	e->turn_rate = wData.turn_rate;
	e->range = wData.range;
	e->ttl = wData.ttl;
	e->firing_arc_1 = wData.firing_arc_1;
	e->firing_arc_2 = wData.firing_arc_2;
	e->point_defence = wData.point_defence != 0;
}

void InitialGameStateExtractor::applyVehicleEquipmentSplitWeapons(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.vehicle_equipment || !data.vehicle_equipment_names || !data.vehicle_weapons)
	{
		return;
	}
	for (unsigned i = 0; i < data.vehicle_equipment->count(); i++)
	{
		if (i >= data.vehicle_equipment_names->count())
		{
			break;
		}
		const auto edata = data.vehicle_equipment->get(i);
		if (edata.type != VEHICLE_EQUIPMENT_TYPE_WEAPON)
		{
			continue;
		}
		if (edata.data_idx >= data.vehicle_weapons->count())
		{
			continue;
		}
		const auto wData = data.vehicle_weapons->get(edata.data_idx);
		if (wData.split_idx < 0)
		{
			continue;
		}
		if (static_cast<unsigned>(wData.split_idx) >= data.vehicle_weapons->count())
		{
			LogError("vehicle weapon {0} split_idx {1} >= weapon table {2}",
			         data.vehicle_equipment_names->get(i), wData.split_idx,
			         data.vehicle_weapons->count());
			continue;
		}
		const auto name = data.vehicle_equipment_names->get(i);
		sp<VEquipmentType> equipment;
		for (auto &pair : state.vehicle_equipment)
		{
			if (pair.second && pair.second->name == name)
			{
				equipment = pair.second;
				break;
			}
		}
		if (!equipment)
		{
			continue;
		}
		const auto fragData = data.vehicle_weapons->get(static_cast<unsigned>(wData.split_idx));
		for (auto &ref : equipment->splitIntoTypes)
		{
			if (ref.id.empty())
			{
				continue;
			}
			auto it = state.vehicle_equipment.find(ref.id);
			if (it == state.vehicle_equipment.end() || !it->second)
			{
				LogError("split fragment {0} missing for {1}", ref.id, name);
				continue;
			}
			applyWeaponCombatStats(it->second, fragData);
		}
	}
}

} // namespace OpenApoc
