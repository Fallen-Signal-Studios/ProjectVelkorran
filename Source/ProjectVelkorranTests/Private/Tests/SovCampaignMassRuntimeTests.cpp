// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignMassProxy.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Tests/SovCoordinationRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "AI/NPCDefinition.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MassAgentComponent.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
struct FSovCampaignMassTestAccess
{
	static void Active(ASovEncounterDirector& Director)
	{
		Director.State = ESovEncounterState::Active;
		Director.AttemptId = FGuid::NewGuid();
	}

	static bool SeedMass(ASovEncounterDirector& Director, const FName Id, const FGuid ActorIdentity,
		const FTransform& Transform, FString& Error)
	{
		FSovEncounterMassRecord Record;
		Record.NPC.ParticipantId = Id;
		Record.NPC.ActorRecord.ActorGUID = ActorIdentity;
		Record.NPC.ActorRecord.Transform = Transform;
		Record.NPC.bRequiredForVictory = true;
		Record.Route = { Transform.GetLocation(), Transform.GetLocation() + FVector(300.f, 0.f, 0.f) };
		Record.NextRoutePoint = 1;
		Record.RouteSpeed = 125.f;
		// Synthetic records intentionally have no visual assets: this tests entity and
		// save ownership independently of authored capture/promotion fixtures.
		const int32 Index = Director.MassParticipants.Add(Record);
		if (!Director.CreateMassEntity(Director.MassParticipants[Index], Error))
		{
			Director.MassParticipants.RemoveAt(Index);
			return false;
		}
		return true;
	}

	static FMassEntityHandle Entity(const ASovEncounterDirector& Director, FName Id)
	{ return Director.MassEntities.FindRef(Id); }
	static uint64 Epoch(const ASovEncounterDirector& Director, FName Id)
	{ return Director.MassEpochs.FindRef(Id); }
	static int32 RecordCount(const ASovEncounterDirector& Director) { return Director.MassParticipants.Num(); }
	static int32 EntityCount(const ASovEncounterDirector& Director) { return Director.MassEntities.Num(); }
	static int32 ProxyCount(const ASovEncounterDirector& Director) { return Director.MassProxies.Num(); }
	static FSovEncounterMassRecord Record(const ASovEncounterDirector& Director, FName Id)
	{
		const auto* Found = Director.MassParticipants.FindByPredicate([Id](const auto& Value) { return Value.NPC.ParticipantId == Id; });
		return Found ? *Found : FSovEncounterMassRecord();
	}
	static void CaptureTransforms(ASovEncounterDirector& Director) { Director.CaptureMassTransforms(); }
	static void StepPromotion(ASovEncounterDirector& Director) { Director.TickMassPromotions(); }
	static void BindDeaths(ASovEncounterDirector& Director) { Director.BindDeaths(); }
	static void PendingPromotion(ASovEncounterDirector& Director, FName Id, ASovNPCCharacterBase* NPC)
	{ Director.MassPromotions.Add(Id, NPC); }
	static void RetirePromotion(ASovEncounterDirector& Director, FName Id) { Director.MassPromotions.Remove(Id); }
	static int32 ResidentAssets(const ASovEncounterDirector& Director, FName Id)
	{ const auto* Lease = Director.MassAssetResidency.Find(Id); return Lease ? Lease->Assets.Num() : 0; }
	static void DestroyEntity(ASovEncounterDirector& Director, FName Id) { Director.DestroyMassEntity(Id); }
	static bool RecreateEntity(ASovEncounterDirector& Director, FName Id, FString& Error)
	{
		auto* Record = Director.MassParticipants.FindByPredicate([Id](const auto& Value) { return Value.NPC.ParticipantId == Id; });
		return Record && Director.CreateMassEntity(*Record, Error);
	}
};

