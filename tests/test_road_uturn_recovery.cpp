// A captured Speed 5 day endpoint contained a twenty-car queue around a blocking cycle on the real
// CITYMAP_HUMAN road at x=82..84,y=39,z=2. Poses, facings, stopped counters, complete
// paths and targets below are copied from the frozen 685032b9 diagnostic capture:
// /tmp/openapoc-traffic-observed/reviewed-debug-speed5-day/census.txt (ROAD_DEBUG/MISSION_DEBUG).
// Its final.save SHA256 was 4046ee8b78ab32a9e51fff9149f497b86d4c026864b8d37d3ff8a1db44fccc88.
// The fixture embeds that evidence and uses extracted map data; it does not require the save.
// All twenty captured cars in that three-tile cluster are retained. Other city traffic,
// NPC ownership and subsequent return legs are omitted deliberately. Player ownership
// protects every car from transient retirement. A car parks through the real Land mission
// only after its original physical target is confirmed, freeing shared depot entrances.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_vehicle.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{
UString commonPath, gamePath;
constexpr unsigned STEP = 6;
struct CapturedCar
{
	UString observedId, type;
	Vec3<float> pose;
	float facing;
	Vec3<int> target;
	unsigned stopped;
	std::deque<Vec3<int>> path;
};
constexpr size_t CAR_COUNT = 20;
const std::array<CapturedCar, CAR_COUNT> captured = {
    {{"VEHICLE_135",
      "VEHICLETYPE_POLICE_CAR",
      {83.5, 39.359375, 2},
      4.712389f,
      {64, 89, 1},
      1,
      {{83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 38, 2}, {84, 39, 2}, {83, 39, 2}, {82, 39, 2},
       {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2}, {82, 44, 2}, {82, 45, 2}, {82, 46, 2},
       {82, 47, 2}, {81, 47, 2}, {80, 47, 2}, {79, 47, 2}, {78, 47, 2}, {77, 47, 2}, {76, 47, 2},
       {76, 48, 2}, {76, 49, 2}, {76, 50, 2}, {76, 51, 2}, {76, 52, 2}, {75, 52, 2}, {74, 52, 2},
       {73, 52, 2}, {72, 52, 2}, {71, 52, 2}, {70, 52, 2}, {69, 52, 2}, {68, 52, 2}, {67, 52, 2},
       {66, 52, 2}, {65, 52, 2}, {64, 52, 2}, {63, 52, 2}, {62, 52, 2}, {61, 52, 2}, {61, 53, 2},
       {61, 54, 2}, {61, 55, 2}, {61, 56, 2}, {61, 57, 2}, {61, 58, 2}, {61, 59, 2}, {61, 60, 2},
       {61, 61, 2}, {61, 62, 2}, {61, 63, 2}, {61, 64, 2}, {61, 65, 2}, {61, 66, 2}, {61, 67, 2},
       {61, 68, 2}, {61, 69, 2}, {61, 70, 2}, {61, 71, 2}, {61, 72, 2}, {61, 73, 2}, {61, 74, 2},
       {61, 75, 2}, {61, 76, 2}, {61, 77, 2}, {61, 78, 2}, {60, 78, 2}, {60, 79, 2}, {60, 80, 2},
       {61, 80, 2}, {61, 81, 2}, {61, 82, 2}, {61, 83, 2}, {61, 84, 2}, {61, 85, 2}, {61, 86, 2},
       {61, 87, 2}, {61, 88, 2}, {61, 89, 2}, {62, 89, 2}, {63, 89, 1}, {64, 89, 1}}},
     {"VEHICLE_136",
      "VEHICLETYPE_POLICE_CAR",
      {83.5, 39.359375, 2},
      4.852098f,
      {53, 44, 1},
      1,
      {{83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 38, 2}, {84, 39, 2}, {83, 39, 2}, {82, 39, 2},
       {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {81, 42, 2}, {80, 42, 2}, {79, 42, 2}, {78, 42, 2},
       {77, 42, 2}, {76, 42, 2}, {75, 42, 2}, {74, 42, 2}, {73, 42, 2}, {72, 42, 2}, {71, 42, 2},
       {70, 42, 2}, {69, 42, 2}, {68, 42, 2}, {67, 42, 2}, {67, 43, 2}, {67, 44, 2}, {67, 45, 2},
       {67, 46, 2}, {66, 46, 2}, {65, 46, 2}, {64, 46, 2}, {63, 46, 2}, {62, 46, 2}, {61, 46, 2},
       {60, 46, 2}, {59, 46, 2}, {58, 46, 2}, {57, 46, 2}, {56, 46, 2}, {55, 46, 2}, {54, 46, 2},
       {53, 46, 2}, {53, 45, 1}, {53, 44, 1}}},
     {"VEHICLE_1550",
      "VEHICLETYPE_CIVILIAN_CAR",
      {83.5, 39.359375, 2},
      4.852098f,
      {49, 108, 1},
      146,
      {{83, 39, 2},  {84, 39, 2},  {84, 38, 2},  {84, 38, 2},  {84, 39, 2},  {83, 39, 2},
       {82, 39, 2},  {82, 40, 2},  {82, 41, 2},  {82, 42, 2},  {82, 43, 2},  {82, 44, 2},
       {82, 45, 2},  {82, 46, 2},  {82, 47, 2},  {82, 48, 2},  {82, 49, 2},  {82, 50, 2},
       {82, 51, 2},  {82, 52, 2},  {82, 53, 2},  {82, 54, 2},  {82, 55, 2},  {82, 56, 2},
       {82, 57, 2},  {82, 58, 2},  {82, 59, 2},  {82, 60, 2},  {82, 61, 2},  {82, 62, 2},
       {82, 63, 2},  {82, 64, 2},  {82, 65, 2},  {82, 66, 2},  {82, 67, 2},  {82, 68, 2},
       {82, 69, 2},  {82, 70, 2},  {82, 71, 2},  {82, 72, 2},  {82, 73, 2},  {82, 74, 2},
       {82, 75, 2},  {82, 76, 2},  {82, 77, 2},  {82, 78, 2},  {82, 79, 2},  {82, 80, 2},
       {82, 81, 2},  {82, 82, 2},  {82, 83, 2},  {82, 84, 2},  {82, 85, 2},  {82, 86, 2},
       {82, 87, 2},  {82, 88, 2},  {82, 89, 2},  {82, 90, 2},  {82, 91, 2},  {82, 92, 2},
       {82, 93, 2},  {82, 94, 2},  {82, 95, 2},  {82, 96, 2},  {82, 97, 2},  {82, 98, 2},
       {81, 98, 2},  {80, 98, 2},  {79, 98, 2},  {78, 98, 2},  {77, 98, 2},  {76, 98, 2},
       {75, 98, 2},  {74, 98, 2},  {74, 99, 2},  {74, 100, 2}, {74, 101, 2}, {74, 102, 2},
       {74, 103, 2}, {73, 103, 2}, {72, 103, 2}, {71, 103, 2}, {70, 103, 2}, {69, 103, 2},
       {68, 103, 2}, {67, 103, 2}, {66, 103, 2}, {65, 103, 2}, {64, 103, 2}, {63, 103, 2},
       {62, 103, 2}, {61, 103, 2}, {60, 103, 2}, {59, 103, 2}, {58, 103, 2}, {57, 103, 2},
       {56, 103, 2}, {55, 103, 2}, {54, 103, 2}, {53, 103, 2}, {52, 103, 2}, {52, 104, 2},
       {52, 105, 2}, {52, 106, 2}, {51, 106, 2}, {50, 106, 2}, {49, 106, 2}, {49, 107, 1},
       {49, 108, 1}}},
     {"VEHICLE_1584",
      "VEHICLETYPE_AUTOTRANS",
      {83.5, 39.359375, 2},
      4.852098f,
      {28, 30, 1},
      1,
      {{83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 38, 2}, {84, 39, 2}, {83, 39, 2}, {82, 39, 2},
       {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {81, 42, 2}, {80, 42, 2}, {79, 42, 2}, {78, 42, 2},
       {77, 42, 2}, {76, 42, 2}, {75, 42, 2}, {74, 42, 2}, {73, 42, 2}, {72, 42, 2}, {71, 42, 2},
       {70, 42, 2}, {69, 42, 2}, {68, 42, 2}, {67, 42, 2}, {67, 43, 2}, {67, 44, 2}, {67, 45, 2},
       {67, 46, 2}, {66, 46, 2}, {65, 46, 2}, {64, 46, 2}, {63, 46, 2}, {62, 46, 2}, {61, 46, 2},
       {60, 46, 2}, {59, 46, 2}, {58, 46, 2}, {57, 46, 2}, {56, 46, 2}, {55, 46, 2}, {54, 46, 2},
       {53, 46, 2}, {52, 46, 2}, {51, 46, 2}, {50, 46, 2}, {49, 46, 2}, {49, 45, 2}, {49, 44, 2},
       {49, 43, 2}, {49, 42, 2}, {48, 42, 2}, {47, 42, 2}, {46, 42, 2}, {45, 42, 2}, {44, 42, 2},
       {43, 42, 2}, {42, 42, 2}, {41, 42, 2}, {40, 42, 2}, {39, 42, 2}, {38, 42, 2}, {37, 42, 2},
       {36, 42, 2}, {35, 42, 2}, {34, 42, 2}, {33, 42, 2}, {33, 41, 2}, {32, 41, 2}, {31, 41, 2},
       {31, 40, 2}, {31, 39, 2}, {31, 38, 2}, {31, 37, 2}, {31, 36, 2}, {31, 35, 2}, {31, 34, 2},
       {31, 33, 2}, {31, 32, 2}, {31, 31, 2}, {30, 31, 2}, {29, 31, 2}, {29, 32, 2}, {28, 32, 2},
       {28, 31, 1}, {28, 30, 1}}},
     {"VEHICLE_1597",
      "VEHICLETYPE_AUTOTAXI",
      {83.5, 39.359375, 2},
      4.852098f,
      {62, 43, 1},
      1,
      {{83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 38, 2}, {84, 39, 2}, {83, 39, 2}, {82, 39, 2},
       {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {81, 42, 2}, {80, 42, 2}, {79, 42, 2}, {78, 42, 2},
       {77, 42, 2}, {76, 42, 2}, {75, 42, 2}, {74, 42, 2}, {73, 42, 2}, {72, 42, 2}, {71, 42, 2},
       {70, 42, 2}, {69, 42, 2}, {68, 42, 2}, {67, 42, 2}, {67, 43, 2}, {67, 44, 2}, {67, 45, 2},
       {67, 46, 2}, {66, 46, 2}, {65, 46, 2}, {64, 46, 2}, {63, 46, 2}, {62, 46, 2}, {62, 45, 2},
       {62, 44, 1}, {62, 43, 1}}},
     {"VEHICLE_1630",
      "VEHICLETYPE_AUTOTAXI",
      {83.5, 39.359375, 2},
      4.852098f,
      {58, 78, 1},
      1,
      {{83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2},
       {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {81, 47, 2}, {80, 47, 2}, {79, 47, 2},
       {78, 47, 2}, {77, 47, 2}, {76, 47, 2}, {76, 48, 2}, {76, 49, 2}, {76, 50, 2}, {76, 51, 2},
       {76, 52, 2}, {75, 52, 2}, {74, 52, 2}, {73, 52, 2}, {72, 52, 2}, {71, 52, 2}, {70, 52, 2},
       {69, 52, 2}, {68, 52, 2}, {67, 52, 2}, {66, 52, 2}, {65, 52, 2}, {64, 52, 2}, {63, 52, 2},
       {62, 52, 2}, {61, 52, 2}, {61, 53, 2}, {61, 54, 2}, {61, 55, 2}, {61, 56, 2}, {61, 57, 2},
       {61, 58, 2}, {61, 59, 2}, {61, 60, 2}, {61, 61, 2}, {61, 62, 2}, {61, 63, 2}, {61, 64, 2},
       {61, 65, 2}, {61, 66, 2}, {61, 67, 2}, {61, 68, 2}, {61, 69, 2}, {61, 70, 2}, {61, 71, 2},
       {61, 72, 2}, {61, 73, 2}, {61, 74, 2}, {61, 75, 2}, {61, 76, 2}, {61, 77, 2}, {61, 78, 2},
       {60, 78, 2}, {60, 79, 2}, {60, 80, 2}, {59, 80, 2}, {58, 80, 2}, {58, 79, 1}, {58, 78, 1}}},
     {"VEHICLE_1653",
      "VEHICLETYPE_BLAZER_TURBO_BIKE",
      {83.5, 39.359375, 2},
      4.852098f,
      {46, 88, 2},
      1,
      {{83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {81, 42, 2},
       {80, 42, 2}, {79, 42, 2}, {78, 42, 2}, {77, 42, 2}, {76, 42, 2}, {75, 42, 2}, {74, 42, 2},
       {73, 42, 2}, {72, 42, 2}, {71, 42, 2}, {70, 42, 2}, {69, 42, 2}, {68, 42, 2}, {67, 42, 2},
       {67, 43, 2}, {67, 44, 2}, {67, 45, 2}, {67, 46, 2}, {66, 46, 2}, {65, 46, 2}, {64, 46, 2},
       {63, 46, 2}, {62, 46, 2}, {61, 46, 2}, {60, 46, 2}, {60, 47, 2}, {60, 48, 2}, {60, 49, 2},
       {60, 50, 2}, {60, 51, 2}, {60, 52, 2}, {60, 53, 2}, {60, 54, 2}, {60, 55, 2}, {60, 56, 2},
       {60, 57, 2}, {60, 58, 2}, {60, 59, 2}, {60, 60, 2}, {60, 61, 2}, {60, 62, 2}, {60, 63, 2},
       {59, 63, 2}, {58, 63, 2}, {57, 63, 2}, {57, 64, 2}, {56, 64, 2}, {55, 64, 2}, {54, 64, 2},
       {53, 64, 2}, {52, 64, 2}, {51, 64, 2}, {50, 64, 2}, {49, 64, 2}, {49, 65, 2}, {49, 66, 2},
       {49, 67, 2}, {48, 67, 2}, {47, 67, 2}, {46, 67, 2}, {46, 68, 2}, {46, 69, 2}, {46, 70, 2},
       {46, 71, 2}, {46, 72, 2}, {46, 73, 2}, {45, 73, 2}, {44, 73, 2}, {44, 74, 2}, {44, 75, 2},
       {44, 76, 2}, {44, 77, 2}, {44, 78, 2}, {44, 79, 2}, {44, 80, 2}, {44, 81, 2}, {43, 81, 2},
       {42, 81, 2}, {41, 81, 2}, {40, 81, 2}, {39, 81, 2}, {38, 81, 2}, {38, 82, 2}, {38, 83, 2},
       {38, 84, 2}, {38, 85, 2}, {38, 86, 2}, {38, 87, 2}, {38, 88, 2}, {39, 88, 2}, {40, 88, 2},
       {41, 88, 2}, {42, 88, 2}, {43, 88, 2}, {44, 88, 2}, {45, 88, 2}, {46, 88, 2}}},
     {"VEHICLE_1665",
      "VEHICLETYPE_AUTOTAXI",
      {83.5, 39.359375, 2},
      4.852098f,
      {84, 89, 1},
      203,
      {{83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2},
       {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {82, 48, 2}, {82, 49, 2}, {82, 50, 2},
       {82, 51, 2}, {82, 52, 2}, {82, 53, 2}, {82, 54, 2}, {82, 55, 2}, {82, 56, 2}, {82, 57, 2},
       {82, 58, 2}, {82, 59, 2}, {82, 60, 2}, {82, 61, 2}, {82, 62, 2}, {82, 63, 2}, {82, 64, 2},
       {82, 65, 2}, {82, 66, 2}, {82, 67, 2}, {82, 68, 2}, {82, 69, 2}, {82, 70, 2}, {82, 71, 2},
       {82, 72, 2}, {82, 73, 2}, {82, 74, 2}, {82, 75, 2}, {82, 76, 2}, {82, 77, 2}, {82, 78, 2},
       {82, 79, 2}, {82, 80, 2}, {82, 81, 2}, {82, 82, 2}, {82, 83, 2}, {82, 84, 2}, {82, 85, 2},
       {82, 86, 2}, {82, 87, 2}, {82, 88, 2}, {82, 89, 2}, {82, 90, 2}, {82, 91, 2}, {82, 92, 2},
       {83, 92, 2}, {84, 92, 2}, {84, 91, 2}, {84, 90, 1}, {84, 89, 1}}},
     {"VEHICLE_1685",
      "VEHICLETYPE_CIVILIAN_CAR",
      {83.5, 39.359375, 2},
      4.9865565f,
      {72, 39, 1},
      1,
      {{83, 39, 2},
       {82, 39, 2},
       {82, 40, 2},
       {82, 40, 2},
       {82, 41, 2},
       {82, 42, 2},
       {81, 42, 2},
       {80, 42, 2},
       {79, 42, 2},
       {78, 42, 2},
       {77, 42, 2},
       {76, 42, 2},
       {75, 42, 2},
       {74, 42, 2},
       {74, 41, 2},
       {74, 40, 2},
       {74, 39, 2},
       {73, 39, 1},
       {72, 39, 1}}},
     {"VEHICLE_1693",
      "VEHICLETYPE_BLAZER_TURBO_BIKE",
      {84.5, 39.5, 2},
      0.0f,
      {84, 89, 1},
      5469070,
      {{84, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2},
       {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {82, 48, 2}, {82, 49, 2}, {82, 50, 2},
       {82, 51, 2}, {82, 52, 2}, {82, 53, 2}, {82, 54, 2}, {82, 55, 2}, {82, 56, 2}, {82, 57, 2},
       {82, 58, 2}, {82, 59, 2}, {82, 60, 2}, {82, 61, 2}, {82, 62, 2}, {82, 63, 2}, {82, 64, 2},
       {82, 65, 2}, {82, 66, 2}, {82, 67, 2}, {82, 68, 2}, {82, 69, 2}, {82, 70, 2}, {82, 71, 2},
       {82, 72, 2}, {82, 73, 2}, {82, 74, 2}, {82, 75, 2}, {82, 76, 2}, {82, 77, 2}, {82, 78, 2},
       {82, 79, 2}, {82, 80, 2}, {82, 81, 2}, {82, 82, 2}, {82, 83, 2}, {82, 84, 2}, {82, 85, 2},
       {82, 86, 2}, {82, 87, 2}, {82, 88, 2}, {82, 89, 2}, {82, 90, 2}, {82, 91, 2}, {82, 92, 2},
       {83, 92, 2}, {84, 92, 2}, {84, 91, 2}, {84, 90, 1}, {84, 89, 1}}},
     {"VEHICLE_1709",
      "VEHICLETYPE_CIVILIAN_CAR",
      {82.640625, 39.5, 2},
      0.0f,
      {103, 43, 1},
      5457329,
      {{82, 39, 2},  {83, 39, 2},  {83, 39, 2},  {84, 39, 2},  {85, 39, 2},  {86, 39, 2},
       {87, 39, 2},  {87, 38, 2},  {88, 38, 2},  {89, 38, 2},  {90, 38, 2},  {91, 38, 2},
       {92, 38, 2},  {93, 38, 2},  {94, 38, 2},  {95, 38, 2},  {96, 38, 2},  {97, 38, 2},
       {98, 38, 2},  {98, 39, 2},  {98, 40, 2},  {99, 40, 2},  {100, 40, 2}, {101, 40, 2},
       {102, 40, 2}, {103, 40, 2}, {103, 41, 2}, {103, 42, 1}, {103, 43, 1}}},
     {"VEHICLE_1715",
      "VEHICLETYPE_CIVILIAN_CAR",
      {84.5, 39.359375, 2},
      4.712389f,
      {88, 95, 1},
      5470956,
      {{84, 39, 2}, {83, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2},
       {82, 43, 2}, {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {82, 48, 2}, {82, 49, 2},
       {82, 50, 2}, {82, 51, 2}, {82, 52, 2}, {82, 53, 2}, {82, 54, 2}, {82, 55, 2}, {82, 56, 2},
       {82, 57, 2}, {82, 58, 2}, {82, 59, 2}, {82, 60, 2}, {82, 61, 2}, {82, 62, 2}, {82, 63, 2},
       {82, 64, 2}, {82, 65, 2}, {82, 66, 2}, {82, 67, 2}, {82, 68, 2}, {82, 69, 2}, {82, 70, 2},
       {82, 71, 2}, {82, 72, 2}, {82, 73, 2}, {82, 74, 2}, {82, 75, 2}, {82, 76, 2}, {82, 77, 2},
       {82, 78, 2}, {82, 79, 2}, {82, 80, 2}, {82, 81, 2}, {82, 82, 2}, {82, 83, 2}, {82, 84, 2},
       {82, 85, 2}, {82, 86, 2}, {82, 87, 2}, {82, 88, 2}, {82, 89, 2}, {82, 90, 2}, {82, 91, 2},
       {82, 92, 2}, {82, 93, 2}, {82, 94, 2}, {82, 95, 2}, {82, 96, 2}, {82, 97, 2}, {82, 98, 2},
       {83, 98, 2}, {84, 98, 2}, {85, 98, 2}, {86, 98, 2}, {87, 98, 2}, {88, 98, 2}, {88, 97, 2},
       {88, 96, 1}, {88, 95, 1}}},
     {"VEHICLE_1733",
      "VEHICLETYPE_BLAZER_TURBO_BIKE",
      {84.5, 39.359375, 2},
      4.712389f,
      {64, 89, 1},
      5456270,
      {{84, 39, 2}, {83, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2},
       {82, 43, 2}, {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {81, 47, 2}, {80, 47, 2},
       {79, 47, 2}, {78, 47, 2}, {77, 47, 2}, {76, 47, 2}, {76, 48, 2}, {76, 49, 2}, {76, 50, 2},
       {76, 51, 2}, {76, 52, 2}, {75, 52, 2}, {74, 52, 2}, {73, 52, 2}, {72, 52, 2}, {71, 52, 2},
       {70, 52, 2}, {69, 52, 2}, {68, 52, 2}, {67, 52, 2}, {66, 52, 2}, {65, 52, 2}, {64, 52, 2},
       {63, 52, 2}, {62, 52, 2}, {61, 52, 2}, {61, 53, 2}, {61, 54, 2}, {61, 55, 2}, {61, 56, 2},
       {61, 57, 2}, {61, 58, 2}, {61, 59, 2}, {61, 60, 2}, {61, 61, 2}, {61, 62, 2}, {61, 63, 2},
       {61, 64, 2}, {61, 65, 2}, {61, 66, 2}, {61, 67, 2}, {61, 68, 2}, {61, 69, 2}, {61, 70, 2},
       {61, 71, 2}, {61, 72, 2}, {61, 73, 2}, {61, 74, 2}, {61, 75, 2}, {61, 76, 2}, {61, 77, 2},
       {61, 78, 2}, {60, 78, 2}, {60, 79, 2}, {60, 80, 2}, {61, 80, 2}, {61, 81, 2}, {61, 82, 2},
       {61, 83, 2}, {61, 84, 2}, {61, 85, 2}, {61, 86, 2}, {61, 87, 2}, {61, 88, 2}, {61, 89, 2},
       {62, 89, 2}, {63, 89, 1}, {64, 89, 1}}},
     {"VEHICLE_1741",
      "VEHICLETYPE_AUTOTRANS",
      {84.5, 39.359375, 2},
      4.712389f,
      {68, 98, 1},
      5461963,
      {{84, 39, 2}, {83, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2},
       {82, 43, 2}, {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {82, 48, 2}, {82, 49, 2},
       {82, 50, 2}, {82, 51, 2}, {82, 52, 2}, {82, 53, 2}, {82, 54, 2}, {82, 55, 2}, {82, 56, 2},
       {82, 57, 2}, {82, 58, 2}, {82, 59, 2}, {82, 60, 2}, {82, 61, 2}, {82, 62, 2}, {82, 63, 2},
       {82, 64, 2}, {82, 65, 2}, {82, 66, 2}, {82, 67, 2}, {82, 68, 2}, {82, 69, 2}, {82, 70, 2},
       {82, 71, 2}, {82, 72, 2}, {82, 73, 2}, {82, 74, 2}, {82, 75, 2}, {82, 76, 2}, {82, 77, 2},
       {82, 78, 2}, {82, 79, 2}, {82, 80, 2}, {82, 81, 2}, {82, 82, 2}, {82, 83, 2}, {82, 84, 2},
       {82, 85, 2}, {82, 86, 2}, {82, 87, 2}, {82, 88, 2}, {82, 89, 2}, {81, 89, 2}, {80, 89, 2},
       {79, 89, 2}, {78, 89, 2}, {77, 89, 2}, {76, 89, 2}, {75, 89, 2}, {74, 89, 2}, {74, 90, 2},
       {74, 91, 2}, {74, 92, 2}, {74, 93, 2}, {74, 94, 2}, {74, 95, 2}, {73, 95, 2}, {72, 95, 2},
       {71, 95, 2}, {70, 95, 2}, {70, 96, 2}, {70, 97, 2}, {70, 98, 2}, {69, 98, 1}, {68, 98, 1}}},
     {"VEHICLE_1756",
      "VEHICLETYPE_CIVILIAN_CAR",
      {84.5, 39.359375, 2},
      4.712389f,
      {51, 73, 2},
      5464606,
      {{84, 39, 2}, {83, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2},
       {81, 42, 2}, {80, 42, 2}, {79, 42, 2}, {78, 42, 2}, {77, 42, 2}, {76, 42, 2}, {75, 42, 2},
       {74, 42, 2}, {73, 42, 2}, {72, 42, 2}, {71, 42, 2}, {70, 42, 2}, {69, 42, 2}, {68, 42, 2},
       {67, 42, 2}, {67, 43, 2}, {67, 44, 2}, {67, 45, 2}, {67, 46, 2}, {66, 46, 2}, {65, 46, 2},
       {64, 46, 2}, {63, 46, 2}, {62, 46, 2}, {61, 46, 2}, {60, 46, 2}, {60, 47, 2}, {60, 48, 2},
       {60, 49, 2}, {60, 50, 2}, {60, 51, 2}, {60, 52, 2}, {60, 53, 2}, {60, 54, 2}, {60, 55, 2},
       {60, 56, 2}, {60, 57, 2}, {60, 58, 2}, {60, 59, 2}, {60, 60, 2}, {60, 61, 2}, {60, 62, 2},
       {60, 63, 2}, {59, 63, 2}, {58, 63, 2}, {57, 63, 2}, {57, 64, 2}, {56, 64, 2}, {55, 64, 2},
       {54, 64, 2}, {53, 64, 2}, {52, 64, 2}, {51, 64, 2}, {50, 64, 2}, {49, 64, 2}, {49, 65, 2},
       {49, 66, 2}, {49, 67, 2}, {48, 67, 2}, {47, 67, 2}, {46, 67, 2}, {46, 68, 2}, {46, 69, 2},
       {46, 70, 2}, {46, 71, 2}, {46, 72, 2}, {46, 73, 2}, {47, 73, 2}, {48, 73, 2}, {49, 73, 2},
       {50, 73, 2}, {51, 73, 2}}},
     {"VEHICLE_1782",
      "VEHICLETYPE_BLAZER_TURBO_BIKE",
      {84.5, 39.359375, 2},
      4.712389f,
      {45, 97, 1},
      5462159,
      {{84, 39, 2}, {83, 39, 2}, {83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2},
       {82, 43, 2}, {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {81, 47, 2}, {80, 47, 2},
       {79, 47, 2}, {78, 47, 2}, {77, 47, 2}, {76, 47, 2}, {76, 48, 2}, {76, 49, 2}, {76, 50, 2},
       {76, 51, 2}, {76, 52, 2}, {75, 52, 2}, {74, 52, 2}, {73, 52, 2}, {72, 52, 2}, {71, 52, 2},
       {70, 52, 2}, {69, 52, 2}, {68, 52, 2}, {67, 52, 2}, {66, 52, 2}, {65, 52, 2}, {64, 52, 2},
       {63, 52, 2}, {62, 52, 2}, {61, 52, 2}, {61, 53, 2}, {61, 54, 2}, {61, 55, 2}, {61, 56, 2},
       {61, 57, 2}, {61, 58, 2}, {61, 59, 2}, {61, 60, 2}, {61, 61, 2}, {61, 62, 2}, {61, 63, 2},
       {61, 64, 2}, {61, 65, 2}, {61, 66, 2}, {61, 67, 2}, {61, 68, 2}, {61, 69, 2}, {61, 70, 2},
       {61, 71, 2}, {61, 72, 2}, {61, 73, 2}, {61, 74, 2}, {61, 75, 2}, {61, 76, 2}, {61, 77, 2},
       {61, 78, 2}, {60, 78, 2}, {60, 79, 2}, {60, 80, 2}, {61, 80, 2}, {61, 81, 2}, {61, 82, 2},
       {61, 83, 2}, {61, 84, 2}, {61, 85, 2}, {61, 86, 2}, {61, 87, 2}, {61, 88, 2}, {61, 89, 2},
       {61, 90, 2}, {61, 91, 2}, {61, 92, 2}, {60, 92, 2}, {59, 92, 2}, {58, 92, 2}, {57, 92, 2},
       {56, 92, 2}, {55, 92, 2}, {54, 92, 2}, {53, 92, 2}, {52, 92, 2}, {52, 93, 2}, {52, 94, 2},
       {51, 94, 2}, {50, 94, 2}, {49, 94, 2}, {49, 95, 2}, {49, 96, 2}, {48, 96, 2}, {48, 97, 2},
       {48, 98, 2}, {48, 99, 2}, {47, 99, 2}, {46, 99, 2}, {45, 99, 2}, {45, 98, 1}, {45, 97, 1}}},
     {"VEHICLE_1841",
      "VEHICLETYPE_CIVILIAN_CAR",
      {83.5, 39.359375, 2},
      4.712389f,
      {57, 89, 1},
      1,
      {{83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2},
       {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {81, 47, 2}, {80, 47, 2}, {79, 47, 2},
       {78, 47, 2}, {77, 47, 2}, {76, 47, 2}, {76, 48, 2}, {76, 49, 2}, {76, 50, 2}, {76, 51, 2},
       {76, 52, 2}, {75, 52, 2}, {74, 52, 2}, {73, 52, 2}, {72, 52, 2}, {71, 52, 2}, {70, 52, 2},
       {69, 52, 2}, {68, 52, 2}, {67, 52, 2}, {66, 52, 2}, {65, 52, 2}, {64, 52, 2}, {63, 52, 2},
       {62, 52, 2}, {61, 52, 2}, {61, 53, 2}, {61, 54, 2}, {61, 55, 2}, {61, 56, 2}, {61, 57, 2},
       {61, 58, 2}, {61, 59, 2}, {61, 60, 2}, {61, 61, 2}, {61, 62, 2}, {61, 63, 2}, {61, 64, 2},
       {61, 65, 2}, {61, 66, 2}, {61, 67, 2}, {61, 68, 2}, {61, 69, 2}, {61, 70, 2}, {61, 71, 2},
       {61, 72, 2}, {61, 73, 2}, {61, 74, 2}, {61, 75, 2}, {61, 76, 2}, {61, 77, 2}, {61, 78, 2},
       {60, 78, 2}, {60, 79, 2}, {60, 80, 2}, {61, 80, 2}, {61, 81, 2}, {61, 82, 2}, {61, 83, 2},
       {61, 84, 2}, {61, 85, 2}, {61, 86, 2}, {61, 87, 2}, {61, 88, 2}, {61, 89, 2}, {61, 90, 2},
       {61, 91, 2}, {61, 92, 2}, {60, 92, 2}, {59, 92, 2}, {58, 92, 2}, {57, 92, 2}, {57, 91, 2},
       {57, 90, 1}, {57, 89, 1}}},
     {"VEHICLE_1853",
      "VEHICLETYPE_CIVILIAN_CAR",
      {83.5, 39.359375, 2},
      4.712389f,
      {88, 104, 2},
      1,
      {{83, 39, 2},  {82, 39, 2},  {82, 40, 2},  {82, 40, 2},  {82, 41, 2},  {82, 42, 2},
       {82, 43, 2},  {82, 44, 2},  {82, 45, 2},  {82, 46, 2},  {82, 47, 2},  {82, 48, 2},
       {82, 49, 2},  {82, 50, 2},  {82, 51, 2},  {82, 52, 2},  {82, 53, 2},  {82, 54, 2},
       {82, 55, 2},  {82, 56, 2},  {82, 57, 2},  {82, 58, 2},  {82, 59, 2},  {82, 60, 2},
       {82, 61, 2},  {82, 62, 2},  {82, 63, 2},  {82, 64, 2},  {82, 65, 2},  {82, 66, 2},
       {82, 67, 2},  {82, 68, 2},  {82, 69, 2},  {82, 70, 2},  {82, 71, 2},  {82, 72, 2},
       {82, 73, 2},  {82, 74, 2},  {82, 75, 2},  {82, 76, 2},  {82, 77, 2},  {82, 78, 2},
       {82, 79, 2},  {82, 80, 2},  {82, 81, 2},  {82, 82, 2},  {82, 83, 2},  {82, 84, 2},
       {82, 85, 2},  {82, 86, 2},  {82, 87, 2},  {82, 88, 2},  {82, 89, 2},  {82, 90, 2},
       {82, 91, 2},  {82, 92, 2},  {82, 93, 2},  {82, 94, 2},  {82, 95, 2},  {82, 96, 2},
       {82, 97, 2},  {82, 98, 2},  {82, 99, 2},  {82, 100, 2}, {82, 101, 2}, {82, 102, 2},
       {82, 103, 2}, {82, 104, 2}, {83, 104, 2}, {84, 104, 2}, {85, 104, 2}, {86, 104, 2},
       {87, 104, 2}, {88, 104, 2}}},
     {"VEHICLE_1895",
      "VEHICLETYPE_BLAZER_TURBO_BIKE",
      {83.5, 39.359375, 2},
      4.712389f,
      {30, 42, 1},
      1,
      {{83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 38, 2}, {84, 39, 2}, {83, 39, 2}, {82, 39, 2},
       {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {81, 42, 2}, {80, 42, 2}, {79, 42, 2}, {78, 42, 2},
       {77, 42, 2}, {76, 42, 2}, {75, 42, 2}, {74, 42, 2}, {73, 42, 2}, {72, 42, 2}, {71, 42, 2},
       {70, 42, 2}, {69, 42, 2}, {68, 42, 2}, {67, 42, 2}, {67, 43, 2}, {67, 44, 2}, {67, 45, 2},
       {67, 46, 2}, {66, 46, 2}, {65, 46, 2}, {64, 46, 2}, {63, 46, 2}, {62, 46, 2}, {61, 46, 2},
       {60, 46, 2}, {59, 46, 2}, {58, 46, 2}, {57, 46, 2}, {56, 46, 2}, {55, 46, 2}, {54, 46, 2},
       {53, 46, 2}, {52, 46, 2}, {51, 46, 2}, {50, 46, 2}, {49, 46, 2}, {49, 45, 2}, {49, 44, 2},
       {49, 43, 2}, {49, 42, 2}, {48, 42, 2}, {47, 42, 2}, {46, 42, 2}, {45, 42, 2}, {44, 42, 2},
       {43, 42, 2}, {42, 42, 2}, {41, 42, 2}, {40, 42, 2}, {39, 42, 2}, {38, 42, 2}, {37, 42, 2},
       {36, 42, 2}, {35, 42, 2}, {34, 42, 2}, {33, 42, 2}, {32, 42, 2}, {31, 42, 1}, {30, 42, 1}}},
     {"VEHICLE_1900",
      "VEHICLETYPE_AUTOTRANS",
      {83.5, 39.359375, 2},
      4.712389f,
      {55, 96, 2},
      1,
      {{83, 39, 2}, {82, 39, 2}, {82, 40, 2}, {82, 40, 2}, {82, 41, 2}, {82, 42, 2}, {82, 43, 2},
       {82, 44, 2}, {82, 45, 2}, {82, 46, 2}, {82, 47, 2}, {82, 48, 2}, {82, 49, 2}, {82, 50, 2},
       {82, 51, 2}, {82, 52, 2}, {82, 53, 2}, {82, 54, 2}, {82, 55, 2}, {82, 56, 2}, {82, 57, 2},
       {82, 58, 2}, {82, 59, 2}, {82, 60, 2}, {82, 61, 2}, {82, 62, 2}, {82, 63, 2}, {82, 64, 2},
       {82, 65, 2}, {82, 66, 2}, {82, 67, 2}, {82, 68, 2}, {82, 69, 2}, {82, 70, 2}, {82, 71, 2},
       {82, 72, 2}, {82, 73, 2}, {82, 74, 2}, {82, 75, 2}, {82, 76, 2}, {82, 77, 2}, {82, 78, 2},
       {82, 79, 2}, {82, 80, 2}, {82, 81, 2}, {82, 82, 2}, {82, 83, 2}, {82, 84, 2}, {82, 85, 2},
       {82, 86, 2}, {82, 87, 2}, {82, 88, 2}, {82, 89, 2}, {81, 89, 2}, {80, 89, 2}, {79, 89, 2},
       {78, 89, 2}, {77, 89, 2}, {76, 89, 2}, {75, 89, 2}, {74, 89, 2}, {74, 90, 2}, {74, 91, 2},
       {74, 92, 2}, {74, 93, 2}, {74, 94, 2}, {74, 95, 2}, {73, 95, 2}, {72, 95, 2}, {71, 95, 2},
       {70, 95, 2}, {69, 95, 2}, {68, 95, 2}, {67, 95, 2}, {66, 95, 2}, {65, 95, 2}, {64, 95, 2},
       {63, 95, 2}, {62, 95, 2}, {61, 95, 2}, {61, 96, 2}, {61, 97, 2}, {61, 98, 2}, {61, 99, 2},
       {60, 99, 2}, {59, 99, 3}, {58, 99, 3}, {57, 99, 3}, {56, 99, 3}, {55, 99, 3}, {55, 98, 3},
       {55, 97, 2}, {55, 96, 2}}}}};

// A separate fresh day with the first junction fix exposed a second exact pair. These
// records come from diagnose-junction-cycle/fresh-scoped-day/census.txt. The police route
// starts A->B->A before its real eastbound continuation, while the civilian car on the
// corner at B requests A. Neither tile has opposite connections for that blocked heading.
const std::array<CapturedCar, 2> cornerPair = {
    {{"VEHICLE_135",
      "VEHICLETYPE_POLICE_CAR",
      {64.5, 46.359375, 2},
      4.712389f,
      {94, 31, 2},
      12072477,
      {{64, 46, 2}, {64, 47, 2}, {64, 46, 2}, {65, 46, 2}, {66, 46, 2}, {67, 46, 2}, {67, 45, 2},
       {67, 44, 2}, {67, 43, 2}, {67, 42, 2}, {68, 42, 2}, {69, 42, 2}, {70, 42, 2}, {71, 42, 2},
       {72, 42, 2}, {73, 42, 2}, {74, 42, 2}, {75, 42, 2}, {76, 42, 2}, {77, 42, 2}, {78, 42, 2},
       {79, 42, 2}, {80, 42, 2}, {81, 42, 2}, {82, 42, 2}, {82, 41, 2}, {82, 40, 2}, {82, 39, 2},
       {83, 39, 2}, {84, 39, 2}, {85, 39, 2}, {86, 39, 2}, {87, 39, 2}, {87, 38, 2}, {87, 37, 2},
       {87, 36, 2}, {87, 35, 2}, {87, 34, 2}, {87, 33, 2}, {87, 32, 2}, {88, 32, 2}, {89, 32, 2},
       {90, 32, 2}, {91, 32, 2}, {91, 33, 2}, {91, 34, 2}, {92, 34, 2}, {93, 34, 2}, {94, 34, 2},
       {94, 33, 2}, {94, 32, 2}, {94, 31, 2}}},
     {"VEHICLE_813",
      "VEHICLETYPE_CIVILIAN_CAR",
      {64.5, 47.640625, 2},
      1.5707964f,
      {78, 105, 1},
      12066679,
      {{64, 47, 2},  {64, 46, 2},  {63, 46, 2},  {62, 46, 2},  {61, 46, 2},  {60, 46, 2},
       {60, 47, 2},  {60, 48, 2},  {60, 49, 2},  {60, 50, 2},  {60, 51, 2},  {60, 52, 2},
       {60, 53, 2},  {60, 54, 2},  {60, 55, 2},  {60, 56, 2},  {60, 57, 2},  {60, 58, 2},
       {60, 59, 2},  {60, 60, 2},  {60, 61, 2},  {60, 62, 2},  {60, 63, 2},  {60, 64, 2},
       {60, 65, 2},  {60, 66, 2},  {60, 67, 2},  {60, 68, 2},  {60, 69, 2},  {60, 70, 2},
       {60, 71, 2},  {60, 72, 2},  {60, 73, 2},  {60, 74, 2},  {60, 75, 2},  {60, 76, 2},
       {60, 77, 2},  {60, 78, 2},  {60, 79, 2},  {60, 80, 2},  {61, 80, 2},  {61, 81, 2},
       {61, 82, 2},  {61, 83, 2},  {61, 84, 2},  {61, 85, 2},  {61, 86, 2},  {61, 87, 2},
       {61, 88, 2},  {61, 89, 2},  {61, 90, 2},  {61, 91, 2},  {61, 92, 2},  {61, 93, 2},
       {61, 94, 2},  {61, 95, 2},  {61, 96, 2},  {61, 97, 2},  {61, 98, 2},  {61, 99, 2},
       {61, 100, 2}, {61, 101, 2}, {61, 102, 2}, {61, 103, 2}, {62, 103, 2}, {63, 103, 2},
       {64, 103, 2}, {65, 103, 2}, {66, 103, 2}, {67, 103, 2}, {68, 103, 2}, {69, 103, 2},
       {70, 103, 2}, {71, 103, 2}, {72, 103, 2}, {73, 103, 2}, {74, 103, 2}, {75, 103, 2},
       {76, 103, 2}, {76, 104, 2}, {77, 104, 2}, {78, 104, 2}, {79, 104, 2}, {80, 104, 2},
       {80, 105, 2}, {79, 105, 1}, {78, 105, 1}}}}};

bool loadCity(GameState &state)
{
	TEST_REQUIRE(loadStartedGameState(state, commonPath, gamePath), "could not load city fixture");
	TEST_REQUIRE(state.current_city && state.current_city->map, "no initialized human city");
	state.skipTurboCalculations = false;
	return true;
}

bool test_observed_cycle_finishes_original_targets()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load captured cycle map");
	auto city = state.current_city;
	const std::array<std::pair<Vec3<int>, std::vector<bool>>, 3> geometry = {{
	    {{82, 39, 2}, {false, true, true, true}},
	    {{83, 39, 2}, {false, true, false, true}},
	    {{84, 39, 2}, {true, true, false, true}},
	}};
	for (const auto &[position, connections] : geometry)
	{
		const auto scenery = city->map->getTile(position)->presentScenery;
		TEST_REQUIRE(scenery && !scenery->destroyed && scenery->type->connection == connections,
		             "observed road geometry changed at {0}", position);
	}
	const GroundVehicleTileHelper road{*city->map, VehicleType::Type::Road};
	std::array<sp<Vehicle>, CAR_COUNT> cars;
	for (size_t i = 0; i < cars.size(); i++)
	{
		const auto &record = captured[i];
		for (auto it = record.path.begin(); std::next(it) != record.path.end(); ++it)
		{
			const auto next = std::next(it);
			TEST_REQUIRE(*it == *next ||
			                 road.canEnterTile(city->map->getTile(*it), city->map->getTile(*next)),
			             "captured {0} route is disconnected at {1}", record.observedId, *it);
		}
		auto v = city->placeVehicle(state, {&state, record.type}, state.getPlayer(), record.pose,
		                            record.facing);
		TEST_REQUIRE(v && v->tileObject, "could not place {0}", record.observedId);
		TEST_REQUIRE(v->setMission(state, VehicleMission::gotoLocation(state, *v, record.target)),
		             "could not create captured target mission");
		TEST_REQUIRE(!v->missions.empty(), "captured target mission disappeared at placement");
		auto &mission = v->missions.front();
		mission.currentPlannedPath = record.path;
		mission.roadStoppedTicks = record.stopped;
		mission.blockedWaitTicks = 8;
		v->goalPosition = v->position;
		v->velocity = {0, 0, 0};
		cars[i] = v;
	}
	// Every recorded car is initially blocked by another captured member. A finite queue
	// with no free endpoint necessarily contains a cycle; the selected blocker may vary
	// among stacked cars because the map scans tile objects in pointer order.
	for (size_t i = 0; i < cars.size(); i++)
	{
		const auto &path = captured[i].path;
		const int heading = VehicleMission::roadHeading(path[0], path[1]);
		const int onward = VehicleMission::roadHeading(path[1], path[2]);
		const auto blocker = cars[i]->missions.front().roadBlocker(
		    *cars[i], city->map->getTile(path[0]), city->map->getTile(path[1]),
		    onward < 0 ? heading : onward);
		TEST_REQUIRE(blocker && std::find(cars.begin(), cars.end(), blocker) != cars.end(),
		             "captured {0} is not blocked by a captured queue member",
		             captured[i].observedId);
	}
	std::array<bool, CAR_COUNT> moved{}, completed{};
	for (unsigned ticks = 0; ticks < TICKS_PER_HOUR; ticks += STEP)
	{
		// Increment in real city movement steps; no coarse snapshot or stopped-counter proxy.
		for (size_t i = 0; i < cars.size(); i++)
		{
			if (completed[i])
				continue;
			auto v = cars[i];
			v->update(state, STEP);
			TEST_REQUIRE(!v->isDead() && !v->stranded && v->tileObject,
			             "{0} disappeared or became stranded during recovery",
			             captured[i].observedId);
			const auto d = v->position - captured[i].pose;
			moved[i] = moved[i] || d.x * d.x + d.y * d.y >= 4.0f;
			completed[i] = v->missions.empty() &&
			               v->tileObject->getOwningTile()->position == captured[i].target;
			if (completed[i])
			{
				StateRef<Building> destination;
				for (const auto &building : city->buildings)
				{
					if (building->carEntranceLocation == captured[i].target &&
					    city->hasVehicleAccess(*building, *v->type))
					{
						destination = building;
						break;
					}
				}
				TEST_REQUIRE(destination, "captured target {0} has no real depot entrance",
				             captured[i].target);
				VehicleMission landing;
				landing.type = VehicleMission::MissionType::Land;
				landing.targetBuilding = destination;
				TEST_REQUIRE(v->setMission(state, landing) && !v->isDead() && !v->tileObject &&
				                 v->currentBuilding == destination,
				             "arrived {0} did not park through the real landing callback",
				             captured[i].observedId);
			}
		}
		state.gameTime.addTicks(STEP);
		if (std::all_of(completed.begin(), completed.end(), [](bool done) { return done; }))
			break;
	}
	for (size_t i = 0; i < cars.size(); i++)
	{
		TEST_CHECK(moved[i], "{0} reset its recovery without moving two tiles",
		           captured[i].observedId);
		TEST_CHECK(completed[i], "{0} did not finish at its original target {1}; position {2}",
		           captured[i].observedId, captured[i].target, cars[i]->position);
	}
	return true;
}

bool test_observed_corner_pair_finishes_original_targets()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load corner-pair map");
	auto city = state.current_city;
	const auto a = city->map->getTile(Vec3<int>{64, 46, 2});
	const auto b = city->map->getTile(Vec3<int>{64, 47, 2});
	TEST_REQUIRE(
	    a->presentScenery &&
	        a->presentScenery->type->connection == std::vector<bool>({false, true, true, true}) &&
	        b->presentScenery &&
	        b->presentScenery->type->connection == std::vector<bool>({true, false, false, true}),
	    "captured T/corner geometry changed");
	std::array<sp<Vehicle>, 2> cars;
	for (size_t i = 0; i < cars.size(); i++)
	{
		const auto &record = cornerPair[i];
		auto v = city->placeVehicle(state, {&state, record.type}, state.getPlayer(), record.pose,
		                            record.facing);
		TEST_REQUIRE(
		    v && v->setMission(state, VehicleMission::gotoLocation(state, *v, record.target)) &&
		        !v->missions.empty(),
		    "could not place corner-pair car {0}", record.observedId);
		auto &m = v->missions.front();
		m.currentPlannedPath = record.path;
		m.roadStoppedTicks = record.stopped;
		m.blockedWaitTicks = 8;
		v->goalPosition = v->position;
		v->velocity = {0, 0, 0};
		cars[i] = v;
	}
	TEST_REQUIRE(cars[0]->missions.front().roadBlocker(*cars[0], a, b, 0).get() == cars[1].get() &&
	                 cars[1]->missions.front().roadBlocker(*cars[1], b, a, 3).get() ==
	                     cars[0].get(),
	             "captured corner pair did not reproduce reciprocal blockers");
	std::array<bool, 2> moved{}, completed{};
	for (unsigned ticks = 0; ticks < TICKS_PER_HOUR; ticks += STEP)
	{
		for (size_t i = 0; i < cars.size(); i++)
		{
			auto v = cars[i];
			v->update(state, STEP);
			TEST_REQUIRE(!v->isDead() && !v->stranded && v->tileObject,
			             "corner-pair car disappeared instead of recovering");
			const auto d = v->position - cornerPair[i].pose;
			moved[i] = moved[i] || d.x * d.x + d.y * d.y >= 4;
			completed[i] = v->missions.empty() &&
			               v->tileObject->getOwningTile()->position == cornerPair[i].target;
		}
		state.gameTime.addTicks(STEP);
		if (completed[0] && completed[1])
			break;
	}
	for (size_t i = 0; i < cars.size(); i++)
	{
		TEST_CHECK(moved[i], "corner-pair {0} failed to move two tiles", cornerPair[i].observedId);
		TEST_CHECK(completed[i], "corner-pair {0} did not finish original target {1}",
		           cornerPair[i].observedId, cornerPair[i].target);
	}
	return true;
}

bool test_recovery_prefix_preserves_real_blockers_and_future_route()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load recovery-prefix map");
	auto city = state.current_city;
	auto v = city->placeVehicle(state, {&state, "VEHICLETYPE_AUTOTAXI"}, state.getPlayer(),
	                            {83.5f, 39.640625f, 2}, 1.5707963f);
	auto blocker = city->placeVehicle(state, {&state, "VEHICLETYPE_CIVILIAN_CAR"},
	                                  state.getPlayer(), {84.5f, 39.359375f, 2}, 4.712389f);
	TEST_REQUIRE(v && blocker, "could not place prefix crossing control");
	TEST_REQUIRE(v->setMission(state, VehicleMission::gotoLocation(state, *v, {84, 37, 2})) &&
	                 !v->missions.empty() &&
	                 blocker->setMission(
	                     state, VehicleMission::gotoLocation(state, *blocker, {82, 39, 2})) &&
	                 !blocker->missions.empty(),
	             "could not create prefix crossing missions");
	blocker->missions.front().currentPlannedPath = {{84, 39, 2}, {83, 39, 2}, {82, 39, 2}};
	auto &m = v->missions.front();
	// This is a minimal inferred route shape on real connected roads. Trim the leading
	// A->B->A, then still honor the crossing conflict at the next real C (84,39).
	m.currentPlannedPath = {{83, 39, 2}, {82, 39, 2}, {83, 39, 2},
	                        {84, 39, 2}, {84, 38, 2}, {84, 37, 2}};
	Vec3<float> destination;
	float facing = 0;
	int turbo = 0;
	TEST_REQUIRE(m.advanceAlongPath(state, *v, destination, facing, turbo),
	             "prefix control supplied no movement goal");
	TEST_CHECK(destination == v->position && m.currentPlannedPath.front() == Vec3<int>(83, 39, 2) &&
	               m.currentPlannedPath[1] == Vec3<int>(84, 39, 2),
	           "prefix repair removed the next real junction admission check to {0}", destination);
	// A future A->B->A is not the current-tile prefix. Keep it: this normalization is
	// not a general path optimizer, and the blocked crossing still supplies the wait goal.
	const std::deque<Vec3<int>> future = {
	    {83, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 39, 2}, {85, 39, 2}};
	m.currentPlannedPath = future;
	m.blockedWaitTicks = 0;
	m.roadStoppedTicks = 0;
	TEST_REQUIRE(m.advanceAlongPath(state, *v, destination, facing, turbo),
	             "future-loop control supplied no goal");
	TEST_CHECK(destination == v->position && m.currentPlannedPath == future,
	           "current-prefix repair altered a later excursion or bypassed its blocker");
	// Reproduce the faulty recovery builder: from65 headingEast, retreat to the T at64.
	// Its South branch's cached shortest route starts64,47->64,46, immediately undoing
	// the chosen branch. A fallback ending at the retreat junction is allowed; targetLocation
	// must remain94,31 so ordinary mission restart can plan the remaining journey.
	VehicleMission recovery;
	const Vec3<int> target{94, 31, 2};
	recovery.type = VehicleMission::MissionType::GotoLocation;
	recovery.targetLocation = target;
	recovery.currentPlannedPath = {{65, 46, 2}, target};
	TEST_REQUIRE(recovery.roadUTurn(*v, city->map->getTile(Vec3<int>{65, 46, 2}), 1),
	             "recovery builder could not retreat to the real T-junction");
	TEST_REQUIRE(recovery.currentPlannedPath.size() >= 2,
	             "recovery builder dropped its retreat path");
	TEST_CHECK(recovery.currentPlannedPath[0] == Vec3<int>(65, 46, 2) &&
	               recovery.currentPlannedPath[1] == Vec3<int>(64, 46, 2) &&
	               recovery.targetLocation == target,
	           "recovery changed its original target or lost the retreat edge");
	for (size_t i = 0; i + 2 < recovery.currentPlannedPath.size(); i++)
		TEST_CHECK(recovery.currentPlannedPath[i] != recovery.currentPlannedPath[i + 2],
		           "recovery builder forced an immediate branch return {0}->{1}->{0}",
		           recovery.currentPlannedPath[i], recovery.currentPlannedPath[i + 1]);
	return true;
}

bool test_junction_eligibility_and_geometric_rejections()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load eligibility map");
	auto city = state.current_city;
	auto v = city->placeVehicle(state, {&state, "VEHICLETYPE_AUTOTAXI"}, state.getPlayer(),
	                            {84.5f, 39.359375f, 2}, 0);
	TEST_REQUIRE(v, "could not place eligibility vehicle");
	auto t = city->map->getTile(Vec3<int>{84, 39, 2});
	TEST_REQUIRE(t->presentScenery->type->road_type == SceneryTileType::RoadType::Junction,
	             "observed T is no longer a junction");
	// Use independent missions: roadUTurn changes a route. A sprite-facing restriction would
	// reject this north-facing saved pose even though its westbound route can recover.
	for (int heading : {1, 3})
	{
		VehicleMission m;
		m.currentPlannedPath = {{84, 39, 2}, {83, 39, 2}, {82, 39, 2}};
		TEST_CHECK(m.roadUTurn(*v, t, heading),
		           "T-junction rejected connected opposite heading {0}", heading);
	}
	Tile *cross = nullptr, *corner = nullptr;
	for (const auto &[position, type] : city->initial_tiles)
	{
		if (type->tile_type != SceneryTileType::TileType::Road || type->connection.size() < 4)
			continue;
		auto tile = city->map->getTile(position);
		if (!tile->presentScenery || tile->presentScenery->destroyed)
			continue;
		const auto &c = type->connection;
		if (!cross && c[0] && c[1] && c[2] && c[3])
			cross = tile;
		if (!corner && c[0] && c[1] && !c[2] && !c[3])
			corner = tile;
		if (cross && corner)
			break;
	}
	TEST_REQUIRE(cross && corner, "no real crossroad/corner for eligibility controls");
	for (int heading = 0; heading < 4; heading++)
	{
		VehicleMission m;
		m.currentPlannedPath = {cross->position, {82, 39, 2}};
		TEST_CHECK(m.roadUTurn(*v, cross, heading), "crossroad rejected opposite heading {0}",
		           heading);
	}
	VehicleMission cornerMission;
	cornerMission.currentPlannedPath = {corner->position, {82, 39, 2}};
	const auto oldCornerPath = cornerMission.currentPlannedPath;
	TEST_CHECK(!cornerMission.roadUTurn(*v, corner, 0) &&
	               cornerMission.currentPlannedPath == oldCornerPath,
	           "corner recovery invented a missing opposite connection");
	// Isolate the terminal flag from connectivity. This is a controlled rule variation, not
	// claimed to be observed map data; a modded terminal must still reject recovery.
	const auto oldType = t->presentScenery->type;
	auto terminal = mksp<SceneryTileType>();
	terminal->tile_type = SceneryTileType::TileType::Road;
	terminal->connection = oldType->connection;
	terminal->road_type = SceneryTileType::RoadType::Terminal;
	state.scenery_tile_types["CITYTILE_UTURN_TEST_TERMINAL"] = terminal;
	t->presentScenery->type = {&state, "CITYTILE_UTURN_TEST_TERMINAL"};
	VehicleMission terminalMission;
	terminalMission.currentPlannedPath = {{84, 39, 2}, {83, 39, 2}, {82, 39, 2}};
	const auto oldTerminalPath = terminalMission.currentPlannedPath;
	TEST_CHECK(!terminalMission.roadUTurn(*v, t, 3) &&
	               terminalMission.currentPlannedPath == oldTerminalPath,
	           "terminal road recovered despite its terminal flag");
	t->presentScenery->type = oldType;
	return true;
}

bool test_goto_duplicates_preserve_lane_and_blocking()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load duplicate-route map");
	auto city = state.current_city;
	auto v = city->placeVehicle(state, {&state, "VEHICLETYPE_AUTOTAXI"}, state.getPlayer(),
	                            {83.5f, 39.640625f, 2}, 1.5707963f);
	TEST_REQUIRE(v, "could not place duplicate-route vehicle");
	const Vec3<int> target{86, 39, 2};
	TEST_REQUIRE(v->setMission(state, VehicleMission::gotoLocation(state, *v, target)) &&
	                 !v->missions.empty(),
	             "could not create natural road route");
	auto &m = v->missions.front();
	TEST_REQUIRE(m.currentPlannedPath.size() > 2 &&
	                 m.currentPlannedPath[0] == Vec3<int>(83, 39, 2) &&
	                 m.currentPlannedPath[1] == m.currentPlannedPath[0],
	             "natural goto no longer includes its repeated current tile");
	auto independent = m;
	Vec3<float> destination;
	float facing = 0;
	int turbo = 0;
	TEST_REQUIRE(independent.advanceAlongPath(state, *v, destination, facing, turbo),
	             "natural goto supplied no goal");
	TEST_CHECK(destination == Vec3<float>(84.5f, 39.640625f, 2),
	           "repeated current node forced an unnecessary center goal: {0}", destination);
	for (unsigned ticks = 0; ticks < 2 * TICKS_PER_MINUTE && !v->missions.empty(); ticks += STEP)
		v->update(state, STEP);
	TEST_REQUIRE(v->tileObject && v->missions.empty() &&
	                 v->tileObject->getOwningTile()->position == target,
	             "deduplicating the natural route dropped its final goal");
	// A duplicated junction node must not hide the route's actual north exit. Eastbound straight
	// clears a westbound occupant, but an east-to-north turn crosses it (matrix [3][13]).
	v->setPosition({83.5f, 39.640625f, 2});
	TEST_REQUIRE(v->setMission(state, VehicleMission::gotoLocation(state, *v, {84, 37, 2})) &&
	                 !v->missions.empty(),
	             "could not create duplicated crossing route");
	auto &crossing = v->missions.front();
	crossing.currentPlannedPath = {{83, 39, 2}, {84, 39, 2}, {84, 39, 2}, {84, 38, 2}, {84, 37, 2}};
	v->goalPosition = v->position;
	v->velocity = {0, 0, 0};
	auto blocker = city->placeVehicle(state, {&state, "VEHICLETYPE_CIVILIAN_CAR"},
	                                  state.getPlayer(), {84.5f, 39.359375f, 2}, 4.712389f);
	TEST_REQUIRE(blocker, "could not place the crossing lane occupant");
	TEST_REQUIRE(
	    blocker->setMission(state, VehicleMission::gotoLocation(state, *blocker, {82, 39, 2})) &&
	        !blocker->missions.empty(),
	    "could not create westbound blocker route");
	blocker->missions.front().currentPlannedPath = {{84, 39, 2}, {83, 39, 2}, {82, 39, 2}};
	TEST_REQUIRE(crossing.roadBlocker(*v, city->map->getTile(Vec3<int>{83, 39, 2}),
	                                  city->map->getTile(Vec3<int>{84, 39, 2}), 0)
	                     .get() == blocker.get(),
	             "duplicate fixture does not contain a crossing conflict");
	TEST_REQUIRE(crossing.advanceAlongPath(state, *v, destination, facing, turbo),
	             "crossing route supplied no wait goal");
	TEST_CHECK(destination == v->position &&
	               crossing.currentPlannedPath.front() == Vec3<int>(83, 39, 2),
	           "duplicated junction node bypassed a crossing blocker to {0}", destination);
	return true;
}

bool test_road_takeoff_keeps_authored_entrance_move_and_lands()
{
	GameState state;
	TEST_REQUIRE(loadCity(state), "could not load entrance-movement map");
	auto city = state.current_city;
	StateRef<VehicleType> type{&state, "VEHICLETYPE_AUTOTAXI"};
	StateRef<Building> building;
	for (const auto &candidate : city->buildings)
		if (candidate && city->hasVehicleAccess(*candidate, *type))
		{
			building = candidate;
			break;
		}
	TEST_REQUIRE(building, "no accessible real road entrance");
	auto v = city->placeVehicle(state, type, state.getPlayer(), building);
	TEST_REQUIRE(v && v->currentBuilding == building, "could not park the departure vehicle");
	VehicleMission takeoff;
	takeoff.type = VehicleMission::MissionType::TakeOff;
	TEST_REQUIRE(v->setMission(state, takeoff), "could not start real takeoff");
	TEST_REQUIRE(v->tileObject && !v->missions.empty() &&
	                 v->missions.front().type == VehicleMission::MissionType::TakeOff,
	             "takeoff did not launch onto the road entrance");
	const auto entrance = building->carEntranceLocation;
	const auto center = city->map->getTile(entrance)->getRestingPosition();
	const auto launchPosition = v->position;
	TEST_REQUIRE(v->missions.front().currentPlannedPath ==
	                     std::deque<Vec3<int>>({entrance, entrance}) &&
	                 launchPosition != center,
	             "real takeoff no longer authors the two-node entrance centering move");
	v->update(state, STEP);
	TEST_CHECK(v->goalPosition == center, "takeoff skipped the physical entrance goal to {0}",
	           v->goalPosition);
	for (unsigned ticks = STEP; ticks < 2 * TICKS_PER_MINUTE && !v->missions.empty(); ticks += STEP)
		v->update(state, STEP);
	TEST_REQUIRE(v->tileObject && v->missions.empty() && v->position == center &&
	                 v->position != launchPosition,
	             "takeoff completed without moving from the door to its entrance goal");
	// Road landing has no authored movement path: the real Land start enters the depot only
	// from this road entrance. Preserve the callback and its parked-vehicle membership.
	VehicleMission landing;
	landing.type = VehicleMission::MissionType::Land;
	landing.targetBuilding = building;
	TEST_REQUIRE(v->setMission(state, landing), "could not start road landing");
	v->update(state, STEP);
	TEST_CHECK(!v->isDead() && !v->tileObject && v->currentBuilding == building &&
	               building->currentVehicles.count({&state, v}) == 1,
	           "road landing lost or destroyed the vehicle instead of returning it to its depot");
	return true;
}
} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
		return EXIT_FAILURE;
	applyDeterministicTestConfig();
	commonPath = config().getString("common");
	gamePath = config().getString("gamestate");
	if (commonPath.empty() || gamePath.empty())
		return EXIT_FAILURE;
	Framework fw("OpenApoc", false);
	return runTestSuite({
	    {"observed_cycle_finishes_original_targets", test_observed_cycle_finishes_original_targets},
	    {"observed_corner_pair_finishes_original_targets",
	     test_observed_corner_pair_finishes_original_targets},
	    {"recovery_prefix_preserves_real_blockers_and_future_route",
	     test_recovery_prefix_preserves_real_blockers_and_future_route},
	    {"junction_eligibility_and_geometric_rejections",
	     test_junction_eligibility_and_geometric_rejections},
	    {"goto_duplicates_preserve_lane_and_blocking",
	     test_goto_duplicates_preserve_lane_and_blocking},
	    {"road_takeoff_keeps_authored_entrance_move_and_lands",
	     test_road_takeoff_keeps_authored_entrance_move_and_lands},
	});
}
