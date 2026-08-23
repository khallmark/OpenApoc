#include "framework/framework.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/baselayout.h"
#include "game/state/rules/city/ufopaedia.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::extractResearch(GameState &state) const
{
	auto &data = this->ufo2p;
	for (unsigned i = 0; i < data.research_data->count(); i++)
	{
		auto rdata = data.research_data->get(i);

		auto r = mksp<ResearchTopic>();

		r->name = data.research_names->get(i);
		auto id = ResearchTopic::getPrefix() + canon_string(r->name);
		r->description = data.research_descriptions->get(i);
		r->ufopaedia_entry = "";
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
		// FIXME: this assumed all listed techs are required, which is not true for some topics
		// (It's possible that an unknown member in ResearchData marks this, or it's done
		// in-code)
		// This should be fixed up in the patch.

		ResearchDependency dependency;
		dependency.type = ResearchDependency::Type::All;

		for (int pre = 0; pre < 3; pre++)
		{

			if (rdata.prereqTech[pre] != 0xffff)
			{
				auto prereqId = ResearchTopic::getPrefix() +
				                canon_string(data.research_names->get(rdata.prereqTech[pre]));
				dependency.topics.emplace(StateRef<ResearchTopic>{&state, prereqId});
			}
		}

		r->dependencies.research.push_back(dependency);

		/*ItemDependency itemdep;
		itemdep.agentItemsRequired[{&state, "AEQUIPMENTTYPE_PSICLONE"}] = 1;
		r->dependencies.items.push_back(itemdep);*/

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

} // namespace OpenApoc
