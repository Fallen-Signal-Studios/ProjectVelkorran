// Copyright Fallen Signal Studios. All Rights Reserved.
// Regression coverage for the commander-death sever defect.
//
// A completed sever is a durable transaction: it records a transaction id, retains a
// receipt, and may already have paid a Selene Echo reward. Losing the command source
// afterwards used to run DeactivateWithoutSever, which set the link Inactive and called
// ClearAllParticipantContributions - withdrawing the earned Severed tag from every living
// participant and allowing the same link to be activated and severed a second time.
//
// Both source-loss entry points are covered here: ASC death notification and actor
// destruction. The active-link path is asserted too, so the fix cannot silently disable
// legitimate deactivation.
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovCoordinationRuntimeTestFixtures.h"
#include "Components/SovCommandLinkComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FSourceLossWorld
{
	UWorld* World = nullptr;
	ASovCoordinationTestNPC* Commander = nullptr;
	ASovCoordinationTestNPC* FirstLinked = nullptr;
	ASovCoordinationTestNPC* SecondLinked = nullptr;
	ASovBotTestCharacter* Severer = nullptr;
	USovCommandLinkComponent* Link = nullptr;

	FSourceLossWorld()
	{
		const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		Commander = Spawn(FVector::ZeroVector);
		FirstLinked = Spawn(FVector(200., 0., 0.));
		SecondLinked = Spawn(FVector(400., 0., 0.));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Severer = World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(),
			FVector(600., 0., 0.), FRotator::ZeroRotator, Params);
		// A different team makes the severer hostile to the coordination participants,
		// which TrySeverCommandLink requires before it will sever anything.
		Severer->TestTeam = 1;
		Severer->InitializeTestCombat(1);

		Link = NewObject<USovCommandLinkComponent>(Commander);
		// Identity must be configured before registration, because an activating
		// BeginPlay refuses reconfiguration on a live instance.
		Link->ConfigureLinkId(TEXT("Test.SourceLoss.Link"));
		Commander->AddInstanceComponent(Link);
		Link->RegisterComponent();
		Link->RegisterLinkedActor(FirstLinked);
		Link->RegisterLinkedActor(SecondLinked);
		if (Link->GetCommandLinkState() != ESovCommandLinkState::Active)
		{
			Link->ActivateCommandLink(Commander);
		}
	}

	~FSourceLossWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}

	ASovCoordinationTestNPC* Spawn(const FVector& Position) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Character = World->SpawnActor<ASovCoordinationTestNPC>(ASovCoordinationTestNPC::StaticClass(),
			Position, FRotator::ZeroRotator, Params);
		Character->InitializeTestCombat();
		return Character;
	}

	static int32 SeveredTags(const ASovCoordinationTestNPC* Character)
	{
		const auto* AbilitySystem = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
		return AbilitySystem ? AbilitySystem->GetTagCount(FSovGameplayTags::Get().State_CommandLink_Severed) : -1;
	}

	static int32 ActiveTags(const ASovCoordinationTestNPC* Character)
	{
		const auto* AbilitySystem = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
		return AbilitySystem ? AbilitySystem->GetTagCount(FSovGameplayTags::Get().State_CommandLink_Active) : -1;
	}

	/** Drives the real death notification the component binds to, not a private setter. */
	static void Kill(ASovCoordinationTestNPC* Character)
	{
		auto* AbilitySystem = Cast<USovCoordinationTestASC>(Character->GetNarrativeAbilitySystemComponent());
		if (!AbilitySystem) { return; }
		AbilitySystem->SeedDead(true);
		AbilitySystem->OnDeathStateChanged.Broadcast(Character, AbilitySystem, true);
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCommanderDeathKeepsSever, "ProjectVelkorran.Campaign.CommandLink.CommanderDeathPreservesSeveredState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCommanderDeathKeepsSever::RunTest(const FString& Parameters)
{
	FSourceLossWorld F;
	if (!TestEqual(TEXT("Link starts active"), static_cast<int32>(F.Link->GetCommandLinkState()),
		static_cast<int32>(ESovCommandLinkState::Active))) { return false; }
	TestEqual(TEXT("A living participant carries the active contribution"), F.ActiveTags(F.FirstLinked), 1);

	FSovCommandLinkSeverResult Result;
	if (!TestEqual(TEXT("A hostile severer newly severs the link"),
		static_cast<int32>(F.Link->TrySeverCommandLink(F.Severer, Result)),
		static_cast<int32>(ESovCommandLinkSeverResolution::NewlySevered))) { return false; }
	const FGuid SeverTransaction = Result.TransactionId;
	TestEqual(TEXT("Severing moves participants to the severed contribution"), F.SeveredTags(F.FirstLinked), 1);
	TestEqual(TEXT("Severing clears the active contribution"), F.ActiveTags(F.FirstLinked), 0);

	F.Kill(F.Commander);

	TestEqual(TEXT("Commander death leaves the link severed"), static_cast<int32>(F.Link->GetCommandLinkState()),
		static_cast<int32>(ESovCommandLinkState::Severed));
	TestEqual(TEXT("A surviving participant keeps the severed state it earned"), F.SeveredTags(F.FirstLinked), 1);
	TestEqual(TEXT("Every surviving participant keeps it"), F.SeveredTags(F.SecondLinked), 1);
	TestEqual(TEXT("No participant silently regains the active contribution"), F.ActiveTags(F.FirstLinked), 0);
	TestEqual(TEXT("The dead commander retires its own contribution"), F.SeveredTags(F.Commander), 0);
	TestEqual(TEXT("The sever receipt survives the commander's death"),
		F.Link->CaptureCommandLinkState().LastSeverTransactionId, SeverTransaction);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCommanderDeathSeverIdempotence, "ProjectVelkorran.Campaign.CommandLink.CommanderDeathPreservesSeverIdempotence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCommanderDeathSeverIdempotence::RunTest(const FString& Parameters)
{
	// Withdrawing severed state would let the same link pay a second Echo reward.
	FSourceLossWorld F;
	FSovCommandLinkSeverResult Result;
	F.Link->TrySeverCommandLink(F.Severer, Result);
	const FGuid FirstTransaction = Result.TransactionId;
	F.Kill(F.Commander);

	FSovCommandLinkSeverResult SecondResult;
	TestEqual(TEXT("A severed link cannot be severed again after its commander dies"),
		static_cast<int32>(F.Link->TrySeverCommandLink(F.Severer, SecondResult)),
		static_cast<int32>(ESovCommandLinkSeverResolution::AlreadySevered));
	TestEqual(TEXT("The repeat attempt reports the original transaction, not a new one"),
		SecondResult.TransactionId, FirstTransaction);
	TestEqual(TEXT("Reactivating a severed link after source loss is refused"),
		static_cast<int32>(F.Link->GetCommandLinkState()), static_cast<int32>(ESovCommandLinkState::Severed));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCommanderDestroyKeepsSever, "ProjectVelkorran.Campaign.CommandLink.CommanderDestructionPreservesSeveredState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCommanderDestroyKeepsSever::RunTest(const FString& Parameters)
{
	// Destruction reaches the same deactivation through a different delegate.
	FSourceLossWorld F;
	FSovCommandLinkSeverResult Result;
	if (!TestEqual(TEXT("A hostile severer newly severs the link"),
		static_cast<int32>(F.Link->TrySeverCommandLink(F.Severer, Result)),
		static_cast<int32>(ESovCommandLinkSeverResolution::NewlySevered))) { return false; }

	F.Commander->Destroy();

	TestEqual(TEXT("Commander destruction leaves the link severed"),
		static_cast<int32>(F.Link->GetCommandLinkState()), static_cast<int32>(ESovCommandLinkState::Severed));
	TestEqual(TEXT("A surviving participant keeps the severed state it earned"), F.SeveredTags(F.FirstLinked), 1);
	TestEqual(TEXT("Every surviving participant keeps it"), F.SeveredTags(F.SecondLinked), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCommanderDeathStillEndsActiveLink, "ProjectVelkorran.Campaign.CommandLink.CommanderDeathStillEndsActiveLink", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCommanderDeathStillEndsActiveLink::RunTest(const FString& Parameters)
{
	// The fix must not disable legitimate deactivation: an active link genuinely ends
	// when its commander dies, because the coordination it grants is no longer sourced.
	FSourceLossWorld F;
	if (!TestEqual(TEXT("Link starts active"), static_cast<int32>(F.Link->GetCommandLinkState()),
		static_cast<int32>(ESovCommandLinkState::Active))) { return false; }
	TestEqual(TEXT("A living participant carries the active contribution"), F.ActiveTags(F.FirstLinked), 1);

	F.Kill(F.Commander);

	TestEqual(TEXT("Commander death deactivates an unsevered link"),
		static_cast<int32>(F.Link->GetCommandLinkState()), static_cast<int32>(ESovCommandLinkState::Inactive));
	TestEqual(TEXT("Deactivation withdraws the active contribution"), F.ActiveTags(F.FirstLinked), 0);
	TestEqual(TEXT("Deactivation never invents a severed contribution"), F.SeveredTags(F.FirstLinked), 0);
	return true;
}
#endif
