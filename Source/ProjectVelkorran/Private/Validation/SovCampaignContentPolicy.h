// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>

/**
 * Path rules behind the campaign's forbidden-content validation, kept free of Unreal so they are
 * exercised by Tests/Portable without an editor build.
 *
 * The governing constraint is the one the existing rule table already states: a keyword alone is
 * never sufficient. Narrative's demo root holds VFX, audio and meshes the campaign legitimately
 * reuses, and the tracked content includes a rock mesh named "Rock_shopk" — so a caller must
 * combine a path match with the asset's actual class before calling anything a defect.
 */
namespace SovCampaignContentPolicy
{
/** Narrative ships its demo tale content under this root. */
inline const char* DemoContentRoot() { return "/NarrativePro/Pro/Demo/"; }

template<typename Char> inline Char FoldCase(Char Character)
{
	return (Character >= Char('A') && Character <= Char('Z'))
		? static_cast<Char>(Character - Char('A') + Char('a'))
		: Character;
}

/**
 * Case-insensitive containment of Needle within Text. Written out rather than delegated so it
 * behaves identically for TCHAR and char, and so the portable suite exercises the same comparison
 * the engine-side caller uses.
 */
template<typename Char>
inline bool ContainsFolded(const Char* Text, std::size_t Length, const char* Needle)
{
	if (!Text || !Needle) { return false; }
	std::size_t NeedleLength = 0;
	while (Needle[NeedleLength] != '\0') { ++NeedleLength; }
	if (NeedleLength == 0) { return true; }
	if (Length < NeedleLength) { return false; }
	for (std::size_t Start = 0; Start + NeedleLength <= Length; ++Start)
	{
		std::size_t Index = 0;
		while (Index < NeedleLength
			&& FoldCase(Text[Start + Index]) == FoldCase(static_cast<Char>(Needle[Index])))
		{
			++Index;
		}
		if (Index == NeedleLength) { return true; }
	}
	return false;
}

/**
 * True when a package path lies under Narrative's demo root.
 *
 * This is a location test only. It is deliberately NOT a verdict: demo VFX, audio and meshes are
 * legitimately referenced by the campaign, so the caller must also confirm the asset is tale
 * content — a quest or a dialogue — before treating it as a forbidden system.
 */
template<typename Char> inline bool IsDemoContentPath(const Char* Path, std::size_t Length)
{
	return ContainsFolded(Path, Length, DemoContentRoot());
}
}
