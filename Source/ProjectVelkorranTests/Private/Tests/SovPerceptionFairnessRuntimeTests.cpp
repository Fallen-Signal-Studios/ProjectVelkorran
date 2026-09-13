// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovThreatAttackTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovEncounterRuntimeTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "UObject/Script.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	/**
	 * E6. A tracked hostile, reached through the same path the game uses to choose its AI controller:
	 * NPC definition -> NPC class -> AIControllerClass. The Security Drone is omitted: it shares the
	 * Enforcer's BP_NarrativeNPCController and sits beside the external SciFi_Drone_1 pack (X2).
	 */
	struct FSovPerceptionRosterEntry
	{
		const TCHAR* Name;
		const TCHAR* DefinitionPath;
		const TCHAR* PawnClassPath;
	};
	const FSovPerceptionRosterEntry SovPerceptionRoster[] = {
		{ TEXT("Aurelion Enforcer"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionEnforcer.NPC_AurelionEnforcer"), nullptr },
		{ TEXT("Aurelion Elite"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionElite.NPC_AurelionElite"), nullptr },
		{ TEXT("Aurelion Linkbound"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionLinkbound.NPC_AurelionLinkbound"), nullptr },
		{ TEXT("Aurelion WallRunner"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionWallRunner.NPC_AurelionWallRunner"), nullptr },
		{ TEXT("Aurelion Weaver"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionWeaver.NPC_AurelionWeaver"), nullptr },
		{ TEXT("Aurelion Contaminated Drone"), TEXT("/Game/Aurelion/Enemies/NPC_AurelionContaminatedDrone.NPC_AurelionContaminatedDrone"), nullptr },
		{ TEXT("Dominion Hound"), nullptr, TEXT("/NarrativePro/Pro/Core/AI/BP/BP_DominionHound.BP_DominionHound_C") },
		{ TEXT("Dominion Hound Master"), nullptr, TEXT("/NarrativePro/Pro/Core/AI/BP/BP_DominionHoundMaster.BP_DominionHoundMaster_C") },
	};

	TSubclassOf<AController> SovResolveAuthoredController(const FSovPerceptionRosterEntry& Entry)
	{
		UClass* PawnClass = nullptr;
		if (Entry.DefinitionPath)
		{
			if (const UNPCDefinition* Definition = LoadObject<UNPCDefinition>(nullptr, Entry.DefinitionPath))
			{
				PawnClass = Definition->NPCClassPath.LoadSynchronous();
			}
		}
		else
		{
			PawnClass = LoadClass<APawn>(nullptr, Entry.PawnClassPath);
		}
		const APawn* Defaults = PawnClass ? Cast<APawn>(PawnClass->GetDefaultObject()) : nullptr;
		return Defaults ? Defaults->AIControllerClass : nullptr;
	}

	/** A hostile combatant possessed by an authored controller class, with an undetected enemy 250 cm away. */
	struct FSovPerceptionWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovBotTestCharacter* Source = nullptr;
		ASovBotTestCharacter* Target = nullptr;
		ANarrativeNPCController* Controller = nullptr;
		FGameplayAbilitySpecHandle Bite;

		explicit FSovPerceptionWorld(TSubclassOf<AController> ControllerClass)
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldInitialization);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f);
			Source = Spawn(FVector::ZeroVector);
			Target = Spawn(FVector(250., 0., 0.));
			Controller = SpawnController(ControllerClass);
			if (!Source || !Target || !Controller) { return; }
			Source->InitializeTestCombat(0);
			Target->InitializeTestCombat(1);
			Target->GetCapsuleComponent()->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel, ECR_Block);
			Controller->Possess(Source);
			// Binds the authored perception component exactly as the controller's own refresh does in play.
			Controller->RefreshThreatMemory();
			Bite = Source->GetNarrativeAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(USovThreatHoundBite::StaticClass(), 1));
		}
		~FSovPerceptionWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		bool Valid() const { return Source && Target && Controller && Bite.IsValid(); }

		ASovBotTestCharacter* Spawn(const FVector& Location) const
		{
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}
		ANarrativeNPCController* SpawnController(TSubclassOf<AController> ControllerClass) const
		{
			if (!ControllerClass || !ControllerClass->IsChildOf(ANarrativeNPCController::StaticClass())) { return nullptr; }
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return Cast<ANarrativeNPCController>(World->SpawnActor(ControllerClass.Get(), &FTransform::Identity, Params));
		}
		UAIPerceptionComponent* Perception() const { return Controller ? Controller->GetAIPerceptionComponent() : nullptr; }

		/** A real stimulus through the authored perception component and its registered listener. */
		void Sense(ANarrativeNPCController* Listener, TSubclassOf<UAISense> Sense, bool bSensed) const
		{
			UAIPerceptionComponent* Component = Listener ? Listener->GetAIPerceptionComponent() : nullptr;
			const UAISense* SenseDefaults = Sense ? Sense->GetDefaultObject<UAISense>() : nullptr;
			if (!Component || !SenseDefaults || !Listener->GetPawn()) { return; }
			FAIStimulus Stimulus(*SenseDefaults, 1.f, Target->GetActorLocation(), Listener->GetPawn()->GetActorLocation());
			if (!bSensed) { Stimulus.MarkNoLongerSensed(); }
			Component->RegisterStimulus(Target, Stimulus);
			Component->ProcessStimuli();
		}
		/** The Hound bite's native acquisition: actor focus first, then its world scan for the nearest hostile. */
		AActor* TryBite() const
		{
			auto* ASC = Source->GetNarrativeAbilitySystemComponent();
			if (!ASC->TryActivateAbility(Bite)) { return nullptr; }
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Bite);
			const auto* Ability = Spec ? Cast<USovThreatHoundBite>(Spec->GetPrimaryInstance()) : nullptr;
			AActor* Acquired = Ability ? Ability->GetCurrentAttackTarget() : nullptr;
			if (Spec && Spec->IsActive()) { ASC->CancelAbilityHandle(Bite); }
			return Acquired;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerceptionRosterTest, "ProjectVelkorran.Campaign.PerceptionFairness.RosterControllersRequireObservationBeforeAcquisition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPerceptionRosterTest::RunTest(const FString& Parameters)
{
	for (const FSovPerceptionRosterEntry& Entry : SovPerceptionRoster)
	{
		const FString Who = Entry.Name;
		const TSubclassOf<AController> ControllerClass = SovResolveAuthoredController(Entry);
		if (!TestNotNull(Who + TEXT(": authored AI controller class resolves"), ControllerClass.Get())) { continue; }
		AddInfo(FString::Printf(TEXT("%s uses %s"), *Who, *ControllerClass->GetPathName()));
		FSovPerceptionWorld F(ControllerClass);
		if (!TestTrue(Who + TEXT(": authored controller possesses a real combatant"), F.Valid())) { continue; }

		const UAIPerceptionComponent* Perception = F.Perception();
		TestTrue(Who + TEXT(": controller carries authored sight and hearing perception"), Perception
			&& Perception->IsSenseEnabled(UAISense_Sight::StaticClass()) && Perception->IsSenseEnabled(UAISense_Hearing::StaticClass()));
		TestTrue(Who + TEXT(": controller is threat-memory managed, so the legacy nonperception fallback is unreachable"),
			F.Controller->IsThreatMemoryManaged());

		// Undetected, 250 cm away: well inside sight range and the bite's attack range.
		TestFalse(Who + TEXT(": an undetected player is not a direct target"), F.Controller->CanDirectlyTargetThreat(F.Target));
		TestNull(Who + TEXT(": the Hound world scan does not acquire an undetected player by distance"), F.TryBite());

		// Squad awareness only through the explicit, bounded sharing channel.
		ASovBotTestCharacter* AllyPawn = F.Spawn(FVector(0., 300., 0.));
		ANarrativeNPCController* Ally = F.SpawnController(ControllerClass);
		if (TestTrue(Who + TEXT(": an allied authored controller exists"), AllyPawn && Ally))
		{
			AllyPawn->InitializeTestCombat(0);
			Ally->Possess(AllyPawn);
			Ally->RefreshThreatMemory();
			const FGameplayTagContainer Faction(FNarrativeGameplayTags::Get().Narrative_Factions_Heroes);
			UArsenalStatics::AddFactionsToActor(F.Source, Faction);
			UArsenalStatics::AddFactionsToActor(AllyPawn, Faction);
			TestFalse(Who + TEXT(": an ally cannot be alerted while the sender has no observation at all"), F.Controller->ShareThreatWith(Ally, F.Target));
			TestFalse(Who + TEXT(": the ally has no direct target of its own"), Ally->CanDirectlyTargetThreat(F.Target));
		}

		// Hearing: investigation only, for the listener and for any ally it tells.
		F.Sense(F.Controller, UAISense_Hearing::StaticClass(), true);
		FNarrativeThreatMemory Memory;
		TestTrue(Who + TEXT(": a noise records an investigation position"),
			F.Controller->GetBestThreatMemory(F.Target, Memory) && Memory.LastKnownPosition.Equals(F.Target->GetActorLocation()));
		TestFalse(Who + TEXT(": hearing alone cannot authorise direct targeting"), F.Controller->CanDirectlyTargetThreat(F.Target));
		TestNull(Who + TEXT(": hearing alone cannot start an attack"), F.TryBite());
		if (Ally)
		{
			TestTrue(Who + TEXT(": a heard noise can be passed to the squad"), F.Controller->ShareThreatWith(Ally, F.Target));
			FNarrativeThreatMemory Heard;
			TestTrue(Who + TEXT(": the ally holds an ally alert for the noise"), Ally->GetBestThreatMemory(F.Target, Heard) && Heard.Source == ENarrativeThreatSource::AllyAlert);
			TestFalse(Who + TEXT(": a relayed noise gives the ally no direct target"), Ally->CanDirectlyTargetThreat(F.Target));
		}

		// Sight: legitimate acquisition.
		F.Sense(F.Controller, UAISense_Sight::StaticClass(), true);
		TestTrue(Who + TEXT(": line-of-sight perception authorises direct targeting"), F.Controller->CanDirectlyTargetThreat(F.Target));
		TestTrue(Who + TEXT(": the bite acquires the player once seen"), F.TryBite() == F.Target);
		if (Ally)
		{
			TestTrue(Who + TEXT(": a seen threat can be shared with the ally"), F.Controller->ShareThreatWith(Ally, F.Target));
			FNarrativeThreatMemory Shared;
			TestTrue(Who + TEXT(": the ally holds an ally alert"), Ally->GetBestThreatMemory(F.Target, Shared) && Shared.Source == ENarrativeThreatSource::AllyAlert);
			TestFalse(Who + TEXT(": an ally alert is investigation, not a direct target"), Ally->CanDirectlyTargetThreat(F.Target));
		}

		// Losing sight: last-known position, not omniscience.
		const FVector LastSeen = F.Target->GetActorLocation();
		F.Sense(F.Controller, UAISense_Sight::StaticClass(), false);
		F.Target->SetActorLocation(FVector(650., 150., 0.));
		F.Controller->RefreshThreatMemory();
		TestFalse(Who + TEXT(": losing sight revokes direct targeting"), F.Controller->CanDirectlyTargetThreat(F.Target));
		TestNull(Who + TEXT(": the bite cannot reacquire after sight is lost"), F.TryBite());
		TestTrue(Who + TEXT(": memory keeps the last-known position rather than tracking the player"),
			F.Controller->GetBestThreatMemory(F.Target, Memory) && Memory.LastKnownPosition.Equals(LastSeen));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerceptionEncounterTest, "ProjectVelkorran.Campaign.PerceptionFairness.EncounterActivationGrantsNoTargetWithoutAuthoredReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPerceptionEncounterTest::RunTest(const FString& Parameters)
{
	const TSubclassOf<AController> ControllerClass = SovResolveAuthoredController(SovPerceptionRoster[0]);
	if (!TestNotNull(TEXT("Enforcer's authored controller class resolves"), ControllerClass.Get())) { return false; }
	FSovPerceptionWorld F(ControllerClass);
	if (!TestTrue(TEXT("Authored controller possesses a real combatant"), F.Valid())) { return false; }

	auto* Director = F.World->SpawnActor<ASovEncounterRuntimeTestDirector>();
	if (!TestNotNull(TEXT("Encounter director"), Director)) { return false; }
	Director->SeedState(ESovEncounterState::Active);
	F.Controller->RefreshThreatMemory();
	TestEqual(TEXT("The encounter is active"), Director->GetEncounterState(), ESovEncounterState::Active);
	TestFalse(TEXT("Encounter activation alone grants no direct target"), F.Controller->CanDirectlyTargetThreat(F.Target));
	TestNull(TEXT("Encounter activation alone starts no attack"), F.TryBite());

	TestTrue(TEXT("A command broadcast is accepted as an observation"), F.Controller->ReportThreatObservation(
		F.Target, ENarrativeThreatSource::Command, F.Target->GetActorLocation(), 1.f, 1.f, 4.f));
	TestFalse(TEXT("A command broadcast is investigation, not a direct target"), F.Controller->CanDirectlyTargetThreat(F.Target));
	TestNull(TEXT("A command broadcast starts no attack"), F.TryBite());

	// Scripted or forced combat overrides stealth explicitly, through an authored direct report.
	TestTrue(TEXT("An authored forced-combat report is accepted"), F.Controller->ReportThreatObservation(
		F.Target, ENarrativeThreatSource::Damage, F.Target->GetActorLocation(), 1.f, 1.f, 4.f));
	TestTrue(TEXT("The authored report authorises direct targeting"), F.Controller->CanDirectlyTargetThreat(F.Target));
	TestTrue(TEXT("The authored report lets the attack acquire the player"), F.TryBite() == F.Target);
	return true;
}
#endif
