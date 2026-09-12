// Compiles the same path rules the campaign forbidden-content validation uses.
#include "Validation/SovCampaignContentPolicy.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
using namespace SovCampaignContentPolicy;

bool DemoPath(const std::string& Text)
{
	return IsDemoContentPath(Text.c_str(), Text.size());
}
}

int main()
{
	int Checks = 0;
	auto Check = [&Checks](bool Condition) { assert(Condition); ++Checks; };

	// Folding is ASCII-only and leaves everything else alone.
	Check(FoldCase('A') == 'a');
	Check(FoldCase('Z') == 'z');
	Check(FoldCase('a') == 'a');
	Check(FoldCase('/') == '/');
	Check(FoldCase('0') == '0');
	Check(FoldCase('_') == '_');

	// Containment basics, including the boundaries a hand-written scan gets wrong.
	Check(ContainsFolded("abcdef", 6, "abc"));
	Check(ContainsFolded("abcdef", 6, "def"));   // match ending exactly at the end
	Check(ContainsFolded("abcdef", 6, "cd"));
	Check(ContainsFolded("abcdef", 6, ""));      // empty needle is contained
	Check(!ContainsFolded("abcdef", 6, "abcdefg"));  // needle longer than text
	Check(!ContainsFolded("abcdef", 6, "xyz"));
	const char* NullText = nullptr;
	Check(!ContainsFolded(NullText, 0, "abc"));
	Check(!ContainsFolded(NullText, 5, "abc"));
	Check(!ContainsFolded("abc", 3, nullptr));
	Check(ContainsFolded("abcdef", 6, "ABCDEF"));
	Check(ContainsFolded("ABCDEF", 6, "abcdef"));
	// Length is honoured over any terminator, so a truncated view cannot match past its end.
	Check(!ContainsFolded("abcdef", 3, "def"));
	Check(ContainsFolded("abcdef", 3, "abc"));

	// The demo root, in the spellings a package path actually arrives in.
	Check(DemoPath("/NarrativePro/Pro/Demo/Quests/SecretMerchant/QBP_Demo_Narrative_SecretMerchant"));
	Check(DemoPath("/NarrativePro/Pro/Demo/Character/Definitions/Luca/Dialogue/DBP_Luca_SecretMerchant"));
	Check(DemoPath("/narrativepro/pro/demo/quests/x"));
	Check(DemoPath("/NARRATIVEPRO/PRO/DEMO/QUESTS/X"));
	Check(DemoPath("/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Firearms/Weapon_DemoPistol"));

	// Near misses must not match. The trailing slash is what separates the demo root from a
	// sibling directory whose name merely starts with "Demo".
	Check(!DemoPath("/NarrativePro/Pro/DemoItems/Thing"));
	Check(!DemoPath("/NarrativePro/Pro/Demonstration/Thing"));
	Check(!DemoPath("/NarrativePro/Pro/Core/Abilities/Cues/GC_TakeDamage"));
	Check(!DemoPath("/Game/Aurelion/Enemies/NPC_AurelionSecurityDrone"));
	Check(!DemoPath(""));
	Check(!DemoPath("/"));
	Check(!DemoPath("Demo"));

    // Assets that exist in tracked content and must NOT be classified by a keyword rule. These are
    // the concrete false positives a "shop"/"demo" keyword would produce: a rock mesh whose name
    // contains "shopk", level-prototyping props, and demo VFX the campaign may legitimately reuse.
	Check(!DemoPath("/NarrativePro/Pro/Core/VFX/Rock_shopk/S_Rock_shopk"));
	Check(!DemoPath("/Game/Aurelion/Art/Props/BP_SampleShop1"));
	// Demo VFX IS under the demo root, so the path rule alone matches it — which is exactly why the
	// engine-side caller must also require a quest or dialogue class before reporting a defect.
	Check(DemoPath("/NarrativePro/Pro/Demo/VFX/Epic/Niagara/StaticMesh/S_Rock_shopk"));

	// Every prefix of a real demo path either matches or does not, and never reads out of bounds.
	{
		const std::string Full = "/NarrativePro/Pro/Demo/Quests/SecretMerchant/QBP";
		for (std::size_t Length = 0; Length <= Full.size(); ++Length)
		{
			const bool Matched = IsDemoContentPath(Full.c_str(), Length);
			Check(Matched == (Length >= std::strlen("/NarrativePro/Pro/Demo/")));
		}
	}

	std::cout << Checks << " campaign content path checks passed\n";
	return 0;
}