namespace
{
	struct FCampaignMassRuntimeWorld
	{
		UWorld* World = nullptr;
		FCampaignMassRuntimeWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		}
		~FCampaignMassRuntimeWorld()
		{
			if (!World) { return; }
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
		ASovEncounterDirector* Director(FName Id)
		{
			auto* Result = World ? World->SpawnActor<ASovEncounterDirector>() : nullptr;
			if (Result) { Result->EncounterId = Id; FSovCampaignMassTestAccess::Active(*Result); }
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignMassEntityOwnershipTest,
	"ProjectVelkorran.Campaign.Mass.DirectorEntityIdentityRouteAndSaveOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovCampaignMassEntityOwnershipTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCampaignMassRuntimeWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	auto* Director = Fixture.Director(TEXT("Test.MassEntity"));
	auto* Entities = Fixture.World->GetSubsystem<UMassEntitySubsystem>();
	if (!TestNotNull(TEXT("Director"), Director) || !TestNotNull(TEXT("Real Mass subsystem"), Entities)) { return false; }
	FMassEntityManager& Manager = Entities->GetMutableEntityManager();
	const FName Id(TEXT("Required.Courtyard.Guard"));
	const FGuid StableIdentity = FGuid::NewGuid();
	const FTransform Initial(FRotator(0.f, 35.f, 0.f), FVector(250.f, 40.f, 10.f));
	FSovEncounterParticipant Participant;
	Participant.ParticipantId = Id;
	Participant.bAllowMassRepresentation = true;
	Director->Participants.Add(Participant);
	FString Error;
	if (!TestTrue(TEXT("Actual Mass template creates the director-owned entity"),
		FSovCampaignMassTestAccess::SeedMass(*Director, Id, StableIdentity, Initial, Error)))
	{
		AddError(Error);
		return false;
	}
	const FMassEntityHandle Entity = FSovCampaignMassTestAccess::Entity(*Director, Id);
	TestTrue(TEXT("Entity is live in the world Mass manager"), Manager.IsEntityValid(Entity));
	const auto* Identity = Manager.GetFragmentDataPtr<FNarrativeMassParticipantFragment>(Entity);
	if (!TestNotNull(TEXT("Dedicated participant identity fragment"), Identity)) { return false; }
	const FNarrativeMassParticipantFragment OriginalIdentity = *Identity;
	TestTrue(TEXT("Receipt stores the exact owning director"), Identity->Owner.Get() == Director);
	TestEqual(TEXT("Stable actor GUID survives the actor-to-entity identity seam"), Identity->ActorIdentity, StableIdentity);
	TestEqual(TEXT("Participant ID survives"), Identity->ParticipantId, Id);
	TestEqual(TEXT("Encounter ID survives"), Identity->EncounterId, Director->EncounterId);
	TestTrue(TEXT("Entity generation is nonzero and owned by this director"),
		Identity->Generation != 0 && Identity->Generation == FSovCampaignMassTestAccess::Epoch(*Director, Id));
	TestTrue(TEXT("All campaign representations remain noncombat presentation actors"), Identity->bPresentationOnly);
	TestNull(TEXT("Campaign template never acquires random pedestrian data"), Manager.GetFragmentDataPtr<FNarrativePedFragment>(Entity));
	TestTrue(TEXT("Initial transform is transferred exactly"),
		Manager.GetFragmentDataChecked<FTransformFragment>(Entity).GetTransform().Equals(Initial));
	TestTrue(TEXT("Demoted participant retains required-victory identity without a gameplay actor"),
		Director->IsParticipantMassRepresented(Id) && Director->GetParticipant(Id) == nullptr && Director->Participants[0].bRequiredForVictory);
	int32 NPCCount = 0;
	for (TActorIterator<ASovNPCCharacterBase> It(Fixture.World); It; ++It) { ++NPCCount; }
	TestEqual(TEXT("Entity creation does not recreate an NPC or GAS avatar"), NPCCount, 0);

	const TArray<FVector> Points = { FVector(250.f, 40.f, 10.f), FVector(800.f, 40.f, 10.f), FVector(1000.f, 200.f, 10.f) };
	TestTrue(TEXT("Valid route is admitted for the active owned entity"), Director->SetParticipantMassRoute(Id, Points, 220.f, Error));
	auto& Route = Manager.GetFragmentDataChecked<FSovCampaignMassRouteFragment>(Entity);
	TestTrue(TEXT("Actual entity owns route points"), Route.Points == Points);
	TestEqual(TEXT("Actual entity owns route speed"), Route.Speed, 220.f);
	TestEqual(TEXT("A changed route starts at its first point"), Route.NextPoint, 0);
	TestFalse(TEXT("NaN route speed is rejected"), Director->SetParticipantMassRoute(Id, Points, std::numeric_limits<float>::quiet_NaN(), Error));
	TArray<FVector> TooMany; TooMany.Init(FVector::ZeroVector, 129);
	TestFalse(TEXT("Oversized route is rejected"), Director->SetParticipantMassRoute(Id, TooMany, 100.f, Error));
	TestTrue(TEXT("Rejected route preserves live route"), Route.Points == Points && Route.Speed == 220.f);
	TestFalse(TEXT("Unapproved boundary cannot change tier"),
		Director->SetParticipantRepresentation(Id, ESovCampaignRepresentationTier::Presentation, TEXT("Unapproved"), Error));
	TestTrue(TEXT("Approved boundary switches existing entity to dormant presentation"),
		Director->SetParticipantRepresentation(Id, ESovCampaignRepresentationTier::Presentation, TEXT("Encounter.ControlledBoundary"), Error));
	TestTrue(TEXT("Dormant presentation freezes entity route"), Route.bPresentationOnly);
	TestTrue(TEXT("Dormant transition keeps the exact same entity"), FSovCampaignMassTestAccess::Entity(*Director, Id) == Entity);

	const FTransform SavedTransform(FRotator(0.f, 90.f, 0.f), FVector(800.f, 40.f, 10.f));
	Manager.GetFragmentDataChecked<FTransformFragment>(Entity).GetMutableTransform() = SavedTransform;
	Route.NextPoint = 2;
	FSovCampaignMassTestAccess::CaptureTransforms(*Director);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, true);
	SaveArchive.ArIsSaveGame = true;
	Director->Serialize(SaveArchive);
	TestFalse(TEXT("Director SaveGame serialization succeeds"), SaveArchive.IsError());
	auto* Restored = Fixture.Director(TEXT("Test.MassEntity.Copy"));
	if (!TestNotNull(TEXT("Restored record owner"), Restored)) { return false; }
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Restored->Serialize(LoadArchive);
	TestFalse(TEXT("Director SaveGame deserialization succeeds"), LoadArchive.IsError());
	TestEqual(TEXT("Exactly one authoritative participant record survives SaveGame"), FSovCampaignMassTestAccess::RecordCount(*Restored), 1);
	const FSovEncounterMassRecord Saved = FSovCampaignMassTestAccess::Record(*Restored, Id);
	TestEqual(TEXT("Record keeps stable actor GUID"), Saved.NPC.ActorRecord.ActorGUID, StableIdentity);
	TestTrue(TEXT("Record keeps current Mass transform"), Saved.NPC.ActorRecord.Transform.Equals(SavedTransform));
	TestTrue(TEXT("Record keeps the finite corridor route"), Saved.Route == Points);
	TestEqual(TEXT("Record keeps route progress"), Saved.NextRoutePoint, 2);
	TestEqual(TEXT("Record keeps route speed"), Saved.RouteSpeed, 220.f);
	TestEqual(TEXT("Record keeps presentation tier"), Saved.Tier, ESovCampaignRepresentationTier::Presentation);
	TestTrue(TEXT("Required-victory flag survives SaveGame"), Saved.NPC.bRequiredForVictory);
	TestEqual(TEXT("Transient Mass handles are not serialized into another owner"), FSovCampaignMassTestAccess::EntityCount(*Restored), 0);
	TestEqual(TEXT("Transient representation actors are not serialized"), FSovCampaignMassTestAccess::ProxyCount(*Restored), 0);
	TestEqual(TEXT("Runtime receipt generation is not a saved association"), FSovCampaignMassTestAccess::Epoch(*Restored, Id), static_cast<uint64>(0));

	auto* Proxy = Fixture.World->SpawnActor<ASovCampaignMassProxy>();
	if (!TestNotNull(TEXT("Configured non-NPC proxy class"), Proxy)) { return false; }
	TestNull(TEXT("Campaign proxy has no MassAgent"), Proxy->FindComponentByClass<UMassAgentComponent>());
	auto* Receipt = NewObject<UNarrativeMassParticipantReceiptComponent>(Proxy);
	Proxy->AddInstanceComponent(Receipt); Receipt->RegisterComponent();
	TestFalse(TEXT("Assetless synthetic record cannot fabricate a valid visual receipt"), Receipt->Bind(Manager.AsShared(), Entity));
	TestFalse(TEXT("Failed visual acceptance leaves no current receipt"), Receipt->IsCurrent(Manager));
	TestEqual(TEXT("Failed visual acceptance cleans its owner registry"), FSovCampaignMassTestAccess::ProxyCount(*Director), 0);
	FSovCampaignMassTestAccess::DestroyEntity(*Director, Id);
	TestFalse(TEXT("Logical receipt invalidates before deferred entity removal"), Director->IsMassRepresentationCurrent(Manager, Entity, *Proxy, OriginalIdentity));
	Manager.FlushCommands();
	TestFalse(TEXT("Mass teardown removes the original entity"), Manager.IsEntityValid(Entity));
	TestTrue(TEXT("Teardown retains authoritative record for recreation"), Director->IsParticipantMassRepresented(Id));
	TestTrue(TEXT("Retained record recreates a real entity"), FSovCampaignMassTestAccess::RecreateEntity(*Director, Id, Error));
	const FMassEntityHandle Replacement = FSovCampaignMassTestAccess::Entity(*Director, Id);
	TestTrue(TEXT("Replacement has a live, distinct entity handle"), Manager.IsEntityValid(Replacement) && Replacement != Entity);
	TestTrue(TEXT("Replacement advances participant generation"), FSovCampaignMassTestAccess::Epoch(*Director, Id) > OriginalIdentity.Generation);
	TestFalse(TEXT("Original actor/entity/generation callback cannot take over replacement"),
		Director->AcceptMassRepresentation(Manager, Entity, *Proxy, OriginalIdentity));
	Proxy->Destroy();
	FSovCampaignMassTestAccess::DestroyEntity(*Director, Id);
	Manager.FlushCommands();
	TestFalse(TEXT("Replacement also tears down without a stale entity"), Manager.IsEntityValid(Replacement));
	TestEqual(TEXT("No stale proxy actor remains owned"), FSovCampaignMassTestAccess::ProxyCount(*Director), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignMassRequiredWaveTest,
	"ProjectVelkorran.Campaign.Mass.RequiredDemotedParticipantBlocksWaveAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovCampaignMassRequiredWaveTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCampaignMassRuntimeWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	auto* Director = Fixture.Director(TEXT("Test.MassRequiredWave"));
	if (!TestNotNull(TEXT("Director"), Director)) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* First = Fixture.World->SpawnActor<ASovCoordinationTestNPC>(ASovCoordinationTestNPC::StaticClass(), FVector(200.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
	auto* Future = Fixture.World->SpawnActor<ASovCoordinationTestNPC>(ASovCoordinationTestNPC::StaticClass(), FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Current wave NPC fixture"), First) || !TestNotNull(TEXT("Future wave NPC fixture"), Future)) { return false; }
	First->InitializeTestCombat(); Future->InitializeTestCombat();
	auto* Coordination = Director->GetCoordinationComponent();
	if (!TestNotNull(TEXT("Real encounter coordinator"), Coordination)) { return false; }
	FSovEncounterParticipant CurrentParticipant;
	CurrentParticipant.ParticipantId = TEXT("Required.Current"); CurrentParticipant.Character = First;
	CurrentParticipant.bAllowMassRepresentation = true;
	FSovEncounterParticipant FutureParticipant;
	FutureParticipant.ParticipantId = TEXT("Required.Future"); FutureParticipant.Character = Future;
	Director->Participants = { CurrentParticipant, FutureParticipant };
	FSovEncounterCompositionMember CurrentMember; CurrentMember.ParticipantId = CurrentParticipant.ParticipantId;
	FSovEncounterCompositionMember FutureMember; FutureMember.ParticipantId = FutureParticipant.ParticipantId; FutureMember.Wave = 1;
	Coordination->Composition = { CurrentMember, FutureMember };
	FString Error;
	TestTrue(TEXT("Two-wave authored composition is valid"), Coordination->ValidateComposition(Error));
	Coordination->InitializeCoordination();
	TestTrue(TEXT("Future wave is staged before demotion"), Future->IsHidden());
	if (!TestTrue(TEXT("Required current participant gains a real director-owned Mass entity"),
		FSovCampaignMassTestAccess::SeedMass(*Director, CurrentParticipant.ParticipantId, FGuid::NewGuid(), First->GetActorTransform(), Error)))
	{
		AddError(Error);
		return false;
	}
	Director->Participants[0].Character = nullptr;
	First->Destroy();
	TestTrue(TEXT("Composition accepts an explicitly represented required participant"), Coordination->ValidateComposition(Error));
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		// Invoke the public UActorComponent tick contract, retaining the real virtual override.
		static_cast<UActorComponent*>(Coordination)->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("A missing actor backed by Mass is not a defeat gate"), Coordination->GetCurrentWave(), 0);
	TestTrue(TEXT("Future wave stays staged while required participant is demoted"), Future->IsHidden());
	TestFalse(TEXT("Future wave collision remains disabled"), Future->GetActorEnableCollision());
	TestEqual(TEXT("Required Mass representation does not complete the encounter"), Director->GetEncounterState(), ESovEncounterState::Active);
	FSovCampaignMassTestAccess::DestroyEntity(*Director, CurrentParticipant.ParticipantId);
	if (auto* Entities = Fixture.World->GetSubsystem<UMassEntitySubsystem>()) { Entities->GetMutableEntityManager().FlushCommands(); }
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignMassRoundTripTest,
	"ProjectVelkorran.Campaign.Mass.RealNPCSnapshotDemotionPromotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignMassRoundTripTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FCampaignMassRuntimeWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	auto* Save = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!TestNotNull(TEXT("Actual Narrative save authority"), Save) || !TestTrue(TEXT("In-memory save initializes without disk writes"), Save->UpdateSaveObject(true))) { return false; }
	auto* Director = Fixture.Director(TEXT("Test.MassRoundTrip"));
	auto* Entities = Fixture.World->GetSubsystem<UMassEntitySubsystem>();
	if (!Director || !Entities) { AddError(TEXT("Director/Mass subsystem unavailable")); return false; }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Original = Fixture.World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(), FVector(125.f, 45.f, 0.f), FRotator::ZeroRotator, Spawn);
	if (!Original || !Original->FindComponentByClass<UStaticMeshComponent>()->GetStaticMesh())
	{ AddError(TEXT("Native fixture requires engine BasicShapes/Cube mesh")); return false; }
	TStrongObjectPtr<UNPCDefinition> Definition(NewObject<UNPCDefinition>());
	Original->SetNPCDefinition(Definition.Get());
	const FGuid Stable = Original->GetActorGUID_Implementation();
	auto* ASC = Original->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 47.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 12.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 31.f);
	const FGameplayAbilitySpecHandle Attack = ASC->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
	FSovEncounterParticipant Participant; Participant.ParticipantId = TEXT("Required.Guard"); Participant.Character = Original;
	Participant.bAllowMassRepresentation = true; Director->Participants.Add(Participant);
	Director->GetCoordinationComponent()->InitializeCoordination();
	FString Error;
	TestTrue(TEXT("Actual native GAS attack activates before conversion test"), ASC->TryActivateAbility(Attack));
	TestFalse(TEXT("Active attack conversion rejected without cancellation"), Director->SetParticipantRepresentation(Participant.ParticipantId,
		ESovCampaignRepresentationTier::Mass, TEXT("Encounter.ControlledBoundary"), Error));
	TestTrue(TEXT("Rejected conversion leaves exact original NPC"), Director->GetParticipant(Participant.ParticipantId) == Original);
	ASC->CancelAbilityHandle(Attack);
	// A skeletal body without a valid locomotion profile must retain its gameplay actor.
	TStrongObjectPtr<USkeletalMesh> UnsupportedBody(NewObject<USkeletalMesh>());
	Original->GetMesh()->SetSkeletalMesh(UnsupportedBody.Get());
	TestFalse(TEXT("Tier C rejects a skeletal body without animation profile"), Director->SetParticipantRepresentation(Participant.ParticipantId,
		ESovCampaignRepresentationTier::Mass, TEXT("Encounter.ControlledBoundary"), Error));
	TestTrue(TEXT("Profile rejection preserves visible original actor and ownership"), Director->GetParticipant(Participant.ParticipantId) == Original
		&& IsValid(Original) && !Original->IsActorBeingDestroyed() && !Original->IsHidden());
	Original->GetMesh()->SetSkeletalMesh(nullptr);
	if (!TestTrue(TEXT("Real NPC snapshot demotes through director"), Director->SetParticipantRepresentation(Participant.ParticipantId,
		ESovCampaignRepresentationTier::Mass, TEXT("Encounter.ControlledBoundary"), Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Original actor is destroyed, not parked/pool-reused"), !IsValid(Original) || Original->IsActorBeingDestroyed());
	TestNull(TEXT("Mass owns the participant without an ASC actor"), Director->GetParticipant(Participant.ParticipantId));
	const FMassEntityHandle Entity = FSovCampaignMassTestAccess::Entity(*Director, Participant.ParticipantId);
	auto* Proxy = Fixture.World->SpawnActor<ASovCampaignMassProxy>();
	auto* Receipt = NewObject<UNarrativeMassParticipantReceiptComponent>(Proxy); Proxy->AddInstanceComponent(Receipt); Receipt->RegisterComponent();
	TestTrue(TEXT("Exact captured static equipment visual binds to actual entity receipt"), Receipt->Bind(Entities->GetMutableEntityManager().AsShared(), Entity));
	TestTrue(TEXT("Captured visual creates real proxy mesh"), Proxy->FindComponentByClass<UStaticMeshComponent>() != nullptr);
	TestTrue(TEXT("Director owns asset residency independently of visual proxy"), FSovCampaignMassTestAccess::ResidentAssets(*Director, Participant.ParticipantId) >= 3);
	TWeakObjectPtr<UNPCDefinition> RetainedDefinition(Definition.Get());
	Definition.Reset(); Proxy->Destroy(); CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Definition survives source and proxy destruction plus real GC"), RetainedDefinition.IsValid());
	const FVector Moved(440.f, 85.f, 0.f);
	Entities->GetMutableEntityManager().GetFragmentDataChecked<FTransformFragment>(Entity).GetMutableTransform().SetLocation(Moved);
	if (!TestTrue(TEXT("Promotion starts a new campaign class instance"), Director->SetParticipantRepresentation(Participant.ParticipantId,
		ESovCampaignRepresentationTier::Actor, TEXT("Encounter.ControlledBoundary"), Error))) { AddError(Error); return false; }
	ASovNPCCharacterBase* Replacement = Director->GetParticipant(Participant.ParticipantId);
	TestTrue(TEXT("Promotion never reuses original actor"), Replacement && Replacement != Original);
	TestTrue(TEXT("Replacement is hidden until all snapshot fields verify"), Replacement && Replacement->IsHidden());
	FSovCampaignMassTestAccess::StepPromotion(*Director);
	if (!TestEqual(TEXT("Verified promotion keeps encounter active"), Director->GetEncounterState(), ESovEncounterState::Active)) { return false; }
	TestFalse(TEXT("Promotion completes asynchronously through readiness gate"), Director->IsMassPromotionPending());
	TestFalse(TEXT("Successful promotion relinquishes Mass record"), Director->IsParticipantMassRepresented(Participant.ParticipantId));
	TestEqual(TEXT("Verified actor promotion releases director residency lease"), FSovCampaignMassTestAccess::ResidentAssets(*Director, Participant.ParticipantId), 0);
	TestEqual(TEXT("Stable participant actor GUID restored"), Replacement->GetActorGUID_Implementation(), Stable);
	TestTrue(TEXT("Entity current position, not original spawn, wins promotion"), Replacement->GetActorLocation().Equals(Moved));
	TestEqual(TEXT("Health preserved"), Replacement->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 47.f);
	TestEqual(TEXT("Shield preserved"), Replacement->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 12.f);
	TestEqual(TEXT("Poise preserved"), Replacement->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 31.f);
	TestTrue(TEXT("Required mission ownership remains registered"), Director->Participants[0].bRequiredForVictory);
	TestTrue(TEXT("Coordinator rebinds the replacement ASC"), Replacement->GetNarrativeAbilitySystemComponent()->GetBotAttackCoordinator() == Director->GetCoordinationComponent());
	TestFalse(TEXT("Verified replacement is visible"), Replacement->IsHidden());
	Entities->GetMutableEntityManager().FlushCommands();
	TestFalse(TEXT("Old Mass entity removed after handoff"), Entities->GetMutableEntityManager().IsEntityValid(Entity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterDeferredVictoryRepairTest,
	"ProjectVelkorran.Campaign.Encounter.DeferredVictorySurvivesOptionalPromotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterDeferredVictoryRepairTest::RunTest(const FString& Parameters)
{
	FCampaignMassRuntimeWorld Fixture;
	auto* Director = Fixture.Director(TEXT("Test.DeferredVictory"));
	auto* Required = Fixture.World ? Fixture.World->SpawnActor<ASovCoordinationTestNPC>() : nullptr;
	auto* Optional = Fixture.World ? Fixture.World->SpawnActor<ASovCoordinationTestNPC>() : nullptr;
	if (!Director || !Required || !Optional) { AddError(TEXT("Native encounter fixture unavailable.")); return false; }
	Required->InitializeTestCombat(); Optional->InitializeTestCombat();
	FSovEncounterParticipant A; A.ParticipantId = TEXT("Required"); A.Character = Required;
	FSovEncounterParticipant B; B.ParticipantId = TEXT("Optional"); B.Character = Optional; B.bRequiredForVictory = false;
	Director->Participants = { A, B }; FSovCampaignMassTestAccess::BindDeaths(*Director);
	FSovCampaignMassTestAccess::PendingPromotion(*Director, B.ParticipantId, Optional);
	auto* ASC = CastChecked<USovCoordinationTestASC>(Required->GetNarrativeAbilitySystemComponent());
	ASC->SeedDead(true); ASC->OnDeathStateChanged.Broadcast(Required, ASC, true);
	TestTrue(TEXT("Actual ASC death delegate persists confirmed defeat while mutation cannot complete"), Director->IsParticipantDefeatConfirmed(A.ParticipantId));
	TestEqual(TEXT("Outstanding optional promotion blocks immediate completion"), Director->GetEncounterState(), ESovEncounterState::Active);
	FSovCampaignMassTestAccess::RetirePromotion(*Director, B.ParticipantId);
	Director->ReconcileEncounterState();
	TestEqual(TEXT("Stabilization completes without a second death event"), Director->GetEncounterState(), ESovEncounterState::Succeeded);
	TestTrue(TEXT("Completion reward granted once"), Director->ClaimCompletionReward(TEXT("Once")));
	Director->ReconcileEncounterState();
	TestFalse(TEXT("Reconciliation cannot duplicate completion reward"), Director->ClaimCompletionReward(TEXT("Once")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterRequiredActorLossRepairTest,
	"ProjectVelkorran.Campaign.Encounter.UnexpectedActorLossFailsWithoutForgedDefeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterRequiredActorLossRepairTest::RunTest(const FString& Parameters)
{
	FCampaignMassRuntimeWorld Fixture;
	auto* Director = Fixture.Director(TEXT("Test.RequiredActorLoss"));
	auto* Required = Fixture.World ? Fixture.World->SpawnActor<ASovCoordinationTestNPC>() : nullptr;
	auto* Optional = Fixture.World ? Fixture.World->SpawnActor<ASovCoordinationTestNPC>() : nullptr;
	if (!Director || !Required || !Optional) { AddError(TEXT("Native encounter fixture unavailable.")); return false; }
	Required->InitializeTestCombat(); Optional->InitializeTestCombat();
	FSovEncounterParticipant A; A.ParticipantId = TEXT("Required"); A.Character = Required;
	FSovEncounterParticipant B; B.ParticipantId = TEXT("Optional"); B.Character = Optional; B.bRequiredForVictory = false;
	Director->Participants = { A, B }; FSovCampaignMassTestAccess::BindDeaths(*Director);
	Optional->Destroy();
	TestTrue(TEXT("Optional actor destruction does not invent mandatory failure"), Director->ReconcileEncounterState());
	Required->Destroy(); Director->ReconcileEncounterState();
	TestEqual(TEXT("Unexpected required actor destruction exits active encounter"), Director->GetEncounterState(), ESovEncounterState::Failed);
	TestFalse(TEXT("Destruction never fabricates a confirmed defeat"), Director->IsParticipantDefeatConfirmed(A.ParticipantId));
	TestFalse(TEXT("Required actor loss grants no victory reward"), Director->ClaimCompletionReward(TEXT("NoReward")));
	return true;
}
#endif
