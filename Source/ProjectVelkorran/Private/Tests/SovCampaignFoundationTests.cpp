// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovPlayerCharacterBase.h"
#include "Characters/SovSeleneCharacter.h"
#include "Characters/SovTarrikCharacter.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Framework/SovCampaignGameMode.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeGameState.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovCampaignCharacterCompositionTest,
	"ProjectVelkorran.Campaign.Foundation.CharacterComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovCampaignCharacterCompositionTest::RunTest(const FString& Parameters)
{
	const ASovPlayerCharacterBase* SharedCharacter =
		GetDefault<ASovPlayerCharacterBase>();
	const ASovTarrikCharacter* Tarrik = GetDefault<ASovTarrikCharacter>();
	const ASovSeleneCharacter* Selene = GetDefault<ASovSeleneCharacter>();

	TestNotNull(TEXT("Shared player CDO exists"), SharedCharacter);
	TestNotNull(TEXT("Tarrik CDO exists"), Tarrik);
	TestNotNull(TEXT("Selene CDO exists"), Selene);
	if (!SharedCharacter || !Tarrik || !Selene)
	{
		return false;
	}

	const auto TestSharedSystems = [this](
		const TCHAR* OwnerName,
		const ASovPlayerCharacterBase* Character)
	{
		TestNotNull(
			*FString::Printf(TEXT("%s owns Echo"), OwnerName),
			Character->GetEchoComponent());
		TestNotNull(
			*FString::Printf(TEXT("%s owns Shield"), OwnerName),
			Character->GetShieldComponent());
		TestNotNull(
			*FString::Printf(TEXT("%s owns Health recharge"), OwnerName),
			Character->GetHealthRechargeComponent());
		TestNotNull(
			*FString::Printf(TEXT("%s owns Poise"), OwnerName),
			Character->GetPoiseComponent());
	};

	TestSharedSystems(TEXT("Shared base"), SharedCharacter);
	TestSharedSystems(TEXT("Tarrik"), Tarrik);
	TestSharedSystems(TEXT("Selene"), Selene);

	TestTrue(
		TEXT("Shared base has no protagonist-specific Guard"),
		SharedCharacter->GetGuardComponent() == nullptr);
	TestTrue(
		TEXT("Shared base has no Tarrik Echo generator"),
		SharedCharacter->GetTarrikEchoGenerationComponent() == nullptr);
	TestNotNull(TEXT("Tarrik owns Guard"), Tarrik->GetGuardComponent());
	TestNotNull(
		TEXT("Tarrik owns Cinderline Echo generation"),
		Tarrik->GetTarrikEchoGenerationComponent());
	TestTrue(
		TEXT("Selene has no Tarrik Guard"),
		Selene->GetGuardComponent() == nullptr);
	TestTrue(
		TEXT("Selene has no Tarrik Echo generator"),
		Selene->GetTarrikEchoGenerationComponent() == nullptr);

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	TestTrue(
		TEXT("Tarrik class supplies its canonical identity"),
		Tarrik->GetProtagonistIdentityTag() == Tags.Character_Player_Tarrik);
	TestTrue(
		TEXT("Selene class supplies its canonical identity"),
		Selene->GetProtagonistIdentityTag() == Tags.Character_Player_Selene);
	TestTrue(
		TEXT("Tarrik identity is a player identity"),
		Tarrik->GetProtagonistIdentityTag().MatchesTag(Tags.Character_Player));
	TestTrue(
		TEXT("Selene identity is a player identity"),
		Selene->GetProtagonistIdentityTag().MatchesTag(Tags.Character_Player));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovCampaignFrameworkDefaultsTest,
	"ProjectVelkorran.Campaign.Foundation.FrameworkDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovCampaignFrameworkDefaultsTest::RunTest(const FString& Parameters)
{
	const ASovCampaignGameMode* GameMode = GetDefault<ASovCampaignGameMode>();
	TestNotNull(TEXT("Campaign GameMode CDO exists"), GameMode);
	if (!GameMode)
	{
		return false;
	}

	TestTrue(
		TEXT("Campaign GameMode owns the project PlayerController seam"),
		GameMode->PlayerControllerClass == ASovPlayerController::StaticClass());
	TestTrue(
		TEXT("Campaign GameMode owns the project PlayerState seam"),
		GameMode->PlayerStateClass == ASovPlayerState::StaticClass());
	TestTrue(
		TEXT("Campaign GameMode retains Narrative GameState behavior"),
		GameMode->GameStateClass == ANarrativeGameState::StaticClass());
	TestTrue(
		TEXT("Native campaign fallback pawn is Tarrik"),
		GameMode->DefaultPawnClass == ASovTarrikCharacter::StaticClass());

	return true;
}

#endif
