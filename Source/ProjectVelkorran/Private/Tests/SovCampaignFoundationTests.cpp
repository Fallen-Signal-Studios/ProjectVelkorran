// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneDeflection.h"
#include "Characters/SovDroneNPCBase.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Characters/SovSeleneCharacter.h"
#include "Characters/SovTarrikCharacter.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Framework/SovCampaignGameMode.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GAS/SovCombatTypes.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeGameState.h"

#include <type_traits>

static_assert(std::is_base_of_v<ASovNPCCharacterBase, ASovDroneNPCBase>);
static_assert(std::is_base_of_v<USovDismembermentComponent, USovDroneDismembermentComponent>);
static_assert(std::is_base_of_v<UActorComponent, USovDeflectionComponent>);
static_assert(!std::is_base_of_v<USovGuardComponent, USovDeflectionComponent>);
static_assert(std::is_same_v<std::underlying_type_t<ESovDefenseKind>, uint8>);
static_assert(std::is_same_v<
	decltype(FSovDamageResult{}.DefenseKind),
	ESovDefenseKind>);

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
	TestTrue(
		TEXT("Tarrik has no Selene Deflection"),
		Tarrik->FindComponentByClass<USovDeflectionComponent>() == nullptr);
	TestTrue(
		TEXT("Tarrik has no Selene Echo generator"),
		Tarrik->FindComponentByClass<USovSeleneEchoGenerationComponent>()
			== nullptr);
	TestNotNull(
		TEXT("Selene owns Deflection"),
		Selene->GetDeflectionComponent());
	TestNotNull(
		TEXT("Selene owns precision Echo generation"),
		Selene->GetSeleneEchoGenerationComponent());
	TArray<USovDeflectionComponent*> DeflectionComponents;
	Selene->GetComponents(DeflectionComponents);
	TArray<USovSeleneEchoGenerationComponent*> SeleneEchoGenerators;
	Selene->GetComponents(SeleneEchoGenerators);
	TestEqual(
		TEXT("Selene owns exactly one Deflection component"),
		DeflectionComponents.Num(),
		1);
	TestEqual(
		TEXT("Selene owns exactly one precision Echo generator"),
		SeleneEchoGenerators.Num(),
		1);

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
	FSovSeleneCoreLoopContractTest,
	"ProjectVelkorran.Campaign.Selene.CoreLoopContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovSeleneCoreLoopContractTest::RunTest(const FString& Parameters)
{
	const ASovSeleneCharacter* Selene = GetDefault<ASovSeleneCharacter>();
	const USovDeflectionComponent* Deflection = Selene
		? Selene->GetDeflectionComponent()
		: nullptr;
	const USovSeleneEchoGenerationComponent* EchoGeneration = Selene
		? Selene->GetSeleneEchoGenerationComponent()
		: nullptr;
	const USovWeakPointComponent* WeakPoint = GetDefault<USovWeakPointComponent>();
	const USovGameplayAbility_SeleneDeflection* DeflectionAbility =
		GetDefault<USovGameplayAbility_SeleneDeflection>();

	TestNotNull(TEXT("Selene CDO exists"), Selene);
	TestNotNull(TEXT("Deflection component exists"), Deflection);
	TestNotNull(TEXT("Selene Echo generator exists"), EchoGeneration);
	TestNotNull(TEXT("Weak Point component CDO exists"), WeakPoint);
	TestNotNull(TEXT("Deflection ability CDO exists"), DeflectionAbility);
	if (!Deflection || !EchoGeneration || !WeakPoint || !DeflectionAbility)
	{
		return false;
	}

	TestTrue(
		TEXT("Perfect Deflection window follows the Phase 1 prototype"),
		FMath::IsNearlyEqual(
			Deflection->GetPerfectDeflectionWindow(),
			0.11f));
	TestTrue(
		TEXT("Deflection requires prototype start Stamina"),
		FMath::IsNearlyEqual(
			Deflection->GetMinimumDeflectionStartStamina(),
			8.0f));
	TestTrue(
		TEXT("Perfect Deflection grants prototype Echo"),
		FMath::IsNearlyEqual(
			EchoGeneration->GetPerfectDeflectionEchoReward(),
			10.0f));
	TestTrue(
		TEXT("First unbroken weak point grants prototype Echo"),
		FMath::IsNearlyEqual(
			EchoGeneration->GetWeakPointBreakEchoReward(),
			8.0f));

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Deflection state tag is registered"), Tags.State_Deflecting.IsValid());
	TestTrue(
		TEXT("Perfect Deflection Echo source is registered"),
		Tags.Echo_Source_PerfectDeflection.IsValid());
	TestTrue(
		TEXT("Weak Point Echo source is registered"),
		Tags.Echo_Source_WeakPointBreak.IsValid());
	TestTrue(
		TEXT("Deflection ability carries its stable identity"),
		DeflectionAbility->GetAssetTags().HasTagExact(
			Tags.Ability_Defense_Selene_Deflection));

	TestTrue(
		TEXT("Defense result kinds remain explicit"),
		ESovDefenseKind::Guard != ESovDefenseKind::Deflection);
	const FSovDamageResult DefaultDamageResult;
	TestFalse(
		TEXT("Unresolved damage has no transaction identity"),
		DefaultDamageResult.TransactionId.IsValid());
	TestTrue(
		TEXT("Unresolved damage has no defense kind"),
		DefaultDamageResult.DefenseKind == ESovDefenseKind::None);
	TestFalse(TEXT("Unresolved damage is not Guarded"), DefaultDamageResult.bGuarded);
	TestFalse(TEXT("Unresolved damage is not Deflected"), DefaultDamageResult.bDeflected);
	TestFalse(
		TEXT("Unresolved damage is not a perfect defense"),
		DefaultDamageResult.bPerfectDefense);
	TestFalse(
		TEXT("Unresolved damage is not classified as Echo ability damage"),
		DefaultDamageResult.bFromEchoAbility);
	TestTrue(
		TEXT("An empty Weak Point component is a valid no-op"),
		WeakPoint->HasValidWeakPointConfiguration());

	const ASovDroneNPCBase* Drone = GetDefault<ASovDroneNPCBase>();
	TestNotNull(TEXT("Drone base CDO exists"), Drone);
	if (Drone)
	{
		TestTrue(
			TEXT("Drone remains inside the project NPC lifecycle"),
			Drone->GetClass()->IsChildOf(ASovNPCCharacterBase::StaticClass()));
		TestNotNull(
			TEXT("Drone retains combat sustain drops"),
			Drone->GetCombatSustainDropComponent());
		TestTrue(
			TEXT("Drone replaces humanoid dismemberment"),
			Drone->GetDismembermentComponent()
				&& Drone->GetDismembermentComponent()->IsA<
					USovDroneDismembermentComponent>());
		TestFalse(
			TEXT("Ordinary drone death explosion is opt-in"),
			Drone->IsDeathExplosionEnabledOnDeath());
	}

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
