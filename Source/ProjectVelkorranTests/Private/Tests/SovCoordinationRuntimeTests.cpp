// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovRuntimeObjectTestFixtures.h"
#include "Tests/SovCoordinationRuntimeTestFixtures.h"
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovCoordinationTestAccess
{
	static void Start(ASovEncounterDirector* Director, ASovPlayerCharacterBase* Player)
	{
		Director->EncounterPlayer = Player; Director->State = ESovEncounterState::Active;
		Director->AttemptId = FGuid::NewGuid(); Director->GetCoordinationComponent()->InitializeCoordination();
	}
	static void Step(USovEncounterCoordinationComponent* Component)
	{ Component->TickComponent(0.1f, LEVELTICK_All, nullptr); }
	static FGuid Warning(USovEncounterCoordinationComponent* Component, FName Id)
	{ const auto* Found = Component->Warnings.Find(Id); return Found ? Found->Id : FGuid(); }
	static void ElapseWarning(USovEncounterCoordinationComponent* Component, FName Id)
	{ if (auto* Found = Component->Warnings.Find(Id)) { Found->AcknowledgedAt -= Component->WarningLeadSeconds; } }
	static int32 Reservations(USovEncounterCoordinationComponent* Component) { return Component->Reservations.Num(); }
	static void Stop(ASovEncounterDirector* Director)
	{ Director->State = ESovEncounterState::Failed; Director->GetCoordinationComponent()->HandleEncounterState(ESovEncounterState::Active, ESovEncounterState::Failed); }
	static void EndRelief(USovEncounterCoordinationComponent* Component) { Component->ReliefUntil = 0.; }
	static void Suspend(ASovEncounterDirector* Director, AActor* Actor) { Director->SuspendActor(Actor); }
	static void ReleaseDirector(ASovEncounterDirector* Director) { Director->ReleaseSuspensions(); }
	static void Stage(USovEncounterCoordinationComponent* Component, FName Id, ASovNPCCharacterBase* Character)
	{ Component->StageParticipant(Id, Character); }
	static void ReleaseStage(USovEncounterCoordinationComponent* Component, FName Id) { Component->ReleaseStagedParticipant(Id); }
	static void ResetStaging(USovEncounterCoordinationComponent* Component) { Component->ResetAttempt(); }
};
namespace
{
	struct FCoordinationWorld
	{
		UWorld* World;
		ASovEncounterDirector* Director;
		ASovExertionRuntimeTestCharacter* Player;
		FCoordinationWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Director = World->SpawnActor<ASovEncounterDirector>(); Director->EncounterId = TEXT("Test.Coordination");
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ASovExertionRuntimeTestCharacter>(ASovExertionRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			Player->InitializeExertion();
		}
		~FCoordinationWorld() { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		ASovCoordinationTestNPC* Add(FName Id, FVector Position, int32 Wave = 0, ESovEncounterDecisionTier Tier = ESovEncounterDecisionTier::Combatant)
		{
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character = World->SpawnActor<ASovCoordinationTestNPC>(ASovCoordinationTestNPC::StaticClass(), Position, FRotator::ZeroRotator, Spawn);
			Character->InitializeTestCombat();
			FSovEncounterParticipant Participant; Participant.ParticipantId = Id; Participant.Character = Character;
			Director->Participants.Add(Participant);
			FSovEncounterCompositionMember Member; Member.ParticipantId = Id; Member.Wave = Wave; Member.Tier = Tier;
			Director->GetCoordinationComponent()->Composition.Add(Member);
			return Character;
		}
	};
	UNarrativeAbilitySystemComponent* ASC(ASovNPCCharacterBase* Character)
	{ return Cast<UNarrativeAbilitySystemComponent>(Character->GetAbilitySystemComponent()); }
	USovBotTestAttackAlpha* Attack(UNarrativeAbilitySystemComponent* Source, FGameplayAbilitySpecHandle& Handle, bool bMelee)
	{
		Handle = Source->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
		auto* Result = Cast<USovBotTestAttackAlpha>(Source->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		Result->BotAttackPressure = bMelee ? ESovBotAttackPressure::Melee : ESovBotAttackPressure::Ranged;
		Result->bBotRequiresLineOfSight = false;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoordinationWaveTest, "ProjectVelkorran.Campaign.Encounter.Coordination.WavesAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCoordinationWaveTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCoordinationWorld Test;
	auto* First = Test.Add(TEXT("First"), FVector(200.f, 0.f, 0.f));
	auto* Second = Test.Add(TEXT("Second"), FVector(300.f, 0.f, 0.f), 1);
	auto* Third = Test.Add(TEXT("Third"), FVector(400.f, 0.f, 0.f), 1, ESovEncounterDecisionTier::Supporting);
	auto* Coordination = Test.Director->GetCoordinationComponent();
	Coordination->MaximumCombatants = 2; Coordination->MaximumSupporting = 1;
	const FGuid OriginalId = Second->GetActorGUID_Implementation(); const FVector OriginalPosition = Second->GetActorLocation();
	ASC(Second)->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 63.f);
	ASC(Third)->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
	FString Error; TestTrue(TEXT("Two authored waves fit their budgets"), Coordination->ValidateComposition(Error));
	Coordination->Composition[2].ParticipantId = TEXT("Second");
	TestFalse(TEXT("Duplicate composition identity rejected"), Coordination->ValidateComposition(Error));
	Coordination->Composition[2].ParticipantId = TEXT("Third");
	FSovCoordinationTestAccess::Start(Test.Director, Test.Player);
	TestFalse(TEXT("Current wave stays presented"), First->IsHidden());
	TestTrue(TEXT("Future waves are staged"), Second->IsHidden() && Third->IsHidden());
	TestFalse(TEXT("Future wave collision disabled"), Second->GetActorEnableCollision());
	TestEqual(TEXT("Staging owns only an added Busy count"), ASC(Third)->GetGameplayTagCount(FNarrativeGameplayTags::Get().State_Busy), 2);
	CastChecked<USovCoordinationTestASC>(ASC(First))->SeedDead(true);
	FSovCoordinationTestAccess::Step(Coordination);
	TestEqual(TEXT("Required defeat promotes next registered wave"), Coordination->GetCurrentWave(), 1);
	TestFalse(TEXT("Promotion restores presentation"), Second->IsHidden());
	TestTrue(TEXT("Promotion restores collision"), Second->GetActorEnableCollision());
	TestEqual(TEXT("Promotion retains exact actor identity"), Second->GetActorGUID_Implementation(), OriginalId);
	TestEqual(TEXT("Promotion retains location"), Second->GetActorLocation(), OriginalPosition);
	TestEqual(TEXT("Promotion does not reset health"), ASC(Second)->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 63.f);
	TestEqual(TEXT("Promotion preserves another owner's Busy count"), ASC(Third)->GetGameplayTagCount(FNarrativeGameplayTags::Get().State_Busy), 1);
	TestTrue(TEXT("Supporting actor may promote within capacity"), Coordination->SetDecisionTier(TEXT("Third"), ESovEncounterDecisionTier::Combatant));
	TestEqual(TEXT("Runtime tier does not mutate authored definition"), Coordination->Composition[2].Tier, ESovEncounterDecisionTier::Supporting);
	FGameplayAbilitySpecHandle Handle; auto* Ability = Attack(ASC(Second), Handle, true);
	TestTrue(TEXT("Test attack activates"), ASC(Second)->TryActivateAbility(Handle, false));
	TestFalse(TEXT("An active attack cannot change decision tier"), Coordination->SetDecisionTier(TEXT("Second"), ESovEncounterDecisionTier::Supporting));
	Ability->FinishTestAttack();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoordinationThreatSuspensionTest, "ProjectVelkorran.Campaign.Encounter.Coordination.ThreatSuspensionOwnersAndPawnReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCoordinationThreatSuspensionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCoordinationWorld Test;
	auto* First = Test.Add(TEXT("First"), FVector(200.f, 0.f, 0.f));
	auto* Future = Test.Add(TEXT("Future"), FVector(300.f, 0.f, 0.f), 1);
	auto* Controller = Test.World->SpawnActor<ANarrativeNPCController>();
	if (!TestNotNull(TEXT("Real Narrative controller"), Controller)) { return false; }
	Controller->Possess(Future); Future->InitializeTestCombat();
	Controller->bRequireThreatMemoryForTargeting = true;
	auto* Coordination = Test.Director->GetCoordinationComponent();
	const auto Observe = [&]()
	{ return Controller->ReportThreatObservation(Test.Player, ENarrativeThreatSource::Damage, Test.Player->GetActorLocation(), 1.f, 1.f, 8.f); };
	TestTrue(TEXT("Managed controller accepts an authoritative observation before staging"), Observe());
	TestTrue(TEXT("Observation permits direct target query"), Controller->CanDirectlyTargetThreat(Test.Player));
	FSovCoordinationTestAccess::Start(Test.Director, Test.Player);
	TestTrue(TEXT("Future-wave staging owns a threat suspension and hides the actor"), Controller->IsThreatMemorySuspended() && Future->IsHidden());
	TestFalse(TEXT("Hidden staged wave cannot acquire new threat observations"), Observe());
	TestFalse(TEXT("Staged wave cannot use its old target query"), Controller->CanDirectlyTargetThreat(Test.Player));
	FSovCoordinationTestAccess::Suspend(Test.Director, Future);
	FSovCoordinationTestAccess::ReleaseDirector(Test.Director);
	TestTrue(TEXT("Director release preserves the independent future-wave suspension"), Controller->IsThreatMemorySuspended());
	TestTrue(TEXT("Director release does not reveal the staged participant"), Future->IsHidden());
	TestFalse(TEXT("Wave owner continues to block observation after director release"), Observe());
	CastChecked<USovCoordinationTestASC>(ASC(First))->SeedDead(true);
	FSovCoordinationTestAccess::Step(Coordination);
	TestEqual(TEXT("Actual wave promotion releases staged participant"), Coordination->GetCurrentWave(), 1);
	TestFalse(TEXT("Last suspension owner is released on promotion"), Controller->IsThreatMemorySuspended());
	TestFalse(TEXT("Promotion restores actor presentation"), Future->IsHidden());
	TestFalse(TEXT("Promotion does not resurrect stale pre-checkpoint observations"), Controller->CanDirectlyTargetThreat(Test.Player));
	TestTrue(TEXT("Promoted controller accepts a fresh observation"), Observe());
	TestTrue(TEXT("Fresh observation restores direct target query"), Controller->CanDirectlyTargetThreat(Test.Player));

	FSovCoordinationTestAccess::Stage(Coordination, TEXT("Future"), Future);
	FSovCoordinationTestAccess::Suspend(Test.Director, Future);
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Replacement = Test.World->SpawnActor<ASovCoordinationTestNPC>(ASovCoordinationTestNPC::StaticClass(), FVector(450.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Replacement controller pawn"), Replacement)) { return false; }
	Controller->Possess(Replacement); Replacement->InitializeTestCombat();
	TestFalse(TEXT("Possession replacement clears old pawn suspension ownership"), Controller->IsThreatMemorySuspended());
	TStrongObjectPtr<UObject> ExternalOwner(NewObject<USovRuntimeTestIdentity>());
	Controller->SetThreatMemorySuspended(ExternalOwner.Get(), true);
	FSovCoordinationTestAccess::ReleaseDirector(Test.Director);
	FSovCoordinationTestAccess::ReleaseStage(Coordination, TEXT("Future"));
	TestTrue(TEXT("Old pawn cleanup cannot remove a replacement pawn's unrelated lease"), Controller->IsThreatMemorySuspended());
	Controller->SetThreatMemorySuspended(ExternalOwner.Get(), false);
	TestFalse(TEXT("Replacement lease owner can release its own suspension"), Controller->IsThreatMemorySuspended());
	FSovCoordinationTestAccess::Stage(Coordination, TEXT("Replacement"), Replacement);
	FSovCoordinationTestAccess::Suspend(Test.Director, Replacement);
	FSovCoordinationTestAccess::ResetStaging(Coordination); // The same cleanup path used by component EndPlay.
	TestTrue(TEXT("Coordination teardown preserves director suspension"), Controller->IsThreatMemorySuspended());
	FSovCoordinationTestAccess::ReleaseDirector(Test.Director);
	TestFalse(TEXT("Both teardown paths leave no threat-suspension lease"), Controller->IsThreatMemorySuspended());
	TestTrue(TEXT("Replacement target query can resume after fresh sensing"), Observe() && Controller->CanDirectlyTargetThreat(Test.Player));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoordinationAdmissionTest, "ProjectVelkorran.Campaign.Encounter.Coordination.AttackLeasesWarningsAndRelief",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCoordinationAdmissionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCoordinationWorld Test;
	auto* First = Test.Add(TEXT("First"), FVector(200.f, 0.f, 0.f));
	auto* Second = Test.Add(TEXT("Second"), FVector(250.f, 0.f, 0.f));
	auto* Ranged = Test.Add(TEXT("Ranged"), FVector(-500.f, 0.f, 0.f));
	auto* Coordination = Test.Director->GetCoordinationComponent(); Coordination->MeleeAttackerSlots = 1;
	FGameplayAbilitySpecHandle FirstHandle, SecondHandle, RangedHandle;
	auto* FirstAbility = Attack(ASC(First), FirstHandle, true);
	Attack(ASC(Second), SecondHandle, true); Attack(ASC(Ranged), RangedHandle, false);
	FSovCoordinationTestAccess::Start(Test.Director, Test.Player);
	TestTrue(TEXT("Existing selector acquires coordination-only attack lease"), ASC(First)->TryActivateBotAttack(Test.Player, FirstHandle));
	TestTrue(TEXT("Coordination-only execution does not require a separate token lease"), ASC(First)->IsBotAttackExecutionValid(Test.Player, FirstHandle));
	TestFalse(TEXT("Second melee attack respects shared slot"), ASC(Second)->TryActivateBotAttack(Test.Player, SecondHandle));
	FirstAbility->FinishTestAttack();
	TestEqual(TEXT("GAS end releases its coordination lease"), FSovCoordinationTestAccess::Reservations(Coordination), 0);
	TestTrue(TEXT("Released melee slot becomes available"), ASC(Second)->TryActivateBotAttack(Test.Player, SecondHandle));
	TestFalse(TEXT("Unacknowledged offscreen shot remains blocked"), ASC(Ranged)->TryActivateBotAttack(Test.Player, RangedHandle));
	FSovCoordinationTestAccess::Step(Coordination);
	const FGuid Warning = FSovCoordinationTestAccess::Warning(Coordination, TEXT("Ranged"));
	TestTrue(TEXT("Ranged source issues an attempt-scoped cue request"), Warning.IsValid());
	TestTrue(TEXT("Presented cue may acknowledge once"), Coordination->AcknowledgeOffscreenWarning(Warning));
	TestFalse(TEXT("Acknowledgement cannot replay"), Coordination->AcknowledgeOffscreenWarning(Warning));
	TestFalse(TEXT("Cue lead time is mandatory"), ASC(Ranged)->TryActivateBotAttack(Test.Player, RangedHandle));
	FSovCoordinationTestAccess::ElapseWarning(Coordination, TEXT("Ranged"));
	TestTrue(TEXT("Presented cue plus elapsed lead admits shot"), ASC(Ranged)->TryActivateBotAttack(Test.Player, RangedHandle));
	TestFalse(TEXT("Shot consumes its warning receipt"), Coordination->AcknowledgeOffscreenWarning(Warning));
	auto* PlayerASC = Test.Player->GetNarrativeAbilitySystemComponent();
	PlayerASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 20.f);
	const float Before = PlayerASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute());
	FSovCoordinationTestAccess::Step(Coordination);
	TestTrue(TEXT("Critical resources open a finite relief period"), Coordination->IsPressureReliefActive());
	TestEqual(TEXT("Relief grants no health"), PlayerASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 20.f);
	TestEqual(TEXT("Relief grants no stamina"), PlayerASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), Before);
	FSovCoordinationTestAccess::EndRelief(Coordination); FSovCoordinationTestAccess::Step(Coordination);
	TestFalse(TEXT("Cooldown prevents immediate relief renewal"), Coordination->IsPressureReliefActive());
	FSovCoordinationTestAccess::Stop(Test.Director);
	TestFalse(TEXT("Attempt failure invalidates an active reservation"), ASC(Second)->IsBotAttackExecutionValid(Test.Player, SecondHandle));
	TestFalse(TEXT("Stale cue cannot authorize another attempt"), Coordination->AcknowledgeOffscreenWarning(Warning));
	return true;
}
#endif
