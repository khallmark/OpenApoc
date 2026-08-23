#pragma once
#include <cstdint>

#define VEHICLE_EQUIPMENT_TYPE_ENGINE 0x00
#define VEHICLE_EQUIPMENT_TYPE_WEAPON 0x01
#define VEHICLE_EQUIPMENT_TYPE_GENERAL 0x02
#define VEHICLE_EQUIPMENT_TYPE_EMPTY 0x04

#define VEHICLE_EQUIPMENT_USABLE_GROUND 0x00
#define VEHICLE_EQUIPMENT_USABLE_AIR 0x01
#define VEHICLE_EQUIPMENT_USABLE_AMMO 0x02 // Not used?
#define VEHICLE_EQUIPMENT_USABLE_GROUND_AIR 0x03

#define VEHICLE_EQUIPMENT_NAMES_OFFSET_START 1353641
#define VEHICLE_EQUIPMENT_NAMES_OFFSET_END 1354564

struct VehicleEquipmentData
{
	uint8_t type;       // VEHICLE_EQUIPMENT_TYPE_*
	uint8_t data_idx;   // Index into VehicleWeaponData or VehicleEngineData or
	                    // VehicleEquipmentData
	uint16_t usable_by; // VEHICLE_EQUIPMENT_USABLE_*
	uint16_t weight;
	uint16_t max_ammo;
	uint16_t ammo_type;
	uint16_t unknown1;
	uint16_t sprite_idx;
	uint8_t size_x;
	uint8_t size_y;
	uint16_t unknown2;
	uint16_t unknown3;
	uint16_t manufacturer;
	uint16_t store_space;
};

static_assert(sizeof(struct VehicleEquipmentData) == 24, "Invalid vehicle_equpment_data size");
#define VEHICLE_EQUIPMENT_DATA_OFFSET_START 1617224
#define VEHICLE_EQUIPMENT_DATA_OFFSET_END 1618400

struct VehicleWeaponData
{
	uint16_t speed;            // FIXME: What units?
	uint16_t projectile_image; // FIXME: Where's the projectile image looked up?
	uint16_t damage;
	uint16_t accuracy; // FIXME: What units?
	uint16_t fire_delay;
	uint16_t tail_size; // FIXME: What units?
	uint16_t guided;
	uint16_t turn_rate; // FIXME: What units?
	uint16_t range;     // FIXME: What units?
	uint16_t ttl;
	uint16_t firing_arc_1; // Firing arc left/right
	uint16_t firing_arc_2; // Firing arc up/down
	uint16_t point_defence;
	int16_t split_idx; // vehicle_weapons index of the split fragment; -1 = none
	uint16_t fire_sfx;
	uint16_t idem;              //  APOC'd says this is duplicated fire_sfx
	uint16_t explosion_graphic; // FIXME: Find ptang lookup? (how many frames etc.)
};

static_assert(sizeof(struct VehicleWeaponData) == 34, "Invalid vehicle_weapon_data size");

#define VEHICLE_WEAPON_DATA_OFFSET_START 1618400
// 25 rows (0-24). Row 24 is the unnamed Disruptor Multi-Bomb fragment
// (parent data_idx 15, split_idx 24 @ 0x18B510). Engine table starts at 1619252.
#define VEHICLE_WEAPON_DATA_OFFSET_END 1619250
#pragma pack(push, 1)
struct VehicleEngineData
{
	uint32_t power;
	uint16_t top_speed;
};
#pragma pack(pop)

static_assert(sizeof(struct VehicleEngineData) == 6, "Invalid vehicle_engine_data size");

#define VEHICLE_ENGINE_DATA_OFFSET_START 1619252
#define VEHICLE_ENGINE_DATA_OFFSET_END 1619318

struct VehicleGeneralEquipmentData
{
	uint16_t accuracy_modifier;
	uint16_t cargo_space;
	uint16_t passengers;
	uint16_t alien_space;
	uint16_t missile_jamming;
	uint16_t shielding;
	uint16_t cloaking;
	uint16_t teleporting;
	uint16_t dimension_shifting;
};
static_assert(sizeof(struct VehicleGeneralEquipmentData) == 18,
              "Invalid vehicle_general_equipment_data size");

#define VEHICLE_GENERAL_EQUIPMENT_DATA_OFFSET_START 1619318
#define VEHICLE_GENERAL_EQUIPMENT_DATA_OFFSET_END 1619552

// UFO2P non-4 hexa "cequip score req data" 1319364-1319463 (file 0x1421C4).
// 5 rows × 5 difficulty columns (Novice..Superhuman) of uint32.
// Rows 0-3 are vehicle_equipment_names 44-47. Row 4 has no named item — do not bind.
#define CEQUIP_SCORE_REQ_ITEM_COUNT 5
#define CEQUIP_SCORE_REQ_DIFFICULTY_COUNT 5
#define CEQUIP_SCORE_REQ_NAMED_ROWS 4
#define CEQUIP_SCORE_REQ_FIRST_VEQUIP 44

struct CequipScoreReqData
{
	uint32_t score[CEQUIP_SCORE_REQ_ITEM_COUNT][CEQUIP_SCORE_REQ_DIFFICULTY_COUNT];
};
static_assert(sizeof(struct CequipScoreReqData) == 100, "Invalid cequip_score_req_data size");
#define CEQUIP_SCORE_REQ_DATA_OFFSET_START 1319364
#define CEQUIP_SCORE_REQ_DATA_OFFSET_END 1319464
