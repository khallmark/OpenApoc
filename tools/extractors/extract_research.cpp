#include "framework/framework.h"
#include "framework/image.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/city/baselayout.h"
#include "game/state/rules/city/ufopaedia.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "library/strings.h"
#include "library/strings_format.h"
#include "tools/extractors/common/research.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"
#include <algorithm>
#include <map>
#include <vector>

namespace OpenApoc
{

static void addResearchItemGate(GameState &state, const UFO2P &data, ItemDependency &items,
                                uint8_t type, uint16_t index, const UString &id)
{
	// FUN_000aab60 @ VA 0xAAB60 / file 0x10D204: type 0xFF returns 0xFF and
	// ignores the index. Any uses signed JG, so 0xFF does not count.
	if (type == 0xff)
	{
		return;
	}
	switch (type)
	{
		case 0:
			if (index >= data.vehicle_equipment_names->count())
			{
				LogError("research {0} craft-eq prereq {1} >= name table {2}", id, index,
				         data.vehicle_equipment_names->count());
				return;
			}
			items.vehicleItemsRequired[{&state, data.getVequipmentId(index)}] = 1;
			break;
		case 1:
			if (index >= data.agent_equipment_names->count())
			{
				LogError("research {0} agent-eq prereq {1} >= name table {2}", id, index,
				         data.agent_equipment_names->count());
				return;
			}
			items.agentItemsRequired[{&state, data.getAEquipmentId(index)}] = 1;
			break;
		case 3:
		{
			const auto itemId = ufo2pAlienLifeformItemId(index);
			if (itemId.empty())
			{
				LogError("research {0} alien-lifeform prereq {1} has no item", id, index);
				return;
			}
			StateRef<AEquipmentType> item{&state, itemId};
			items.agentItemsRequired[item] = 1;
			items.agentItemsConsumed[item] = 1;
			break;
		}
		default:
			LogError("Unexpected item-gate type {0:02x} for {1}", (unsigned)type, id);
	}
}

void InitialGameStateExtractor::extractResearch(GameState &state) const
{
	auto &data = this->ufo2p;
	const unsigned nameCount = data.research_names->count();
	const unsigned recCount = data.research_data->count();
	if (recCount != nameCount)
	{
		LogWarning("research_data count {0} != research_names {1}", recCount, nameCount);
	}
	const unsigned n = std::min(nameCount, recCount);

	// Duplicate EXE names must not share one topic. "Alien building" is ten
	// records (88–97); buildings already point at RESEARCH_ALIEN_BUILDING_0..9.
	// Other collisions (two "Overspawn Autopsy" rows) get _1, _2, …
	std::vector<UString> ids(n);
	std::map<UString, int> seenCanon;
	unsigned alienBuilding = 0;
	for (unsigned i = 0; i < n; i++)
	{
		const auto name = data.research_names->get(i);
		if (name == "Alien building")
		{
			ids[i] = format("RESEARCH_ALIEN_BUILDING_{0}", alienBuilding++);
			continue;
		}
		const auto base = ResearchTopic::getPrefix() + canon_string(name);
		const int seen = seenCanon[base]++;
		ids[i] = (seen == 0) ? base : format("{0}_{1}", base, seen);
	}

	for (unsigned i = 0; i < n; i++)
	{
		auto rdata = data.research_data->get(i);

		auto r = mksp<ResearchTopic>();

		r->name = data.research_names->get(i);
		const auto &id = ids[i];
		r->description = data.research_descriptions->get(i);
		r->ufopaedia_entry = "";
		r->ufopaediaGroup = rdata.ufopaediaGroup;
		r->ufopaediaEntry = rdata.ufopaediaEntry;
		r->man_hours = rdata.skillHours;
		r->man_hours_progress = 0;
		switch (rdata.researchGroup)
		{
			case 0:
				r->type = ResearchTopic::Type::BioChem;
				break;
			case 1:
				r->type = ResearchTopic::Type::Physics;
				break;
			default:
				LogError("Unexpected researchGroup {0:02x} for research item {1}",
				         (unsigned)rdata.researchGroup, id);
		}
		switch (rdata.labSize)
		{
			case 0:
				r->required_lab_size = ResearchTopic::LabSize::Small;
				break;
			case 1:
				r->required_lab_size = ResearchTopic::LabSize::Large;
				break;
			default:
				LogError("Unexpected labSize {0:02x} for research item {1}",
				         (unsigned)rdata.labSize, id);
		}
		// FUN_000aa7a8 @ VA 0xAA7A8 / file 0x10CE4C (ISO non-4). unknown2 (+2)
		// indexes jump table file 0x10CE38 (encoded 0x9A794): 0 All, 1 Any,
		// 2 (t0&&t1)||t2, 3 (t0||t1)&&t2, 4 XOR EBX (zeros the accumulator).
		// Records 36/43/45 are the only unknown2==1 rows. Case 4 stays unmapped
		// here; records 38/44 are applied after patch. unknown1==1 is item-Any.
		ResearchDependency dependency;
		dependency.type =
		    (rdata.unknown2 == 1) ? ResearchDependency::Type::Any : ResearchDependency::Type::All;

		for (int pre = 0; pre < 3; pre++)
		{
			const uint16_t tech = rdata.prereqTech[pre];
			if (tech == 0xffff)
			{
				continue;
			}
			if (tech >= ids.size() || ids[tech].empty())
			{
				LogError("research {0} prereqTech {1} has no topic id", id, tech);
				continue;
			}
			dependency.topics.emplace(StateRef<ResearchTopic>{&state, ids[tech]});
		}

		if (!dependency.topics.empty())
		{
			r->dependencies.research.push_back(dependency);
		}

		// FUN_000aa7a8 first switch is unknown1 (+1) @ file 0x10CE28: 0 All / 1 Any
		// of (prereqType, prereq), (unknown3.lo, leadsTo1), (unknown3.hi, leadsTo2).
		// Type 0xFF omits the slot. Records 62/63/70 are the only unknown1==1 rows.
		r->dependencies.items.type =
		    (rdata.unknown1 == 1) ? ItemDependency::Type::Any : ItemDependency::Type::All;
		addResearchItemGate(state, data, r->dependencies.items, rdata.prereqType, rdata.prereq, id);
		addResearchItemGate(state, data, r->dependencies.items,
		                    static_cast<uint8_t>(rdata.unknown3 & 0xff), rdata.leadsTo1, id);
		addResearchItemGate(state, data, r->dependencies.items,
		                    static_cast<uint8_t>((rdata.unknown3 >> 8) & 0xff), rdata.leadsTo2, id);

		r->score = rdata.score;

		if (state.research.topics.find(id) != state.research.topics.end())
		{
			LogError("Multiple research topics with ID \"{0}\"", id);
		}
		state.research.topics[id] = r;
	}

	// UFO2P non-4 ufopaedia_group at 0x152ADD: 10 names, last is "Alien Craft".
	// Entry→research unlocks stay in the patch (many:1). Ensure every EXE group exists.
	if (!data.ufopaedia_group || data.ufopaedia_group->count() != 10)
	{
		LogError("ufopaedia_group count {0} expected 10",
		         data.ufopaedia_group ? data.ufopaedia_group->count() : 0);
		return;
	}
	if (data.ufopaedia_group->get(9) != "Alien Craft")
	{
		LogError("ufopaedia_group[9] is \"{0}\" expected Alien Craft",
		         data.ufopaedia_group->get(9));
	}
	for (unsigned i = 0; i < data.ufopaedia_group->count(); i++)
	{
		const auto name = data.ufopaedia_group->get(i);
		const auto catID = UString("PAEDIACATEGORY_") + canon_string(name);
		if (state.ufopaedia.find(catID) != state.ufopaedia.end())
		{
			continue;
		}
		auto cat = mksp<UfopaediaCategory>();
		cat->title = name;
		state.ufopaedia[catID] = cat;
	}
}

static bool catalogMatchesDataType(unsigned cat, UfopaediaEntry::Data type)
{
	switch (cat)
	{
		case 0:
		case 9:
			return type == UfopaediaEntry::Data::Vehicle;
		case 1:
			return type == UfopaediaEntry::Data::Facility;
		case 2:
			return type == UfopaediaEntry::Data::VehicleEquipment;
		case 3:
			return type == UfopaediaEntry::Data::Equipment;
		case 4:
			return type == UfopaediaEntry::Data::Organisation;
		case 6:
			return type == UfopaediaEntry::Data::Nothing;
		case 7:
			return type == UfopaediaEntry::Data::Building;
		case 8:
			return type == UfopaediaEntry::Data::Building || type == UfopaediaEntry::Data::Nothing;
		default:
			return false;
	}
}

void InitialGameStateExtractor::applyUfopaediaStartVisible(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.ufopaedia_catalog || !data.ufopaedia_start_visible || !data.ufopaedia_pcx_names)
	{
		return;
	}

