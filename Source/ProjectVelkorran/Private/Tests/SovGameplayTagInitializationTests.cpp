// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_DominionHandler.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Navigation/MapMarker.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Class.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovGameplayTagInitializationTest,
	"ProjectVelkorran.Foundation.GameplayTags.NativeCDOAndRepeatedInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGameplayTagInitializationTest::RunTest(const FString& Parameters)
{
	// These CDOs retain constructor-time copies. Registering tags later cannot
	// repair an empty identity, input, or marker domain already stored in a CDO.
	const UMapMarker* Marker = GetDefault<UMapMarker>();
	const USovGameplayAbility_DominionHandlerCommandHound* CommandAbility =
		GetDefault<USovGameplayAbility_DominionHandlerCommandHound>();
	TestNotNull(TEXT("Native map marker CDO exists"), Marker);
	TestNotNull(TEXT("Native Handler command CDO exists"), CommandAbility);
	if (!Marker || !CommandAbility)
	{
		return false;
	}

	const FGameplayTag InputTag =
		FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	const FGameplayTag IdentityTag =
		FSovGameplayTags::Get().Ability_NPC_DominionHandler_CommandHound;
	const FGameplayTag CompassTag =
		FNavigatorGameplayTags::Get().NavigatorTypes_Compass;
	const FGameplayTag MinimapTag =
		FNavigatorGameplayTags::Get().NavigatorTypes_Minimap;
	const FGameplayTag WorldmapTag =
		FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap;

	TestEqual(TEXT("Narrative input retains its registered name"),
		InputTag.GetTagName(), FName(TEXT("Narrative.Input.Ability1")));
	TestEqual(TEXT("Sovereign ability retains its registered name"),
		IdentityTag.GetTagName(),
		FName(TEXT("Sov.Ability.NPC.DominionHandler.CommandHound")));
	TestEqual(TEXT("Navigator compass retains its registered name"),
		CompassTag.GetTagName(), FName(TEXT("Navigator.NavigatorTypes.Compass")));

	TestTrue(TEXT("Handler CDO captured its native ability identity"),
		CommandAbility->GetHandlerCommandAbilityTag() == IdentityTag);
	TestTrue(TEXT("Handler CDO captured its native input"),
		CommandAbility->GetConfiguredInputTag() == InputTag);
	TestTrue(TEXT("Handler CDO publishes its native GAS asset tag"),
		CommandAbility->GetAssetTags().HasTagExact(IdentityTag));
	TestTrue(TEXT("Map marker CDO captured its default compass domain"),
		Marker->HasDomain(CompassTag));
	TestTrue(TEXT("Map marker CDO captured its default minimap domain"),
		Marker->HasDomain(MinimapTag));
	TestTrue(TEXT("Map marker CDO captured its default world map domain"),
		Marker->HasDomain(WorldmapTag));

	// Automation runs after engine startup has finalized native tags. Repeating
	// initialization must be a no-op; a late registration ensure remains a test
	// failure, and the stored singleton values must not be replaced with empties.
	for (int32 Repeat = 0; Repeat < 2; ++Repeat)
	{
		FNarrativeGameplayTags::InitializeNativeTags();
		FSovGameplayTags::InitializeNativeTags();
		FNavigatorGameplayTags::InitializeNativeTags();
	}

	TestTrue(TEXT("Repeated initialization preserves Narrative input"),
		FNarrativeGameplayTags::Get().Narrative_Input_Ability1 == InputTag);
	TestTrue(TEXT("Repeated initialization preserves Sovereign identity"),
		FSovGameplayTags::Get().Ability_NPC_DominionHandler_CommandHound == IdentityTag);
	TestTrue(TEXT("Repeated initialization preserves Navigator compass"),
		FNavigatorGameplayTags::Get().NavigatorTypes_Compass == CompassTag);
	TestTrue(TEXT("Repeated initialization preserves Navigator minimap"),
		FNavigatorGameplayTags::Get().NavigatorTypes_Minimap == MinimapTag);
	TestTrue(TEXT("Repeated initialization preserves Navigator world map"),
		FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap == WorldmapTag);
	TestTrue(TEXT("Persistent Handler CDO still matches the singleton identity"),
		CommandAbility->GetHandlerCommandAbilityTag()
			== FSovGameplayTags::Get().Ability_NPC_DominionHandler_CommandHound);
	TestTrue(TEXT("Persistent Handler CDO still matches the singleton input"),
		CommandAbility->GetConfiguredInputTag()
			== FNarrativeGameplayTags::Get().Narrative_Input_Ability1);
	TestTrue(TEXT("Persistent map marker CDO still matches the singleton domain"),
		Marker->HasDomain(FNavigatorGameplayTags::Get().NavigatorTypes_Compass));
	return true;
}

#endif
