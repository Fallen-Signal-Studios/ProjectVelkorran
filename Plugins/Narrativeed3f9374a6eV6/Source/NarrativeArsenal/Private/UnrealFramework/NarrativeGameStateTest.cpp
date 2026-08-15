// Copyright Narrative Tools 2025.


#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS 

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeGameStateFactionAttitudeTest, "NarrativeArsenal.UnrealFramework.NarrativeGameState.AttitudeTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNarrativeGameStateFactionAttitudeTest::RunTest(const FString& Parameters)
{
	ANarrativeGameState* GameState = NewObject<ANarrativeGameState>();
	FNarrativeGameplayTags Tags = FNarrativeGameplayTags::Get();

	// Hostile test
	GameState->SetFactionAttitude(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Heroes, ETeamAttitude::Hostile);
	GameState->SetFactionAttitude(Tags.Narrative_Factions_Heroes, Tags.Narrative_Factions_Bandits, ETeamAttitude::Friendly);
	UTEST_EQUAL(TEXT("Hostile: X hostile towards Y"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Heroes), ETeamAttitude::Hostile);
	UTEST_EQUAL(TEXT("Hostile: X friendly towards X"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Bandits), ETeamAttitude::Friendly);
	GameState->FactionAllianceMap.Empty();

	// Friendly test
	GameState->SetFactionAttitude(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Heroes, ETeamAttitude::Friendly);
	GameState->SetFactionAttitude(Tags.Narrative_Factions_Heroes, Tags.Narrative_Factions_Bandits, ETeamAttitude::Hostile);
	UTEST_EQUAL(TEXT("Friendly: X friendly towards Y"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Heroes), ETeamAttitude::Friendly);
	UTEST_EQUAL(TEXT("Friendly: X friendly towards X"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Bandits), ETeamAttitude::Friendly);
	GameState->FactionAllianceMap.Empty();

	// Neutral test
	UTEST_EQUAL(TEXT("X neutral towards Y"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_Bandits, Tags.Narrative_Factions_Heroes), ETeamAttitude::Neutral);

	// HostileAll test
	UTEST_EQUAL(TEXT("HostileAll: hostile towards everything"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_HostileAll, Tags.Narrative_Factions_Heroes), ETeamAttitude::Hostile);
	UTEST_EQUAL(TEXT("HostileAll: hostile towards own faction"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_HostileAll, Tags.Narrative_Factions_HostileAll), ETeamAttitude::Hostile);
	GameState->FactionAllianceMap.Empty();

	// FriendlyAll test
	UTEST_EQUAL(TEXT("FriendlyAll: friendly towards everything"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_FriendlyAll, Tags.Narrative_Factions_Heroes), ETeamAttitude::Friendly);
	UTEST_EQUAL(TEXT("FriendlyAll: friendly towards own faction"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_FriendlyAll, Tags.Narrative_Factions_FriendlyAll), ETeamAttitude::Friendly);
	GameState->FactionAllianceMap.Empty();

	// HostileOthers test
	UTEST_EQUAL(TEXT("HostileOthers: hostile towards other factions"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_HostileOthers, Tags.Narrative_Factions_Heroes), ETeamAttitude::Hostile);
	UTEST_EQUAL(TEXT("HostileOthers: friendly towards own faction"), GameState->GetFactionAttitudeTowardsFaction(Tags.Narrative_Factions_HostileOthers, Tags.Narrative_Factions_HostileOthers), ETeamAttitude::Friendly);
	GameState->FactionAllianceMap.Empty();
	
	return true;
}

#endif