	std::map<UString, std::vector<sp<UfopaediaEntry>>> byPcx;
	for (auto &pair : state.ufopaedia_entries)
	{
		if (!pair.second || !pair.second->background)
		{
			continue;
		}
		UString path = to_lower(pair.second->background->path);
		const auto slash = path.find_last_of("/\\");
		if (slash != UString::npos)
		{
			path = path.substr(slash + 1);
		}
		byPcx[path].push_back(pair.second);
	}

	// Catalog pcxIndex is FUN_0008903c's PTR_s_V_TITLE slot (non-empty
	// strings only). StrTab keeps 8-byte padding nuls as empty entries.
	std::vector<std::string> pcxNames;
	for (unsigned i = 0; i < data.ufopaedia_pcx_names->count(); i++)
	{
		const auto name = data.ufopaedia_pcx_names->get(i);
		if (!name.empty())
		{
			pcxNames.push_back(name);
		}
	}

	const unsigned n = data.ufopaedia_catalog->count();
	for (unsigned i = 0; i < n; i++)
	{
		const auto row = data.ufopaedia_catalog->get(i);
		const auto cat = row.packed & 0xFFu;
		const auto idx = static_cast<uint16_t>(row.packed >> 16);
		if (idx == 0xFFFF)
		{
			continue;
		}
		if (row.pcxIndex >= pcxNames.size())
		{
			continue;
		}
		const auto &name = pcxNames[row.pcxIndex];
		if (name.empty() || name.find("INVALID") != std::string::npos)
		{
			continue;
		}
		const UString key = to_lower(name) + ".pcx";
		auto it = byPcx.find(key);
		if (it == byPcx.end() || it->second.size() != 1)
		{
			continue;
		}
		auto entry = it->second[0];
		if (!catalogMatchesDataType(cat, entry->data_type))
		{
			continue;
		}
		entry->startVisibleFromExe = true;
		entry->catalogCategory = cat;
		entry->catalogIndex = idx;
		entry->startVisible = (i < data.ufopaedia_start_visible->count() &&
		                       data.ufopaedia_start_visible->get(i) != 0);
	}
}

