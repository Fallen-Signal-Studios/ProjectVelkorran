// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovBotAttackTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FBotTestWorld
	{
		UWorld* World = nullptr;
		FBotTestWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			}
		}
		~FBotTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovBotTestCharacter* Character(const FVector Location, const int32 Team)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn);
			if (Actor) { Actor->InitializeTestCombat(Team); }
			return Actor;
		}
		AActor* Wall()
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(25.f, 200.f, 200.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Actor->SetActorLocation(FVector(250.f, 0.f, 0.f));
			return Actor;
		}
	};
	USovBotTestAttackAlpha* Grant(UNarrativeAbilitySystemComponent* ASC, const TSubclassOf<UGameplayAbility> Class, FGameplayAbilitySpecHandle& Out)
	{
		Out = ASC->GiveAbility(FGameplayAbilitySpec(Class, 1));
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Out);
		return Spec ? Cast<USovBotTestAttackAlpha>(Spec->GetPrimaryInstance()) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBotRepertoireTest, "ProjectVelkorran.Campaign.BotSelection.RepertoireAndExactSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovBotRepertoireTest::RunTest(const FString& Parameters)
{
	FBotTestWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.f, 0.f, 0.f), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	FGameplayAbilitySpecHandle AlphaHandle, BetaHandle, PeerHandle;
	auto* Alpha = Grant(ASC, USovBotTestAttackAlpha::StaticClass(), AlphaHandle);
	auto* Beta = Grant(ASC, USovBotTestAttackBeta::StaticClass(), BetaHandle);
	auto* Peer = Grant(ASC, USovBotTestAttackPeer::StaticClass(), PeerHandle);
	if (!Alpha || !Beta || !Peer) { AddError(TEXT("Ability instances failed")); return false; }
	TestEqual(TEXT("Whole repertoire includes all three grants"), ASC->GetBotAttackCandidates(Target, FGameplayTag()).Num(), 3);
	TestEqual(TEXT("Legacy primary filter contains only primary peers"),
		ASC->GetBotAttackCandidates(Target, FNarrativeGameplayTags::Get().Narrative_Input_Attack).Num(), 2);
	FNarrativeBotAttackCandidate Choice, Again;
	TestTrue(TEXT("Ready choice exists"), ASC->SelectBotAttack(Target, FGameplayTag(), Choice));
	TestTrue(TEXT("Repeated read-only choice is stable"), ASC->SelectBotAttack(Target, FGameplayTag(), Again) && Choice.Handle == Again.Handle);
	TestTrue(TEXT("Exact primary activation succeeds"), ASC->TryActivateBotAttack(Target, AlphaHandle));
	TestEqual(TEXT("Chosen ability fired once"), Alpha->ActivationCount, 1);
	TestEqual(TEXT("Same-input peer did not fire"), Peer->ActivationCount, 0);
	TestEqual(TEXT("Different-input special did not fire"), Beta->ActivationCount, 0);
	TestEqual(TEXT("Only chosen spec received release"), Alpha->ReleaseCount, 1);
	TestFalse(TEXT("Busy active attack prevents a second activation"), ASC->TryActivateBestBotAttack(Target, FGameplayTag(), Choice));
	Alpha->FinishTestAttack();
	TestFalse(TEXT("Chosen ability respects its cadence"), ASC->TryActivateBotAttack(Target, AlphaHandle));
	TestTrue(TEXT("Special fallback remains playable on primary cooldown"), ASC->TryActivateBestBotAttack(Target, FGameplayTag(), Choice));
	TestTrue(TEXT("Unfiltered chooser reaches Ability2"), Choice.Handle == BetaHandle);
	TestEqual(TEXT("Special fired once"), Beta->ActivationCount, 1);
	Beta->FinishTestAttack();
	TestTrue(TEXT("Remaining ready primary peer is selected"), ASC->TryActivateBestBotAttack(Target, FGameplayTag(), Choice));
	TestTrue(TEXT("Exact peer selected"), Choice.Handle == PeerHandle);
	Peer->FinishTestAttack();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBotAvailabilityTest, "ProjectVelkorran.Campaign.BotSelection.AvailabilityAndMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovBotAvailabilityTest::RunTest(const FString& Parameters)
{
	FBotTestWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.f, 0.f, 0.f), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	FGameplayAbilitySpecHandle AlphaHandle, BetaHandle;
	auto* Alpha = Grant(ASC, USovBotTestAttackAlpha::StaticClass(), AlphaHandle);
	auto* Beta = Grant(ASC, USovBotTestAttackBeta::StaticClass(), BetaHandle);
	if (!Alpha || !Beta) { AddError(TEXT("Ability instances failed")); return false; }
	Alpha->bRejectActivation = true;
	Beta->bRejectActivation = true;
	FNarrativeBotAttackCandidate Choice;
	TestFalse(TEXT("Unavailable payloads do not activate"), ASC->SelectBotAttack(Target, FGameplayTag(), Choice));
	TestEqual(TEXT("Legacy range survives native cooldown/config rejection"), ASC->GetBotAttackRange(Alpha->InputTag), 1000.f);
	TestTrue(TEXT("Whole repertoire still supplies useful movement distance"), ASC->GetBotCombatMovementRange(Target, FGameplayTag()) >= 1000.f);
	Alpha->bRejectActivation = false;
	Beta->bRejectActivation = false;
	Beta->BotSelectionPriority = 10.f;
	TestTrue(TEXT("Authored priority selects the special"), ASC->SelectBotAttack(Target, FGameplayTag(), Choice) && Choice.Handle == BetaHandle);
	Target->SetActorLocation(FVector(200.f, 0.f, 0.f));
	TestTrue(TEXT("Special minimum range falls back to basic attack"), ASC->SelectBotAttack(Target, FGameplayTag(), Choice) && Choice.Handle == AlphaHandle);
	Target->SetActorLocation(FVector(500.f, 0.f, 0.f));
	AActor* Wall = F.Wall();
	TestFalse(TEXT("Opaque wall blocks all attacks"), ASC->SelectBotAttack(Target, FGameplayTag(), Choice));
	TestTrue(TEXT("Occlusion keeps useful movement range"), ASC->GetBotCombatMovementRange(Target, FGameplayTag()) >= 1000.f);
	Wall->Destroy();
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	TestFalse(TEXT("Cinematic state blocks activation"), ASC->TryActivateBotAttack(Target, AlphaHandle));
	ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	Target->TestTeam = Source->TestTeam;
	TestFalse(TEXT("Friendly target rejected"), ASC->TryActivateBotAttack(Target, AlphaHandle));
	Target->TestTeam = 1;
	ASC->ClearAbility(AlphaHandle);
	TestFalse(TEXT("Revoked selected handle cannot execute"), ASC->TryActivateBotAttack(Target, AlphaHandle));
	Target->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("Zero-health target rejected before death convergence"), ASC->TryActivateBotAttack(Target, BetaHandle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBotTokenTest, "ProjectVelkorran.Campaign.BotSelection.TokenOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovBotTokenTest::RunTest(const FString& Parameters)
{
	FBotTestWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.f, 0.f, 0.f), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.World->SpawnActor<ANarrativeNPCController>();
	if (!TestNotNull(TEXT("Narrative controller"), Controller)) { return false; }
	Controller->Possess(Source);
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	auto* TargetASC = Cast<USovBotTestASC>(Target->GetNarrativeAbilitySystemComponent());
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = Grant(ASC, USovBotTestAttackAlpha::StaticClass(), Handle);
	if (!Ability || !TargetASC) { AddError(TEXT("Token fixtures failed")); return false; }
	Ability->bBotRequiresAttackToken = true;
	TargetASC->TestTokenBudget = 0;
	TestFalse(TEXT("Empty attacker budget blocks exact activation"), ASC->TryActivateBotAttack(Target, Handle));
	TestEqual(TEXT("Rejected token does not execute payload"), Ability->ActivationCount, 0);
	TargetASC->TestTokenBudget = 1;
	TestTrue(TEXT("Available token starts selected attack"), ASC->TryActivateBotAttack(Target, Handle));
	TestEqual(TEXT("Exact attack lease retains its selected target"), ASC->GetBotAttackTarget(Handle), static_cast<AActor*>(Target));
	TestTrue(TEXT("Target owns reciprocal reservation"), TargetASC->HasAttackTokenFor(Controller));
	TestTrue(TEXT("Own Busy state does not cancel active attack"), ASC->IsBotAttackExecutionValid(Target, Handle));
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
	TestFalse(TEXT("Device shutdown invalidates a running generic attack"), ASC->IsBotAttackExecutionValid(Target, Handle));
	ASC->CancelAbilityHandle(Handle);
	TestNull(TEXT("Retired attack cannot expose a stale target"), ASC->GetBotAttackTarget(Handle));
	ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
	TestFalse(TEXT("Ending exact attack releases its new token"), TargetASC->HasAttackTokenFor(Controller));
	// Borrow an existing Behavior Tree-owned token without returning it afterward.
	uint64 Lease = 0;
	bool NewlyAcquired = false;
	TestTrue(TEXT("Existing behavior can acquire token"), Controller->TryAcquireAttackTokenFor(TargetASC, Lease, NewlyAcquired));
	TestTrue(TEXT("Convert reservation to existing BT ownership"), Controller->ReleaseAttackTokenLease(Lease, false));
	FGameplayAbilitySpecHandle PeerHandle;
	auto* Peer = Grant(ASC, USovBotTestAttackPeer::StaticClass(), PeerHandle);
	if (!Peer) { AddError(TEXT("Peer fixture failed")); return false; }
	Peer->bBotRequiresAttackToken = true;
	TestTrue(TEXT("Selector borrows existing ownership"), ASC->TryActivateBotAttack(Target, PeerHandle));
	Peer->FinishTestAttack();
	TestTrue(TEXT("Selector does not return another behavior's token"), TargetASC->HasAttackTokenFor(Controller));
	TestTrue(TEXT("Behavior reacquires its reservation"), Controller->TryAcquireAttackTokenFor(TargetASC, Lease, NewlyAcquired));
	Controller->ReleaseAttackTokenLease(Lease, true);
	TestFalse(TEXT("Behavior can return its own token"), TargetASC->HasAttackTokenFor(Controller));
	return true;
}
#endif
