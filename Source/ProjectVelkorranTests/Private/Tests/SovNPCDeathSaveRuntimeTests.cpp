// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNPCDeathSaveTestFixtures.h"
#include "Tests/SovNPCVisualLifecycleTestFixtures.h"
#include "AI/NPCDefinition.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "NarrativeSavableActor.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace SovNPCDeathSaveTests
{
	struct FWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		UNarrativeSaveSubsystem* Saves = nullptr;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Saves = World ? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
			if (Saves) { Saves->UpdateSaveObject(true); }
		}
		~FWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		static bool InitializeAttributes(UNarrativeAbilitySystemComponent* ASC, float Health = 100.f)
		{
			if (!ASC || !ASC->GetAttributeSet(UNarrativeAttributeSetBase::StaticClass())) { return false; }
			ASC->AttributesToSave = { UNarrativeAttributeSetBase::GetMaxHealthAttribute(), UNarrativeAttributeSetBase::GetHealthAttribute() };
			ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
			ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), Health);
			ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
			return ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()) == 100.f
				&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) == Health;
		}
		ASovNPCVisualLifecycleTestCharacter* Spawn(bool bPlaced = true)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* NPC = World->SpawnActor<ASovNPCVisualLifecycleTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!NPC) { return nullptr; }
			// This fixture excludes AI and asynchronous appearance, not the real NPC/ASC save or death paths.
			NPC->AutoPossessAI = EAutoPossessAI::Disabled;
			NPC->AIControllerClass = nullptr;
			// An unbegun level otherwise classifies every client-loadable actor as startup.
			// This fixture explicitly supplies map-load policy for placed vs dynamic NPCs.
			NPC->bNetLoadOnClient = bPlaced;
			NPC->bNetStartup = bPlaced;
			auto* Definition = NewObject<UNPCDefinition>(NPC);
			Definition->NPCClassPath = ASovNPCVisualLifecycleTestCharacter::StaticClass();
			Definition->bAllowMultipleInstances = true;
			Definition->NPCID = TEXT("DeathSaveFixture");
			NPC->SetNPCDefinition(Definition);
			auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
			auto* Attributes = NPC->GetAttributeSetBase();
			if (!ASC || !Attributes) { return nullptr; }
			// This unbegun fixture must register the real default subobject before actor-info death binding.
			ASC->AddAttributeSetSubobject(Attributes);
			if (ASC->GetAttributeSet(UNarrativeAttributeSetBase::StaticClass()) != Attributes) { return nullptr; }
			ASC->InitAbilityActorInfo(NPC, NPC);
			if (!InitializeAttributes(ASC)) { return nullptr; }
			// Bind the existing reflected native handler without accessing its protected C++ member externally.
			const FName DeathHandler(TEXT("HandleDeath"));
			if (!NPC->FindFunction(DeathHandler)) { return nullptr; }
			FScriptDelegate NativeDeath;
			NativeDeath.BindUFunction(NPC, DeathHandler);
			ASC->OnDeathStateChanged.AddUnique(NativeDeath);
			return NPC;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCDeathCaptureTest,
	"ProjectVelkorran.Campaign.NPCDeathSave.CorpseCaptureKeepsTerminalStateAndRealRevive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCDeathCaptureTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCDeathSaveTests;
	for (bool bPlaced : {true, false})
	{
		FWorld F;
		if (!TestNotNull(TEXT("Native world save owner"), F.Saves)) { return false; }
		auto* NPC = F.Spawn(bPlaced);
		if (!TestNotNull(TEXT("Stable NPC with native ASC"), NPC)) { return false; }
		auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
		TestEqual(TEXT("Actual source placement category before damage"), NPC->IsNetStartupActor(), bPlaced);
		const FGuid Guid = INarrativeStableActor::Execute_GetActorGUID(NPC);
		TestTrue(TEXT("Save the actual living NPC before damage"), F.Saves->SaveSingleActor(NPC));
		ASC->DealDamage(200.f);
		if (!TestTrue(TEXT("Real native damage killed this still-present corpse"), ASC->IsDead() && !NPC->IsAlive())) { return false; }
		TestTrue(TEXT("Native corpse is still a valid actor before checkpoint capture"), IsValid(NPC) && !NPC->IsActorBeingDestroyed());
		TestEqual(TEXT("Native death preserves the actual placement category"), NPC->IsNetStartupActor(), bPlaced);
		if (bPlaced)
		{
			const auto* Existing = F.Saves->GetSaveObject()->RecordMap.Find(Guid);
			if (!TestNotNull(TEXT("Native death marked its placed record"), Existing)) { return false; }
			TestTrue(TEXT("Native death owns the initial tombstone"), Existing->bDestroyed);
		}
		UNarrativeSave* Captured = nullptr;
		if (!TestTrue(TEXT("Actual whole-world checkpoint capture while corpse remains"), F.Saves->CaptureSaveObject(Captured))) { return false; }
		TStrongObjectPtr<UNarrativeSave> KeepCaptured(Captured);
		const auto* DeadRecord = Captured->RecordMap.Find(Guid);
		if (!TestNotNull(TEXT("World capture retains exact dead identity"), DeadRecord)) { return false; }
		TestTrue(TEXT("Capture cannot replace terminal state with a living record"), DeadRecord->bDestroyed);
		TestEqual(TEXT("Record retains actual placement category"), DeadRecord->bNetStartup, bPlaced);
		FWorld Destination;
		auto* Replacement = Destination.Spawn(bPlaced);
		if (!TestNotNull(TEXT("Separate loaded-world NPC"), Replacement)) { return false; }
		TestTrue(TEXT("Native tombstone load retires an existing placed or dynamic NPC"), Destination.Saves->LoadActorFromRecord(Replacement, *DeadRecord));
		TestTrue(TEXT("No resurrected actor remains"), !IsValid(Replacement) || Replacement->IsActorBeingDestroyed());
		ASC->Revive();
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 37.f);
		TestTrue(TEXT("A real native revive changes the owner state"), NPC->IsAlive());
		UNarrativeSave* Revived = nullptr;
		if (!TestTrue(TEXT("Actual new world capture after revive"), F.Saves->CaptureSaveObject(Revived))) { return false; }
		TStrongObjectPtr<UNarrativeSave> KeepRevived(Revived);
		const auto* LiveRecord = Revived->RecordMap.Find(Guid);
		if (!TestNotNull(TEXT("Revived same-GUID record"), LiveRecord)) { return false; }
		TestFalse(TEXT("Old tombstone cannot suppress an actual revive"), LiveRecord->bDestroyed);
		auto* LiveReplacement = Destination.Spawn(bPlaced);
		TestTrue(TEXT("Native live record load succeeds"), Destination.Saves->LoadActorFromRecord(LiveReplacement, *LiveRecord));
		TestTrue(TEXT("Revived state remains living after load"), LiveReplacement->IsAlive());
		TestEqual(TEXT("Actual revived health survives serialization"), LiveReplacement->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 37.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCLegacyDeathRestoreTest,
	"ProjectVelkorran.Campaign.NPCDeathSave.InitializedLegacyZeroHealthRestoresWithoutDamageReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCLegacyDeathRestoreTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCDeathSaveTests;
	FWorld Source;
	auto* Dead = Source.Spawn();
	if (!TestNotNull(TEXT("Native source NPC"), Dead)) { return false; }
	Dead->GetNarrativeAbilitySystemComponent()->DealDamage(200.f);
	if (!TestFalse(TEXT("Source died through actual native damage"), Dead->IsAlive())) { return false; }
	FNarrativeActorRecord Legacy;
	if (!TestTrue(TEXT("Capture real actor and ASC attribute bytes"), Source.Saves->CreateActorRecord(Dead, Legacy))) { return false; }
	// Exact historical format defect: the wrapper lost bDestroyed, while all serialized zero-health bytes remained.
	Legacy.bDestroyed = false;
	FWorld Destination;
	auto* NPC = Destination.Spawn();
	if (!TestNotNull(TEXT("Real restored NPC"), NPC)) { return false; }
	auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
	TStrongObjectPtr<USovNPCDeathSaveObserver> Observer(NewObject<USovNPCDeathSaveObserver>());
	ASC->OnDeathStateChanged.AddDynamic(Observer.Get(), &USovNPCDeathSaveObserver::ObserveDeath);
	ASC->OnDamageResolvedAsTarget.AddDynamic(Observer.Get(), &USovNPCDeathSaveObserver::ObserveDamage);
	int32 DeathEvents = 0, KillEvents = 0;
	const auto DeathTag = FNarrativeGameplayTags::Get().GameplayEvent_Death;
	const auto KillTag = FNarrativeGameplayTags::Get().GameplayEvent_KilledEnemy;
	const auto DeathHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(DeathTag).AddLambda([&](const FGameplayEventData*) { ++DeathEvents; });
	const auto KillHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(KillTag).AddLambda([&](const FGameplayEventData*) { ++KillEvents; });
	TestTrue(TEXT("Native legacy actor/component deserialization succeeds"), Destination.Saves->LoadActorFromRecord(NPC, Legacy));
	TestTrue(TEXT("Initialized zero health is terminal for the exact NPC avatar"), ASC->IsDead() && !NPC->IsAlive());
	TestTrue(TEXT("Native dead state tag agrees with IsAlive"), ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead));
	TestEqual(TEXT("One native death-state convergence"), Observer->DeathTransitions, 1);
	TestEqual(TEXT("Restoration emits no new damage receipt"), Observer->DamageResults, 0);
	TestEqual(TEXT("Restoration does not replay the death ability gameplay event"), DeathEvents, 0);
	TestEqual(TEXT("Restoration does not emit a kill/reward event"), KillEvents, 0);
	TestTrue(TEXT("Repeated actual record load is admitted"), Destination.Saves->LoadActorFromRecord(NPC, Legacy));
	TestEqual(TEXT("Repeated load does not repeat the death-state transition"), Observer->DeathTransitions, 1);
	TestEqual(TEXT("Repeated load still emits no damage"), Observer->DamageResults, 0);
	ASC->GenericGameplayEventCallbacks.FindChecked(DeathTag).Remove(DeathHandle);
	ASC->GenericGameplayEventCallbacks.FindChecked(KillTag).Remove(KillHandle);
	ASC->OnDeathStateChanged.RemoveDynamic(Observer.Get(), &USovNPCDeathSaveObserver::ObserveDeath);
	ASC->OnDamageResolvedAsTarget.RemoveDynamic(Observer.Get(), &USovNPCDeathSaveObserver::ObserveDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCDeathLoadOwnershipTest,
	"ProjectVelkorran.Campaign.NPCDeathSave.MissingInitializationPlayerAndNewerLoadRemainAuthoritative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCDeathLoadOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCDeathSaveTests;
	FWorld F;
	auto* Source = F.Spawn();
	auto* Target = F.Spawn();
	if (!TestNotNull(TEXT("Source NPC"), Source) || !TestNotNull(TEXT("Target NPC"), Target)) { return false; }
	FNarrativeActorRecord AliveRecord, ZeroRecord, IncompleteRecord;
	if (!TestTrue(TEXT("Actual living record"), F.Saves->CreateActorRecord(Source, AliveRecord))) { return false; }
	auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
	SourceASC->DealDamage(200.f);
	if (!TestTrue(TEXT("Actual fatal record"), F.Saves->CreateActorRecord(Source, ZeroRecord))) { return false; }
	ZeroRecord.bDestroyed = false; // Historical wrapper defect only; actual attribute bytes are preserved.
	SourceASC->AttributesToSave.Remove(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	if (!TestTrue(TEXT("A legacy record without explicit initialized maximum"), F.Saves->CreateActorRecord(Source, IncompleteRecord))) { return false; }
	IncompleteRecord.bDestroyed = false;
	TestTrue(TEXT("Missing initialized maximum still loads its ordinary attributes"), F.Saves->LoadActorFromRecord(Target, IncompleteRecord));
	TestTrue(TEXT("Missing initialized maximum cannot invent NPC death"), Target->IsAlive());
	TestTrue(TEXT("Restore actual living record before reentrant test"), F.Saves->LoadActorFromRecord(Target, AliveRecord));
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	bool bEntered = false, bNewerLoaded = false;
	const auto Health = UNarrativeAttributeSetBase::GetHealthAttribute();
	const auto Handle = ASC->GetGameplayAttributeValueChangeDelegate(Health).AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (!bEntered && Change.NewValue == 0.f)
		{
			bEntered = true;
			bNewerLoaded = F.Saves->LoadActorFromRecord(Target, AliveRecord);
		}
	});
	TestTrue(TEXT("Outer native legacy load unwinds safely"), F.Saves->LoadActorFromRecord(Target, ZeroRecord));
	ASC->GetGameplayAttributeValueChangeDelegate(Health).Remove(Handle);
	TestTrue(TEXT("Actual attribute callback performed a newer full record load"), bEntered && bNewerLoaded);
	TestTrue(TEXT("Retired outer load cannot kill newer restored life"), Target->IsAlive());
	TestEqual(TEXT("Newer actual health remains authoritative"), ASC->GetNumericAttribute(Health), 100.f);
	auto* WrongAvatar = F.Spawn();
	if (!TestNotNull(TEXT("Real replacement Narrative avatar"), WrongAvatar)) { return false; }
	ASC->InitAbilityActorInfo(Target, WrongAvatar);
	TestTrue(TEXT("Native actor info actually adopted the other avatar"), ASC->GetAvatarActor() == WrongAvatar);
	TestTrue(TEXT("Ordinary record attributes can load before avatar ownership settles"), F.Saves->LoadActorFromRecord(Target, ZeroRecord));
	TestFalse(TEXT("A different NPC's ASC cannot infer this avatar's death"), ASC->IsDead());
	TestTrue(TEXT("Other avatar's actual ASC remains living"), WrongAvatar->IsAlive());
	ASC->InitAbilityActorInfo(Target, Target);
	auto* RetiredTarget = F.Spawn();
	if (!TestNotNull(TEXT("Separate component-retirement NPC"), RetiredTarget)) { return false; }
	auto* RetiredASC = RetiredTarget->GetNarrativeAbilitySystemComponent();
	bool bRetired = false;
	const auto RetireHandle = RetiredASC->GetGameplayAttributeValueChangeDelegate(Health).AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (!bRetired && Change.NewValue == 0.f) { bRetired = true; RetiredASC->DestroyComponent(); }
	});
	TestFalse(TEXT("Native loader rejects a component destroyed by an attribute callback"), F.Saves->LoadActorFromRecord(RetiredTarget, ZeroRecord));
	RetiredASC->GetGameplayAttributeValueChangeDelegate(Health).Remove(RetireHandle);
	TestTrue(TEXT("Actual component destruction occurred with the avatar still living"), bRetired && RetiredASC->IsBeingDestroyed() && IsValid(RetiredTarget));
	TestFalse(TEXT("Retired ASC cannot publish a later restored death state"), RetiredASC->IsDead());
	auto* PS = F.World->SpawnActor<ASovPlayerState>();
	auto* Player = F.World->SpawnActor<ASovPlayerCharacterBase>();
	if (!TestNotNull(TEXT("Actual player state"), PS) || !TestNotNull(TEXT("Actual player avatar"), Player)) { return false; }
	auto* PlayerASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!TestNotNull(TEXT("Actual player-owned ASC"), PlayerASC)) { return false; }
	auto* PlayerAttributes = PS->GetAttributeSetBase();
	if (!TestNotNull(TEXT("Actual player-state attribute default subobject"), PlayerAttributes)) { return false; }
	PlayerASC->AddAttributeSetSubobject(PlayerAttributes);
	if (!TestTrue(TEXT("Player ASC registers that exact existing set"), PlayerASC->GetAttributeSet(UNarrativeAttributeSetBase::StaticClass()) == PlayerAttributes)) { return false; }
	PlayerASC->InitAbilityActorInfo(PS, Player);
	if (!TestTrue(TEXT("Actual player attributes initialize only after registration"), FWorld::InitializeAttributes(PlayerASC, 0.f))) { return false; }
	FNarrativeActorRecord PlayerRecord;
	if (!TestTrue(TEXT("Capture a player record with zero health"), F.Saves->CreateActorRecord(PS, PlayerRecord))) { return false; }
	TestTrue(TEXT("Actual player record loads"), F.Saves->LoadActorFromRecord(PS, PlayerRecord));
	TestFalse(TEXT("NPC reconciliation cannot convert player downed state to death"), PlayerASC->IsDead());
	return true;
}
#endif