static void replaceResearchGate(GameState &state, const UString &id, ResearchDependency::Type type,
                                const char *const *topics, size_t topicCount)
{
	auto it = state.research.topics.find(id);
	if (it == state.research.topics.end() || !it->second)
	{
		LogError("research {0} missing for FUN_000aa7a8 hardcoded gate", id);
		return;
	}
	ResearchDependency dependency;
	dependency.type = type;
	for (size_t i = 0; i < topicCount; i++)
	{
		const UString topicId = topics[i];
		if (state.research.topics.find(topicId) == state.research.topics.end())
		{
			LogError("research {0} gate topic {1} missing", id, topicId);
			continue;
		}
		dependency.topics.emplace(StateRef<ResearchTopic>{&state, topicId});
	}
	it->second->dependencies.research.clear();
	if (!dependency.topics.empty())
	{
		it->second->dependencies.research.push_back(dependency);
	}
}

void InitialGameStateExtractor::applyAa7a8HardcodedGates(GameState &state) const
{
	// After common_patch deletes prior-art copies. Listing, not decompiler:
	// SI==0x25 MOV EBX,1 at VA 0xAAAEA after All 12..17 and CMP [0xDE420].
	replaceResearchGate(state, "RESEARCH_THE_ALIEN_LIFE_CYCLE", ResearchDependency::Type::All,
	                    UFO2P_AA7A8_LIFE_CYCLE_ALL,
	                    sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL) / sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL[0]));
	replaceResearchGate(state, "RESEARCH_THE_REAL_ALIEN_THREAT", ResearchDependency::Type::All,
	                    UFO2P_AA7A8_THREAT_ALL,
	                    sizeof(UFO2P_AA7A8_THREAT_ALL) / sizeof(UFO2P_AA7A8_THREAT_ALL[0]));
	replaceResearchGate(state, "RESEARCH_ADVANCED_BIOCHEMISTRY_LAB", ResearchDependency::Type::Any,
	                    UFO2P_AA7A8_BIOCHEM_ANY,
	                    sizeof(UFO2P_AA7A8_BIOCHEM_ANY) / sizeof(UFO2P_AA7A8_BIOCHEM_ANY[0]));
}

} // namespace OpenApoc
