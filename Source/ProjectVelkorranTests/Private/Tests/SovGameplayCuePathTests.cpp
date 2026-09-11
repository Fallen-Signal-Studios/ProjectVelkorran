// Copyright Fallen Signal Studios. All Rights Reserved.
// Guards the explicit GameplayCueNotifyPaths configuration.
//
// With no paths configured the engine falls back to scanning all of /Game/ for cue
// notifies, which on this project is every asset. An asset-registry enumeration by native
// parent class found 22 cue notifies and zero outside /Game/Cues and
// /NarrativePro/Pro/Core/Abilities/Cues, so the set was narrowed. These tests fail if that
// configuration is lost, if the broad fallback returns, or if a cue root is dropped.
//
// They deliberately assert the resolved runtime value rather than the ini text, so a
// config that parses but does not reach AbilitySystemGlobals still fails.
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
const TCHAR* ProjectCueRoot = TEXT("/Game/Cues");
const TCHAR* NarrativeCueRoot = TEXT("/NarrativePro/Pro/Core/Abilities/Cues");
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGameplayCuePathsNarrowed, "ProjectVelkorran.Campaign.Validation.GameplayCuePathsNarrowed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovGameplayCuePathsNarrowed::RunTest(const FString& Parameters)
{
	const TArray<FString> Paths = UAbilitySystemGlobals::Get().GetGameplayCueNotifyPaths();
	if (!TestFalse(TEXT("Explicit cue notify paths are configured"), Paths.IsEmpty()))
	{
		// An empty array is exactly the state that triggers the engine's scan of all /Game/.
		return false;
	}
	TestTrue(TEXT("The project cue root is scanned"), Paths.Contains(ProjectCueRoot));
	// The fork's cues are the originals the project's copies override, and they share the
	// same GameplayCue tags. Scanning both registers duplicates, and the loser of the race
	// is skipped - measured resolving inconsistently across tags, discarding some project
	// overrides. The engine's old fallback scanned /Game/ only, so the project always won;
	// keeping the fork root out preserves that.
	TestFalse(TEXT("The Narrative fork cue root is deliberately not scanned"),
		Paths.Contains(NarrativeCueRoot));
	// A bare /Game entry restores the full-project scan even alongside narrow roots.
	TestFalse(TEXT("No entry restores the full /Game scan"),
		Paths.Contains(TEXT("/Game")) || Paths.Contains(TEXT("/Game/")));
	for (const FString& Path : Paths)
	{
		TestTrue(FString::Printf(TEXT("Configured cue path '%s' is a content root, not the project root"), *Path),
			Path.StartsWith(TEXT("/Game/")));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGameplayCuesResolve, "ProjectVelkorran.Campaign.Validation.GameplayCuesStillResolve", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovGameplayCuesResolve::RunTest(const FString& Parameters)
{
	// Narrowing the scan is only correct if the cues it is supposed to find are still
	// found. Asserting a count rather than a list keeps this stable as cues are authored,
	// while still failing if a root stops being scanned entirely.
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestNotNull(TEXT("A gameplay cue manager exists"), Manager)) { return false; }
	const UGameplayCueSet* CueSet = Manager->GetRuntimeCueSet();
	if (!TestNotNull(TEXT("A runtime cue set exists"), CueSet)) { return false; }
	TestTrue(TEXT("The narrowed scan still discovers cue notifies"), CueSet->GameplayCueData.Num() > 0);
	int32 ProjectCues = 0;
	int32 NarrativeCues = 0;
	TSet<FGameplayTag> SeenTags;
	int32 DuplicateTags = 0;
	for (const FGameplayCueNotifyData& Data : CueSet->GameplayCueData)
	{
		const FString Path = Data.GameplayCueNotifyObj.ToString();
		if (Path.StartsWith(ProjectCueRoot)) { ++ProjectCues; }
		else if (Path.StartsWith(NarrativeCueRoot)) { ++NarrativeCues; }
		bool bAlreadySeen = false;
		SeenTags.Add(Data.GameplayCueTag, &bAlreadySeen);
		if (bAlreadySeen) { ++DuplicateTags; }
	}
	TestTrue(TEXT("Cues from the project root resolve"), ProjectCues > 0);
	TestEqual(TEXT("No fork cue is registered alongside the project's override"), NarrativeCues, 0);
	// Two notifies claiming one tag means the engine silently skips one of them, and which
	// one survives depends on load order.
	TestEqual(TEXT("No gameplay cue tag is served by two notifies"), DuplicateTags, 0);
	return true;
}
#endif
