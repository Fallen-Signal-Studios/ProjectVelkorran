// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlacedNPCDefinitionTestFixtures.h"
#include "AI/NPCDefinition.h"
#include "Characters/SovDroneNPCBase.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Components/SovStatusComponent.h"
#include "NarrativeStableActor.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GAS/AbilityConfiguration.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
#include "Tests/SovTrackedContentPaths.h"

#if WITH_AUTOMATION_TESTS
namespace SovPlacedNPCDefinitionTests
{
	struct FWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}
		~FWorld() { if (World) { World->DestroyWorld(false); } }
		ASovPlacedNPCDefinitionTestCharacter* Spawn()
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World ? World->SpawnActor<ASovPlacedNPCDefinitionTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params) : nullptr;
		}
	};
	UNPCDefinition* Definition(bool bUnique = false)
	{
		auto* Result = NewObject<UNPCDefinition>();
		Result->NPCClassPath = ASovNPCCharacterBase::StaticClass();
		Result->bAllowMultipleInstances = !bUnique;
		Result->NPCID = FName(TEXT("PlacedDefinitionTest"));
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionFallbackTest,
	"ProjectVelkorran.Campaign.PlacedNPC.DefinitionFallbackAndExistingOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionFallbackTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Placed NPC fixture"), NPC)) { return false; }
	FString Error;
	TestFalse(TEXT("No authored definition cannot initialize"), NPC->InitializePlaced(Error));
	TestEqual(TEXT("No definition dispatch"), NPC->DefinitionDispatches, 0);
	auto* Authored = Definition();
	NPC->AuthoredPlacedDefinition = Authored;
	TestTrue(TEXT("Matching authored role enters normal definition dispatch"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Actual definition assigned"), NPC->GetNPCDefinition() == Authored);
	TestEqual(TEXT("One native dispatch"), NPC->DefinitionDispatches, 1);
	NPC->AuthoredPlacedDefinition = Definition();
	TestTrue(TEXT("Subsequent calls preserve existing runtime definition"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Existing definition wins over changed authored fallback"), NPC->GetNPCDefinition() == Authored);
	TestEqual(TEXT("No duplicate definition grants"), NPC->DefinitionDispatches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionRestoreTest,
	"ProjectVelkorran.Campaign.PlacedNPC.RestoreAndRoleRejection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionRestoreTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Placed NPC fixture"), NPC)) { return false; }
	FString Error;
	auto* WrongRole = Definition();
	WrongRole->NPCClassPath = ASovDroneNPCBase::StaticClass();
	NPC->AuthoredPlacedDefinition = WrongRole;
	TestFalse(TEXT("Different role cannot be applied to the placed actor"), NPC->InitializePlaced(Error));
	TestEqual(TEXT("Rejected role has no content callback"), NPC->DefinitionDispatches, 0);
	NPC->AuthoredPlacedDefinition = Definition();
	NPC->PrepareForEncounterRestore(FNPCSpawnInfo(), FGuid::NewGuid());
	TestFalse(TEXT("Restore cannot invent a missing definition from placed data"), NPC->InitializePlaced(Error));
	auto* Restored = Definition();
	NPC->SetNPCDefinition(Restored);
	TestTrue(TEXT("Supplied restore definition is retained"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Restore owns actual definition"), NPC->GetNPCDefinition() == Restored);
	TestEqual(TEXT("Restore definition dispatched once"), NPC->DefinitionDispatches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionUniqueTest,
	"ProjectVelkorran.Campaign.PlacedNPC.UniqueDefinitionAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionUniqueTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* First = Scope.Spawn(); auto* Second = Scope.Spawn();
	if (!TestNotNull(TEXT("First placed actor"), First) || !TestNotNull(TEXT("Second placed actor"), Second)) { return false; }
	auto* Unique = Definition(true);
	First->AuthoredPlacedDefinition = Unique; Second->AuthoredPlacedDefinition = Unique;
	FString Error;
	TestFalse(TEXT("Duplicate unique assignment fails before dispatch"), First->InitializePlaced(Error));
	TestFalse(TEXT("Reversed initialization order also rejects the duplicate"), Second->InitializePlaced(Error));
	TestEqual(TEXT("No definition dispatch on either actor"), First->DefinitionDispatches + Second->DefinitionDispatches, 0);
	Unique->bAllowMultipleInstances = true;
	TestTrue(TEXT("Explicit repeatable definition admits first actor"), First->InitializePlaced(Error));
	TestTrue(TEXT("Explicit repeatable definition admits second actor"), Second->InitializePlaced(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCStartupOrderingTest,
	"ProjectVelkorran.Campaign.PlacedNPC.AuthoredDefinitionPrecedesNativeASCStartup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCStartupOrderingTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	// The authored Enforcer, tracked under Content/Aurelion/ with every transitive dependency
	// tracked too, so this exercises shipped data without reaching the untracked SciFi_Drone_1
	// pack. It carries AC_Enforcer, whose six abilities and startup attributes all resolve. No
	// attributes, effects or abilities are applied by the fixture. Only asynchronous appearance
	// loading is suppressed.
	auto* Seed = LoadObject<UNPCDefinition>(nullptr,
		SovTrackedContentPaths::AuthoredEnforcerDefinition);
	if (!TestNotNull(TEXT("Existing authored combat-drone seed"), Seed)
		|| !TestNotNull(TEXT("Existing Narrative ability configuration"), Seed->AbilityConfiguration.Get())) { return false; }
	auto* Configuration = Seed->AbilityConfiguration.Get();
	if (!TestNotNull(TEXT("Seed supplies real default-attribute effect"), Configuration->DefaultAttributes.Get())
		|| !TestTrue(TEXT("Seed supplies actual default abilities"), Configuration->DefaultAbilities.Num() > 0)) { return false; }

	FWorld Scope;
	if (!TestNotNull(TEXT("Startup world"), Scope.World)) { return false; }
	Scope.World->InitializeActorsForPlay(FURL());
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Directly placed NPC"), NPC)) { return false; }
	auto* Authored = Definition();
	Authored->AbilityConfiguration = Configuration;
	NPC->AuthoredPlacedDefinition = Authored;
	auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
	if (!TestNotNull(TEXT("Native pawn-owned ASC"), ASC)) { return false; }
	TestFalse(TEXT("BeginPlay has not run before authored placement"), NPC->HasActorBegunPlay());
	TestEqual(TEXT("No manually seeded starting health"), NPC->GetHealth(), 0.f);
	TestFalse(TEXT("No manually applied startup effects"), ASC->bStartupEffectsApplied);
	NPC->DispatchBeginPlay();
	TestTrue(TEXT("Actual native BeginPlay completed"), NPC->HasActorBegunPlay());
	TestTrue(TEXT("Placed definition owns the first native startup"), NPC->GetNPCDefinition() == Authored);
	TestEqual(TEXT("Definition dispatch is exactly once"), NPC->DefinitionDispatches, 1);
	TestTrue(TEXT("Native attribute effect supplies positive maximum health"), NPC->GetMaxHealth() > 0.f);
	TestTrue(TEXT("Native attribute effect supplies living starting health"), NPC->GetHealth() > 0.f && NPC->GetHealth() <= NPC->GetMaxHealth());
	TestTrue(TEXT("Native startup-effect pipeline ran with its configuration"), ASC->bStartupEffectsApplied);
	TestTrue(TEXT("ASC belongs to its current pawn"), ASC->GetAvatarActor() == NPC);
	int32 GrantedAbilities = 0;
	int32 UnsetAbilityEntries = 0;
	for (const auto& AbilityClass : Configuration->DefaultAbilities)
	{
		// An unset array slot is not an ability and cannot be granted, so asserting a grant for it
		// would assert something incoherent. Count it and surface it below instead of skipping it
		// silently, because a real ability becoming unset is a regression worth seeing.
		if (!AbilityClass.Get()) { ++UnsetAbilityEntries; continue; }
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
		if (TestNotNull(*FString::Printf(TEXT("Native startup grants %s"), *GetNameSafe(AbilityClass.Get())), Spec))
		{
			++GrantedAbilities;
			TestTrue(TEXT("Grant retains Narrative configuration source ownership"), Spec->SourceObject.Get() == Configuration);
		}
	}
	// Without this the loop above would pass vacuously if every entry were unset.
	TestTrue(TEXT("At least one real startup ability was granted"), GrantedAbilities > 0);
	if (UnsetAbilityEntries > 0)
	{
		// An entry can be unset either because it was authored empty or because its asset is absent
		// from this checkout. AC_NPC_ReformationDrone is the known example of the latter: two of its
		// four entries point at GA_DroneGunfire and GA_DroneRocketAbility inside the untracked
		// SciFi_Drone_1 pack. The warning states the fact without guessing which cause applies.
		AddWarning(FString::Printf(
			TEXT("%s has %d of %d DefaultAbilities entries that do not resolve in this checkout. ")
			TEXT("That is either authored-empty or an absent dependency; see ")
			TEXT("Scripts/Manifests/ContentPrerequisites.json for known external packs."),
			*GetNameSafe(Configuration), UnsetAbilityEntries, Configuration->DefaultAbilities.Num()));
	}
	const float StartingHealth = NPC->GetHealth();
	const int32 StartingGrantCount = ASC->GetActivatableAbilities().Num();
	const int32 StartingEffectCount = ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
	FString Error;
	TestTrue(TEXT("Repeated fallback check remains idempotent"), NPC->InitializePlaced(Error));
	TestEqual(TEXT("Fallback never duplicates definition dispatch"), NPC->DefinitionDispatches, 1);
	TestEqual(TEXT("Fallback never reapplies attributes"), NPC->GetHealth(), StartingHealth);
	TestEqual(TEXT("Fallback never duplicates abilities"), ASC->GetActivatableAbilities().Num(), StartingGrantCount);
	TestEqual(TEXT("Fallback never duplicates persistent startup effects"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), StartingEffectCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCStartupPriorityTest,
	"ProjectVelkorran.Campaign.PlacedNPC.SpawnerAndRestoreDefinitionsOwnNativeStartup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCStartupPriorityTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	auto* Seed = LoadObject<UNPCDefinition>(nullptr,
		SovTrackedContentPaths::AuthoredEnforcerDefinition);
	if (!TestNotNull(TEXT("Existing combat-drone seed"), Seed)
		|| !TestNotNull(TEXT("Existing startup configuration"), Seed->AbilityConfiguration.Get())) { return false; }
	for (const bool bRestore : {false, true})
	{
		FWorld Scope;
		if (!TestNotNull(TEXT("Explicit-definition world"), Scope.World)) { return false; }
		Scope.World->InitializeActorsForPlay(FURL());
		auto* NPC = Scope.Spawn();
		if (!TestNotNull(TEXT("Explicit-definition NPC"), NPC)) { return false; }
		// Deliberately lacks a configuration: accidentally choosing this authored
		// fallback instead of the supplied definition would leave health at zero.
		NPC->AuthoredPlacedDefinition = Definition();
		if (bRestore) { NPC->PrepareForEncounterRestore(FNPCSpawnInfo(), FGuid::NewGuid()); }
		auto* Supplied = Definition();
		Supplied->AbilityConfiguration = Seed->AbilityConfiguration;
		NPC->SetNPCDefinition(Supplied);
		NPC->DispatchBeginPlay();
		TestTrue(bRestore ? TEXT("Restore definition wins before native startup") : TEXT("Spawner definition wins before native startup"),
			NPC->GetNPCDefinition() == Supplied);
		TestEqual(TEXT("Supplied definition dispatched once"), NPC->DefinitionDispatches, 1);
		TestTrue(TEXT("Supplied config initializes actual positive health"), NPC->GetHealth() > 0.f && NPC->GetMaxHealth() > 0.f);
		TestTrue(TEXT("Supplied config owns startup effects"), NPC->GetNarrativeAbilitySystemComponent()->bStartupEffectsApplied);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCHomeTransformTest,
	"ProjectVelkorran.Campaign.PlacedNPC.PlacementMetadataPrecedesDefinitionDispatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCHomeTransformTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Placed metadata actor"), NPC)) { return false; }
	const FTransform Placement(FRotator(0.f, 123.f, 0.f), FVector(1234.f, -5678.f, 432.f));
	if (!TestTrue(TEXT("Actual actor receives a nonidentity authored placement"), NPC->SetActorTransform(Placement))) { return false; }
	const FGuid SaveIdentity = FGuid::NewGuid();
	NPC->SpawnInfo.SpawnAssignedSaveGUID = SaveIdentity;
	NPC->SpawnInfo.SpawnParams.bOverride_NPCName = true;
	NPC->SpawnInfo.SpawnParams.NPCName = FText::FromString(TEXT("Existing placed override"));
	NPC->AuthoredPlacedDefinition = Definition();
	FString Error;
	TestTrue(TEXT("Uninitialized Narrative home starts at identity"), NPC->SpawnInfo.SpawnTransform.Equals(FTransform::Identity));
	if (!TestTrue(TEXT("Actual authored fallback succeeds"), NPC->InitializePlaced(Error))) { return false; }
	TestTrue(TEXT("Narrative home is the actual authored location and rotation"), NPC->SpawnInfo.SpawnTransform.Equals(Placement));
	TestTrue(TEXT("Definition callback already observes the correct home"), NPC->SpawnTransformAtDefinitionDispatch.Equals(Placement));
	TestEqual(TEXT("Durable save identity remains unchanged"), NPC->GetActorGUID_Implementation(), SaveIdentity);
	TestTrue(TEXT("Existing name override flag is retained"), NPC->SpawnInfo.SpawnParams.bOverride_NPCName);
	TestEqual(TEXT("Existing name override value is retained"), NPC->SpawnInfo.SpawnParams.NPCName.ToString(), FString(TEXT("Existing placed override")));
	const FTransform Later(FRotator(0.f, -30.f, 0.f), FVector(-1000.f, 900.f, 120.f));
	TestTrue(TEXT("Ordinary later movement succeeds"), NPC->SetActorTransform(Later));
	TestTrue(TEXT("Repeat fallback accepts already initialized definition"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Later movement does not rewrite the original Narrative home"), NPC->SpawnInfo.SpawnTransform.Equals(Placement));
	TestEqual(TEXT("No second definition callback"), NPC->DefinitionDispatches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCExplicitHomeOwnershipTest,
	"ProjectVelkorran.Campaign.PlacedNPC.ExplicitSpawnerAndRestoreHomeOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCExplicitHomeOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	if (!TestNotNull(TEXT("Deferred metadata world"), Scope.World)) { return false; }
	const FTransform Placement(FRotator(0.f, 75.f, 0.f), FVector(720.f, -1930.f, 210.f));
	const FTransform ExplicitHome(FRotator(0.f, -110.f, 0.f), FVector(-4500.f, 3700.f, 330.f));
	auto* Deferred = Scope.World->SpawnActorDeferred<ASovPlacedNPCDefinitionTestCharacter>(
		ASovPlacedNPCDefinitionTestCharacter::StaticClass(), Placement, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual deferred NPC"), Deferred)) { return false; }
	const FGuid Spawner = FGuid::NewGuid(), SavedActor = FGuid::NewGuid();
	Deferred->SpawnInfo.OwningSpawnerGUID = Spawner;
	Deferred->SpawnInfo.SpawnAssignedSaveGUID = SavedActor;
	Deferred->SpawnInfo.SpawnName = TEXT("OwnedSpawn");
	Deferred->SpawnInfo.SpawnTransform = ExplicitHome;
	Deferred->AuthoredPlacedDefinition = Definition();
	auto* Supplied = Definition();
	Deferred->SetNPCDefinition(Supplied);
	Deferred->FinishSpawning(Placement);
	FString Error;
	TestTrue(TEXT("Supplied deferred definition remains admitted"), Deferred->InitializePlaced(Error));
	TestTrue(TEXT("Supplied definition is retained"), Deferred->GetNPCDefinition() == Supplied);
	TestTrue(TEXT("Deferred external home is retained rather than current placement"), Deferred->SpawnInfo.SpawnTransform.Equals(ExplicitHome));
	TestTrue(TEXT("Supplied definition saw its external home"), Deferred->SpawnTransformAtDefinitionDispatch.Equals(ExplicitHome));
	TestEqual(TEXT("External spawner GUID retained"), Deferred->SpawnInfo.OwningSpawnerGUID, Spawner);
	TestEqual(TEXT("External save GUID retained"), Deferred->SpawnInfo.SpawnAssignedSaveGUID, SavedActor);
	TestEqual(TEXT("External spawn name retained"), Deferred->SpawnInfo.SpawnName, FName(TEXT("OwnedSpawn")));
	TestEqual(TEXT("Deferred definition dispatched exactly once"), Deferred->DefinitionDispatches, 1);
	// An external caller may stage metadata before supplying its definition.
	// Both a nonidentity explicit home and an intentional origin home remain owned.
	for (const bool bOriginHome : {false, true})
	{
		auto* Staged = Scope.Spawn();
		if (!TestNotNull(TEXT("External metadata-only actor"), Staged)) { return false; }
		Staged->SetActorTransform(Placement);
		const FTransform Home = bOriginHome ? FTransform::Identity : ExplicitHome;
		Staged->SpawnInfo.SpawnTransform = Home;
		if (bOriginHome) { Staged->SpawnInfo.OwningSpawnerGUID = Spawner; }
		Staged->AuthoredPlacedDefinition = Definition();
		TestTrue(TEXT("Authored fallback preserves externally staged metadata"), Staged->InitializePlaced(Error));
		TestTrue(TEXT("Explicit home not replaced by current location"), Staged->SpawnInfo.SpawnTransform.Equals(Home));
		TestTrue(TEXT("Definition callback sees externally staged home"), Staged->SpawnTransformAtDefinitionDispatch.Equals(Home));
	}
	auto* Restoring = Scope.Spawn();
	if (!TestNotNull(TEXT("Restoring metadata actor"), Restoring)) { return false; }
	Restoring->SetActorTransform(Placement);
	FNPCSpawnInfo SavedInfo;
	SavedInfo.SpawnTransform = ExplicitHome; SavedInfo.OwningSpawnerGUID = Spawner;
	Restoring->PrepareForEncounterRestore(SavedInfo, SavedActor);
	Restoring->AuthoredPlacedDefinition = Definition();
	TestFalse(TEXT("Restore never adopts authored fallback before its own definition"), Restoring->InitializePlaced(Error));
	TestTrue(TEXT("Restore home retained on rejected fallback"), Restoring->SpawnInfo.SpawnTransform.Equals(ExplicitHome));
	TestEqual(TEXT("Restore GUID retained"), Restoring->SpawnInfo.SpawnAssignedSaveGUID, SavedActor);
	TestEqual(TEXT("Restore has no unauthorized definition dispatch"), Restoring->DefinitionDispatches, 0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCStableIdentityMetadataTest,
	"ProjectVelkorran.Campaign.PlacedNPC.StableIdentityPrecedesDefinitionDispatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCStableIdentityMetadataTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* First = Scope.Spawn(); auto* Second = Scope.Spawn();
	if (!TestNotNull(TEXT("First identity fixture"), First) || !TestNotNull(TEXT("Second identity fixture"), Second)) { return false; }
	const FGuid FirstIdentity = First->ASovNPCCharacterBase::GetActorGUID_Implementation();
	const FGuid SecondIdentity = Second->ASovNPCCharacterBase::GetActorGUID_Implementation();
	TestTrue(TEXT("Native fallback is already stable and valid"), FirstIdentity.IsValid() && SecondIdentity.IsValid());
	TestNotEqual(TEXT("Separate placed actors have separate identities"), FirstIdentity, SecondIdentity);
	FString Error;
	TestFalse(TEXT("Unadmitted fallback cannot initialize"), First->InitializePlaced(Error));
	TestFalse(TEXT("Rejected fallback does not publish identity"), First->SpawnInfo.SpawnAssignedSaveGUID.IsValid());
	for (auto* NPC : {First, Second})
	{
		NPC->AuthoredPlacedDefinition = Definition();
		const FGuid Expected = NPC->ASovNPCCharacterBase::GetActorGUID_Implementation();
		if (!TestTrue(TEXT("Fresh placement admitted"), NPC->InitializePlaced(Error))) { return false; }
		TestEqual(TEXT("Spawn metadata contains the preexisting native identity"), NPC->SpawnInfo.SpawnAssignedSaveGUID, Expected);
		TestEqual(TEXT("Definition callback already sees the identity"), NPC->SpawnIdentityAtDefinitionDispatch, Expected);
		NPC->SetActorLocation(FVector(120.f, 340.f, 560.f));
		TestTrue(TEXT("Repeated initialization remains admitted"), NPC->InitializePlaced(Error));
		TestEqual(TEXT("Later movement never regenerates an assigned identity"), NPC->SpawnInfo.SpawnAssignedSaveGUID, Expected);
		TestEqual(TEXT("Identity publication does not duplicate definition dispatch"), NPC->DefinitionDispatches, 1);
	}
	auto* NativeSaved = Scope.Spawn();
	if (!TestNotNull(TEXT("Existing native identity fixture"), NativeSaved)) { return false; }
	const FGuid SavedIdentity = FGuid::NewGuid();
	NativeSaved->SetNativeIdentityForTest(SavedIdentity);
	NativeSaved->AuthoredPlacedDefinition = Definition();
	TestFalse(TEXT("Legacy native identity has no spawn metadata yet"), NativeSaved->SpawnInfo.SpawnAssignedSaveGUID.IsValid());
	if (!TestTrue(TEXT("Existing native identity fallback admitted"), NativeSaved->InitializePlaced(Error))) { return false; }
	TestEqual(TEXT("Existing saved native identity wins over the current actor path"), NativeSaved->SpawnInfo.SpawnAssignedSaveGUID, SavedIdentity);
	TestEqual(TEXT("Callback observes the preserved saved identity"), NativeSaved->SpawnIdentityAtDefinitionDispatch, SavedIdentity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCAuthoredBlueprintCaptureTest,
	"ProjectVelkorran.Campaign.PlacedNPC.AuthoredBlueprintIdentityAndCanonicalCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCAuthoredBlueprintCaptureTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	// Load the real project-owned copies of the existing drone and enforcer seeds.
	// No native fixture replaces their Blueprint GUID/definition callbacks, and no
	// attributes, GUIDs, components or ability grants are manually supplied here.
	const TCHAR* DefinitionPaths[] = {
		TEXT("/Game/Aurelion/Enemies/NPC_AurelionSecurityDrone.NPC_AurelionSecurityDrone"),
		TEXT("/Game/Aurelion/Enemies/NPC_AurelionEnforcer.NPC_AurelionEnforcer")
	};
	for (const TCHAR* DefinitionPath : DefinitionPaths)
	{
		auto* Authored = LoadObject<UNPCDefinition>(nullptr, DefinitionPath);
		if (!TestNotNull(DefinitionPath, Authored)
			|| !TestTrue(TEXT("Actual repeatable role definition"), Authored->bAllowMultipleInstances)
			|| !TestNotNull(TEXT("Actual role startup configuration"), Authored->AbilityConfiguration.Get())) { return false; }
		UClass* RoleClass = Authored->NPCClassPath.LoadSynchronous();
		if (!TestNotNull(TEXT("Actual authored Blueprint class"), RoleClass)
			|| !TestTrue(TEXT("Authored class is a Blueprint NPC"), RoleClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
				&& RoleClass->IsChildOf(ASovNPCCharacterBase::StaticClass()))) { return false; }
		FWorld Scope;
		if (!TestNotNull(TEXT("Actual Blueprint capture world"), Scope.World)) { return false; }
		Scope.World->InitializeActorsForPlay(FURL());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* NPC = Scope.World->SpawnActor<ASovNPCCharacterBase>(RoleClass, FVector(300.f, -450.f, 200.f), FRotator::ZeroRotator, SpawnParameters);
		if (!TestNotNull(TEXT("Actual Blueprint instance"), NPC)) { return false; }
		// DestroyWorld cleans up the world without immediately retiring every actor.
		// Real Blueprint definition/appearance loads are asynchronous, so destroy
		// this owner (and its native-owned visual) first on every test exit. UE then
		// marks them as garbage and their weak UObject delegates cannot run later.
		ON_SCOPE_EXIT
		{
			if (IsValid(NPC) && !NPC->IsActorBeingDestroyed())
			{
				TestTrue(TEXT("Actual Blueprint owner retires before its test world"), NPC->Destroy());
				TestFalse(TEXT("Retired owner cannot receive delayed appearance callbacks"), IsValid(NPC));
			}
		};
		TestFalse(TEXT("Actual placed actor has not begun play"), NPC->HasActorBegunPlay());
		TestNull(TEXT("No spawner has supplied a definition"), NPC->GetNPCDefinition());
		TestFalse(TEXT("No manually supplied spawn GUID"), NPC->GetEncounterSpawnInfo().SpawnAssignedSaveGUID.IsValid());
		const FGuid NativeIdentity = NPC->ASovNPCCharacterBase::GetActorGUID_Implementation();
		NPC->AuthoredPlacedDefinition = Authored;
		NPC->DispatchBeginPlay();
		if (!TestTrue(TEXT("Actual Blueprint survives native startup"), IsValid(NPC) && !NPC->IsActorBeingDestroyed())
			|| !TestTrue(TEXT("Actual placed definition was initialized"), NPC->GetNPCDefinition() == Authored)) { return false; }
		const FGuid BlueprintIdentity = INarrativeStableActor::Execute_GetActorGUID(NPC);
		TestTrue(TEXT("Actual Blueprint GUID callback returns a valid identity"), BlueprintIdentity.IsValid());
		TestEqual(TEXT("Actual Blueprint reads the native placed identity"), BlueprintIdentity, NativeIdentity);
		TestEqual(TEXT("Spawn metadata and actual Blueprint identity agree"), NPC->GetEncounterSpawnInfo().SpawnAssignedSaveGUID, BlueprintIdentity);
		auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
		auto* Save = Scope.World->GetSubsystem<UNarrativeSaveSubsystem>();
		if (!TestNotNull(TEXT("Actual native ASC"), ASC) || !TestNotNull(TEXT("Actual Narrative save subsystem"), Save)) { return false; }
		const float HealthBeforeCapture = NPC->GetHealth();
		const int32 GrantsBeforeCapture = ASC->GetActivatableAbilities().Num();
		TestTrue(TEXT("Real startup effect supplies living health"), HealthBeforeCapture > 0.f && HealthBeforeCapture <= NPC->GetMaxHealth());
		FNarrativeActorRecord Record;
		if (!TestTrue(TEXT("Actual Blueprint and all savable components create a Narrative record"), Save->CreateActorRecord(NPC, Record))) { return false; }
		TestTrue(TEXT("Captured actor record has serialized data"), Record.IsValid() && !Record.ByteData.IsEmpty());
		TestEqual(TEXT("Narrative capture retains actual Blueprint GUID"), Record.ActorGUID, BlueprintIdentity);
		TestTrue(TEXT("Narrative capture retains the actual authored class"), Record.ActorSoftClass.Get() == RoleClass);
		TestTrue(TEXT("Native ASC is present in the saved component record"), Record.SavedComponents.ContainsByPredicate(
			[ASC](const FNarrativeSaveComponent& ComponentRecord) { return ComponentRecord.ComponentName == ASC->GetFName(); }));
		const USovStatusComponent* Status = NPC->GetStatusComponent();
		TestTrue(TEXT("Native status passed serialized component validation"), IsValid(Status) && Record.SavedComponents.ContainsByPredicate(
			[Status](const FNarrativeSaveComponent& ComponentRecord) { return ComponentRecord.ComponentName == Status->GetFName(); }));
		FSovCombatResourceSnapshot Resources;
		if (!TestTrue(TEXT("Real authored startup resources support canonical capture"), USovEncounterSnapshotLibrary::CaptureResources(ASC, Resources))) { return false; }
		TestTrue(TEXT("All current/base resource pairs are valid"), Resources.SchemaVersion == 2 && Resources.IsValid());
		TestEqual(TEXT("Canonical snapshot contains actual health"), Resources.Health, HealthBeforeCapture);
		TestEqual(TEXT("Capture does not change health"), NPC->GetHealth(), HealthBeforeCapture);
		TestEqual(TEXT("Capture does not change ability grants"), ASC->GetActivatableAbilities().Num(), GrantsBeforeCapture);
		TestEqual(TEXT("Repeated Blueprint identity is stable"), INarrativeStableActor::Execute_GetActorGUID(NPC), BlueprintIdentity);
	}
	return true;
}
#endif
