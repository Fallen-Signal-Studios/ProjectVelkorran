// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovRuntimeObjectTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Tests/SovBotAttackTestFixtures.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FThreatWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		FThreatWorld()
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &WorldInitialization);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			}
		}
		~FThreatWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovBotTestCharacter* Character(const FVector Position, const int32 Team)
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character = World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), Position, FRotator::ZeroRotator, Parameters);
			if (Character) { Character->InitializeTestCombat(Team); }
			return Character;
		}
		ANarrativeNPCController* Controller(ASovBotTestCharacter* Character)
		{
			auto* Controller = World->SpawnActor<ANarrativeNPCController>();
			if (Controller) { Controller->Possess(Character); Controller->bShareThreatsWithFaction = false; }
			return Controller;
		}
		void Advance(const float Seconds)
		{
			// World ticks clamp large deltas; expiry requires actual elapsed world time.
			const double Until = World->GetTimeSeconds() + Seconds;
			uint64 FrameNumber = GFrameCounter;
			while (World->GetTimeSeconds() + UE_DOUBLE_SMALL_NUMBER < Until)
			{
				TGuardValue<uint64> Frame(GFrameCounter, ++FrameNumber);
				World->Tick(LEVELTICK_TimeOnly, FMath::Min(0.05, Until - World->GetTimeSeconds()));
			}
		}
	};
	UBlackboardComponent* BlackboardFor(ANarrativeNPCController* Controller)
	{
		const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
		auto* Data = NewObject<UBlackboardData>(Controller);
		FBlackboardEntry TargetEntry;
		TargetEntry.EntryName = Settings->BBKey_AttackTarget;
		TargetEntry.KeyType = NewObject<UBlackboardKeyType_Object>(Data);
		Data->Keys.Add(TargetEntry);
		FBlackboardEntry LocationEntry;
		LocationEntry.EntryName = Settings->BBKey_TargetLocation;
		LocationEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(Data);
		Data->Keys.Add(LocationEntry);
		UBlackboardComponent* Blackboard = nullptr;
		return Controller->UseBlackboard(Data, Blackboard) ? Blackboard : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatSelectionRuntimeTest,
	"ProjectVelkorran.Campaign.Threat.SelectionCloakAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatSelectionRuntimeTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.Controller(Source);
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	auto* Blackboard = BlackboardFor(Controller);
	if (!TestNotNull(TEXT("Blackboard"), Blackboard)) { return false; }
	const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
	FNarrativeBotAttackCandidate Candidate;
	TestTrue(TEXT("Legacy nonperception selection remains available"), ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
	TestTrue(TEXT("Hearing accepted without inventing sight"), Controller->ReportThreatObservation(
		Target, ENarrativeThreatSource::Hearing, Target->GetActorLocation(), 1.f, 1.f, 2.f));
	TestFalse(TEXT("Hearing cannot authorize direct fire"), ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
	TestFalse(TEXT("Unselected archetype cannot use network sense"), Controller->ReportThreatObservation(
		Target, ENarrativeThreatSource::NetworkSensor, Target->GetActorLocation()));
	TestTrue(TEXT("Direct observation admits native selection"), Controller->ReportThreatObservation(
		Target, ENarrativeThreatSource::Sight, Target->GetActorLocation(), 1.f, 1.f, 2.f));
	TestTrue(TEXT("Native GAS ability actually activates"), ASC->TryActivateBotAttack(Target, Handle));
	TestTrue(TEXT("Owned payload valid while observed"), ASC->IsBotAttackExecutionValid(Target, Handle));
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	Controller->SetFocus(Target);
	const FVector LastSeen = Target->GetActorLocation();
	Target->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
	Target->SetActorLocation(FVector(800, 100, 0));
	TestFalse(TEXT("Cloak invalidates the running payload immediately"), ASC->IsBotAttackExecutionValid(Target, Handle));
	Controller->RefreshThreatMemory();
	TestNull(TEXT("Cloak clears live actor tracking from attack key"), Blackboard->GetValueAsObject(Settings->BBKey_AttackTarget));
	TestNull(TEXT("Cloak clears actor focus"), Controller->GetFocusActor());
	TestTrue(TEXT("Investigation is the recorded position, not hidden actor position"),
		Blackboard->GetValueAsVector(Settings->BBKey_TargetLocation).Equals(LastSeen));
	FNarrativeThreatMemory Memory;
	TestTrue(TEXT("Memory remains available while concealed"), Controller->GetBestThreatMemory(Target, Memory));
	TestTrue(TEXT("Hidden actor movement did not update memory"), Memory.LastKnownPosition.Equals(LastSeen));
	F.Advance(2.01f);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Finite threat expired without requiring engine forgotten-actor option"), Controller->GetBestThreatMemory(Target, Memory));
	TestFalse(TEXT("Investigation blackboard location clears at expiry"), Blackboard->IsVectorValueSet(Blackboard->GetKeyID(Settings->BBKey_TargetLocation)));
	Target->GetNarrativeAbilitySystemComponent()->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
	TestFalse(TEXT("Uncloaking does not resurrect expired memory"), Controller->CanDirectlyTargetThreat(Target));
	ASC->CancelAbilityHandle(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatPerceptionRuntimeTest,
	"ProjectVelkorran.Campaign.Threat.PerceptionLossAndForgetting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatPerceptionRuntimeTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.Controller(Source);
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	auto* Perception = NewObject<UAIPerceptionComponent>(Controller);
	Controller->AddInstanceComponent(Perception);
	auto* Sight = NewObject<UAISenseConfig_Sight>(Perception);
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Perception->ConfigureSense(*Sight);
	Controller->SetPerceptionComponent(*Perception);
	Perception->RegisterComponent();
	Perception->Activate(true);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Sensor-managed controller cannot target unobserved actor"), Controller->CanDirectlyTargetThreat(Target));
	FAIStimulus Seen(*GetDefault<UAISense_Sight>(), 2.f, Target->GetActorLocation(), Source->GetActorLocation());
	Perception->RegisterStimulus(Target, Seen);
	Perception->ProcessStimuli();
	TestTrue(TEXT("Actual perception delegate creates direct memory"), Controller->CanDirectlyTargetThreat(Target));
	Controller->RefreshThreatMemory();
	FNarrativeThreatMemory Memory;
	TestTrue(TEXT("Current sight has memory"), Controller->GetBestThreatMemory(Target, Memory));
	TestEqual(TEXT("Refresh preserves authored stimulus strength"), Memory.Strength, 2.f);
	Perception->Deactivate();
	Target->SetActorLocation(FVector(700, 0, 0));
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Disabled sensor cannot authorize stale successful sight"), Controller->CanDirectlyTargetThreat(Target));
	TestTrue(TEXT("Disabled sensor preserves finite last-known memory"), Controller->GetBestThreatMemory(Target, Memory));
	TestTrue(TEXT("Disabled sensor never samples the target's new position"), Memory.LastKnownPosition.Equals(FVector(500, 0, 0)));
	Perception->Activate(true);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Activation alone cannot resurrect retained successful sight"), Controller->CanDirectlyTargetThreat(Target));
	TestTrue(TEXT("Activation retains finite investigation memory"), Controller->GetBestThreatMemory(Target, Memory));
	TestTrue(TEXT("No new stimulus means no updated target position after activation"), Memory.LastKnownPosition.Equals(FVector(500, 0, 0)));
	FAIStimulus Reacquired(*GetDefault<UAISense_Sight>(), 2.f, Target->GetActorLocation(), Source->GetActorLocation());
	Perception->RegisterStimulus(Target, Reacquired);
	Perception->ProcessStimuli();
	Controller->RefreshThreatMemory();
	TestTrue(TEXT("A fresh post-activation sensing event restores direct targeting"), Controller->CanDirectlyTargetThreat(Target));
	Controller->GetBestThreatMemory(Target, Memory);
	TestTrue(TEXT("Fresh sight records the new position"), Memory.LastKnownPosition.Equals(FVector(700, 0, 0)));
	Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	Controller->RefreshThreatMemory();
	Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("A sight-enabled edge also requires a new sensing event"), Controller->CanDirectlyTargetThreat(Target));
	Seen.MarkNoLongerSensed();
	Perception->RegisterStimulus(Target, Seen);
	Perception->ProcessStimuli();
	TestFalse(TEXT("Lost-sight delegate immediately removes direct authorization"), Controller->CanDirectlyTargetThreat(Target));
	TestTrue(TEXT("Lost sight retains investigation memory"), Controller->GetBestThreatMemory(Target, Memory));
	Controller->ForgetThreat(Target);
	TArray<AActor*> Known;
	Perception->GetKnownPerceivedActors(nullptr, Known);
	TestFalse(TEXT("Explicit forgetting clears actual engine perception data"), Known.Contains(Target));
	TestFalse(TEXT("Explicit forgetting clears controller memory"), Controller->GetBestThreatMemory(Target, Memory));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatEventDrivenPerceptionTest,
	"ProjectVelkorran.Campaign.Threat.AuthoredEventDrivenSensorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatEventDrivenPerceptionTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	if (!Source || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.Controller(Source);
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	auto* Blackboard = BlackboardFor(Controller);
	if (!TestNotNull(TEXT("Blackboard"), Blackboard)) { return false; }
	const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	ASC->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
	const auto NewSensor = [Controller]()
	{
		auto* Sensor = NewObject<UAIPerceptionComponent>(Controller);
		Controller->AddInstanceComponent(Sensor);
		auto* Sight = NewObject<UAISenseConfig_Sight>(Sensor);
		Sight->DetectionByAffiliation.bDetectEnemies = true;
		Sight->DetectionByAffiliation.bDetectFriendlies = true;
		Sight->DetectionByAffiliation.bDetectNeutrals = true;
		Sensor->ConfigureSense(*Sight);
		Controller->SetPerceptionComponent(*Sensor);
		Sensor->RegisterComponent();
		return Sensor;
	};
	const auto Observe = [Source, Target](UAIPerceptionComponent* Sensor)
	{
		FAIStimulus Seen(*GetDefault<UAISense_Sight>(), 1.f, Target->GetActorLocation(), Source->GetActorLocation());
		Sensor->RegisterStimulus(Target, Seen);
		Sensor->ProcessStimuli();
	};
	auto* Perception = NewSensor();
	TestTrue(TEXT("Authored default sensor is registered with sight enabled"),
		Perception->IsRegistered() && Perception->IsSenseEnabled(UAISense_Sight::StaticClass()));
	TestFalse(TEXT("Stock sensor does not auto-activate"), Perception->bAutoActivate);
	TestFalse(TEXT("Stock sensing does not require a component tick"), Perception->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("No fixture activation masks the authored inactive default"), Perception->IsActive());
	Controller->RefreshThreatMemory();
	FNarrativeBotAttackCandidate Candidate;
	TestFalse(TEXT("Enabled sight still requires an actual observation before attack selection"),
		ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
	Observe(Perception);
	TestTrue(TEXT("An inactive event-driven sensor admits a real sight event"), Controller->CanDirectlyTargetThreat(Target));
	TestTrue(TEXT("Authored-default sight admits native attack selection"), ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	Controller->SetFocus(Target);
	Controller->RefreshThreatMemory();
	TestTrue(TEXT("Refresh preserves the observed behavior-tree target"), Blackboard->GetValueAsObject(Settings->BBKey_AttackTarget) == Target);
	TestTrue(TEXT("Refresh preserves observed actor focus"), Controller->GetFocusActor() == Target);

	Perception->Activate(true);
	TestFalse(TEXT("Activation does not revive the previous successful stimulus"), Controller->CanDirectlyTargetThreat(Target));
	Observe(Perception);
	TestTrue(TEXT("Fresh sight after activation restores targeting"), Controller->CanDirectlyTargetThreat(Target));
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	Controller->SetFocus(Target);
	Perception->Deactivate();
	TestFalse(TEXT("An explicit deactivation vetoes targeting immediately"), Controller->CanDirectlyTargetThreat(Target));
	TestNull(TEXT("Explicit deactivation clears the behavior-tree target"), Blackboard->GetValueAsObject(Settings->BBKey_AttackTarget));
	TestNull(TEXT("Explicit deactivation clears actor focus"), Controller->GetFocusActor());
	Observe(Perception);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Event-driven stimuli cannot bypass an observed deactivation"), Controller->CanDirectlyTargetThreat(Target));
	Perception->Activate(true);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Reactivation discards even stimuli delivered during suspension"), Controller->CanDirectlyTargetThreat(Target));
	Observe(Perception);
	TestTrue(TEXT("Fresh post-resume sight restores direct authorization"), Controller->CanDirectlyTargetThreat(Target));

	auto* Replacement = NewSensor();
	TestFalse(TEXT("Replacement identity cannot use old sight before rebinding"), Controller->CanDirectlyTargetThreat(Target));
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("A bound replacement without its own stimulus cannot inherit old sight"), Controller->CanDirectlyTargetThreat(Target));
	Perception->Deactivate();
	Observe(Replacement);
	TestTrue(TEXT("Retired sensor deactivation cannot suspend the newly bound default listener"), Controller->CanDirectlyTargetThreat(Target));
	Replacement->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	TestFalse(TEXT("Disabling sight immediately suspends initial event-driven sensing"), Controller->CanDirectlyTargetThreat(Target));
	Controller->RefreshThreatMemory();
	Replacement->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	Controller->RefreshThreatMemory();
	TestFalse(TEXT("Re-enabling sight requires fresh perception"), Controller->CanDirectlyTargetThreat(Target));
	Observe(Replacement);
	TestTrue(TEXT("Fresh sight restores the enabled replacement"), Controller->CanDirectlyTargetThreat(Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatSharingRuntimeTest,
	"ProjectVelkorran.Campaign.Threat.FactionSharingAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatSharingRuntimeTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Ally = F.Character(FVector(200, 200, 0), 0);
	auto* Relay = F.Character(FVector(300, 200, 0), 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	if (!Source || !Ally || !Relay || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Sender = F.Controller(Source);
	auto* Receiver = F.Controller(Ally);
	auto* Third = F.Controller(Relay);
	if (!Sender || !Receiver || !Third) { AddError(TEXT("Controller fixtures failed")); return false; }
	Sender->bShareThreatsWithFaction = Receiver->bShareThreatsWithFaction = Third->bShareThreatsWithFaction = true;
	Sender->ReportThreatObservation(Target, ENarrativeThreatSource::Sight, Target->GetActorLocation(), 1.f, 1.f, 1.f);
	TestFalse(TEXT("Friendly attitude without a common faction cannot broadcast"), Sender->ShareThreatWith(Receiver, Target));
	const FGameplayTagContainer Faction(FNarrativeGameplayTags::Get().Narrative_Factions_Heroes);
	UArsenalStatics::AddFactionsToActor(Source, Faction);
	UArsenalStatics::AddFactionsToActor(Ally, Faction);
	UArsenalStatics::AddFactionsToActor(Relay, Faction);
	TestFalse(TEXT("Unmanaged legacy recipient refuses mandatory-memory alerts"), Sender->ShareThreatWith(Receiver, Target));
	TestTrue(TEXT("Refused alert preserves legacy direct targeting"), Receiver->CanDirectlyTargetThreat(Target));
	Receiver->bRequireThreatMemoryForTargeting = Third->bRequireThreatMemoryForTargeting = true;
	TestTrue(TEXT("Same-faction nearby allies share an observation"), Sender->ShareThreatWith(Receiver, Target));
	FNarrativeThreatMemory Original, Shared;
	Sender->GetBestThreatMemory(Target, Original);
	TestTrue(TEXT("Recipient records source and last-known position"), Receiver->GetBestThreatMemory(Target, Shared));
	TestTrue(TEXT("Shared source is distinguishable"), Shared.Source == ENarrativeThreatSource::AllyAlert);
	TestTrue(TEXT("Sharing preserves sender identity and faction"), Shared.SharedBy.Get() == Source && Shared.SharedFactions.HasAllExact(Faction));
	TestTrue(TEXT("Sharing cannot extend originating observation"), Shared.ExpiresAt <= Original.ExpiresAt);
	TestFalse(TEXT("An ally alert is investigation, not direct target confidence"), Receiver->CanDirectlyTargetThreat(Target));
	TestFalse(TEXT("Shared alerts cannot bounce through a second relay"), Receiver->ShareThreatWith(Third, Target));
	Ally->TestTeam = 1;
	TestFalse(TEXT("Faction membership does not override hostile attitude"), Sender->ShareThreatWith(Receiver, Target));
	Ally->TestTeam = 0;
	Ally->SetActorLocation(FVector(50000, 0, 0));
	TestFalse(TEXT("No unbounded battlefield telepathy"), Sender->ShareThreatWith(Receiver, Target));
	F.Advance(1.01f);
	TestFalse(TEXT("Expired source cannot be renewed through sharing"), Sender->ShareThreatWith(Third, Target));
	Receiver->RefreshThreatMemory();
	TestFalse(TEXT("Recipient alert expires independently"), Receiver->GetBestThreatMemory(Target, Shared));
	TestTrue(TEXT("Fresh observation can be acquired"), Sender->ReportThreatObservation(Target, ENarrativeThreatSource::Damage, Target->GetActorLocation()));
	Sender->UnPossess();
	TestEqual(TEXT("Unpossess clears transient memory across pawn identity"), Sender->GetThreatDebugSnapshot().Num(), 0);
	Receiver->ReportThreatObservation(Target, ENarrativeThreatSource::Damage, Target->GetActorLocation());
	Target->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("Dead target is rejected before death-tag convergence"), Receiver->CanDirectlyTargetThreat(Target));
	Receiver->RefreshThreatMemory();
	TestEqual(TEXT("Dead target leaves no memory entries"), Receiver->GetThreatDebugSnapshot().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatOwnershipRuntimeTest,
	"ProjectVelkorran.Campaign.Threat.SuspensionAndCallbackOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatOwnershipRuntimeTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Replacement = F.Character(FVector(0, 200, 0), 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	auto* OtherTarget = F.Character(FVector(500, 200, 0), 1);
	if (!Source || !Replacement || !Target || !OtherTarget) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.Controller(Source);
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	Controller->bRequireThreatMemoryForTargeting = true;
	UObject* FirstOwner = NewObject<USovRuntimeTestIdentity>(Controller);
	UObject* SecondOwner = NewObject<USovRuntimeTestIdentity>(Controller);
	Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Sight, Target->GetActorLocation());
	Controller->SetThreatMemorySuspended(FirstOwner, true);
	Controller->SetThreatMemorySuspended(SecondOwner, true);
	TestFalse(TEXT("Suspension rejects live observations"), Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Sight, Target->GetActorLocation()));
	TestFalse(TEXT("Suspension rejects direct targeting"), Controller->CanDirectlyTargetThreat(Target));
	Controller->SetThreatMemorySuspended(FirstOwner, false);
	TestTrue(TEXT("One owner cannot release another suspension"), Controller->IsThreatMemorySuspended());
	Controller->SetThreatMemorySuspended(SecondOwner, false);
	TestFalse(TEXT("Final owner releases suspension"), Controller->IsThreatMemorySuspended());
	TestFalse(TEXT("Resume requires fresh observation"), Controller->CanDirectlyTargetThreat(Target));
	Source->SetActorHiddenInGame(true);
	TestFalse(TEXT("Staged hidden observer cannot acquire"), Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Sight, Target->GetActorLocation()));
	Source->SetActorHiddenInGame(false);
	Target->SetActorHiddenInGame(true);
	TestFalse(TEXT("Staged hidden target cannot be observed"), Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Sight, Target->GetActorLocation()));
	Target->SetActorHiddenInGame(false);
	Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Hearing, Target->GetActorLocation());
	Controller->ReportThreatObservation(OtherTarget, ENarrativeThreatSource::Sight, OtherTarget->GetActorLocation());
	auto* Blackboard = BlackboardFor(Controller);
	if (!TestNotNull(TEXT("Blackboard"), Blackboard)) { return false; }
	const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	Controller->SetFocus(OtherTarget);
	Controller->RefreshThreatMemory();
	TestTrue(TEXT("A stale attack key cannot replace valid independent focus"), Controller->GetFocusActor() == OtherTarget);
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	Controller->SetFocus(Target);
	const FVector NewOwnerLocation(123, 456, 789);
	bool bCallbackExecuted = false;
	Blackboard->RegisterObserver(Blackboard->GetKeyID(Settings->BBKey_AttackTarget), Controller,
		FOnBlackboardChangeNotification::CreateLambda([Controller, Replacement, OtherTarget, Blackboard, Settings, NewOwnerLocation, &bCallbackExecuted]
		(const UBlackboardComponent&, FBlackboard::FKey)
		{
			bCallbackExecuted = true;
			Controller->Possess(Replacement);
			Controller->SetFocus(OtherTarget);
			Blackboard->SetValueAsVector(Settings->BBKey_TargetLocation, NewOwnerLocation);
			return EBlackboardNotificationResult::RemoveObserver;
		}));
	Controller->RefreshThreatMemory();
	TestTrue(TEXT("Actual blackboard clear callback changed possession"), bCallbackExecuted && Controller->GetPawn() == Replacement);
	TestTrue(TEXT("Stale cleanup cannot overwrite the new pawn's focus"), Controller->GetFocusActor() == OtherTarget);
	TestTrue(TEXT("Stale cleanup cannot overwrite the new pawn's location"), Blackboard->GetValueAsVector(Settings->BBKey_TargetLocation).Equals(NewOwnerLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatPawnAssignmentRuntimeTest,
	"ProjectVelkorran.Campaign.Threat.PawnAssignmentReentryAndOutgoingASC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatPawnAssignmentRuntimeTest::RunTest(const FString& Parameters)
{
	FThreatWorld F;
	if (!TestNotNull(TEXT("World"), F.World)) { return false; }
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* FirstIncoming = F.Character(FVector(0, 200, 0), 0);
	auto* LatestIncoming = F.Character(FVector(0, 400, 0), 0);
	auto* Target = F.Character(FVector(500, 0, 0), 1);
	if (!Source || !FirstIncoming || !LatestIncoming || !Target) { AddError(TEXT("Character fixtures failed")); return false; }
	auto* Controller = F.Controller(Source);
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	Controller->bRequireThreatMemoryForTargeting = true;
	Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Hearing, Target->GetActorLocation());
	auto* Blackboard = BlackboardFor(Controller);
	if (!TestNotNull(TEXT("Blackboard"), Blackboard)) { return false; }
	const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
	Blackboard->SetValueAsObject(Settings->BBKey_AttackTarget, Target);
	const FVector LatestLocation(71, 82, 93);
	bool bReentered = false;
	Blackboard->RegisterObserver(Blackboard->GetKeyID(Settings->BBKey_AttackTarget), Controller,
		FOnBlackboardChangeNotification::CreateLambda([Controller, LatestIncoming, Target, Blackboard, Settings, LatestLocation, &bReentered]
		(const UBlackboardComponent&, FBlackboard::FKey)
		{
			bReentered = true;
			Controller->SetPawn(LatestIncoming);
			Controller->ReportThreatObservation(Target, ENarrativeThreatSource::Damage, Target->GetActorLocation());
			Controller->SetFocus(Target);
			Blackboard->SetValueAsVector(Settings->BBKey_TargetLocation, LatestLocation);
			return EBlackboardNotificationResult::RemoveObserver;
		}));
	Controller->SetPawn(FirstIncoming);
	TestTrue(TEXT("Cleanup callback reentered SetPawn"), bReentered);
	TestTrue(TEXT("Older assignment cannot overwrite the newer pawn"), Controller->GetPawn() == LatestIncoming);
	TestTrue(TEXT("Owned NPC matches the newest assignment"), Controller->GetOwnedNPC() == LatestIncoming);
	TestTrue(TEXT("Controller exposes only the newest ASC"), Controller->GetAbilitySystemComponent() == LatestIncoming->GetAbilitySystemComponent());
	TestTrue(TEXT("New owner focus survives the older operation"), Controller->GetFocusActor() == Target);
	TestTrue(TEXT("New owner location survives the older operation"), Blackboard->GetValueAsVector(Settings->BBKey_TargetLocation).Equals(LatestLocation));
	auto* OutgoingASC = Source->GetNarrativeAbilitySystemComponent();
	OutgoingASC->OnDeathStateChanged.Broadcast(Source, OutgoingASC, true);
	auto* SupersededASC = FirstIncoming->GetNarrativeAbilitySystemComponent();
	SupersededASC->OnDeathStateChanged.Broadcast(FirstIncoming, SupersededASC, true);
	TestTrue(TEXT("Outgoing and superseded death notifications cannot clear current memory"), Controller->CanDirectlyTargetThreat(Target));
	Controller->SetPawn(nullptr);
	TestNull(TEXT("Unpossess clears OwnedCharacter"), Controller->GetOwnedNPC());
	TestNull(TEXT("Unpossess does not expose the outgoing ASC"), Controller->GetAbilitySystemComponent());
	TestNull(TEXT("Unpossess clears actor focus"), Controller->GetFocusActor());
	return true;
}
#endif
