// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovReplicationReadinessTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovPassiveDefenseTestFixtures.h"
#include "Tests/SovRuntimeActorTestFixtures.h"
#include "Campaign/SovEncounterDirector.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Exertion/SovExertionComponent.h"
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include <atomic>
#include "Net/UnrealNetwork.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Resonance/SovResonanceComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "Targeting/SovAimAssist.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
// These tests prove the single-player campaign stays multiplayer-safe where it is cheap to be: each exercises the
// proxy or remote-player side of a rule by giving an actor a non-authority role, or a pawn a controller that is not
// local, inside one standalone world. They do not replace a real client/server session.
struct FSovReplicationReadinessTestAccess
{
	static void ArmEncounter(ASovEncounterDirector* Director, ASovPlayerCharacterBase* Player, bool bEntryCheckpoint)
	{ Director->EncounterPlayer = Player; Director->bHasEntryCheckpoint = bEntryCheckpoint; }
	static void SetCharges(USovFieldRecoveryComponent* Recovery, int32 Charges) { Recovery->Charges = Charges; }
	static void ReceiveCharges(USovFieldRecoveryComponent* Recovery) { Recovery->OnRep_Charges(); }
	static void Unbind(USovExertionComponent* Exertion) { Exertion->Uninitialize(); }
};

namespace
{
/** Counts the ability system's warning for a loose tag written with a different replication state than it holds. */
struct FSovTagReplicationStateWarnings : public FOutputDevice
{
	FSovTagReplicationStateWarnings() { GLog->AddOutputDevice(this); }
	virtual ~FSovTagReplicationStateWarnings() override { GLog->RemoveOutputDevice(this); }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{ if (Message && FCString::Strifind(Message, TEXT("replication state"))) { ++Count; } }
	/** Log lines can be dispatched after the call that wrote them; flush before reading. */
	int32 Mismatches() const { GLog->Flush(); return Count.load(); }
private:
	std::atomic<int32> Count{0};
};
struct FReplicationWorld
{
	UWorld* World = nullptr;
	FReplicationWorld()
	{
		const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
		if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	}
	~FReplicationWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	template <typename T> T* Spawn(FVector Location = FVector::ZeroVector)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationExertionProxyTest, "ProjectVelkorran.Replication.ExertionProxyPredictsProfileSpeedsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationExertionProxyTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Proxy = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Proxy character"), Proxy)) { return false; }
	auto* Movement = Cast<UNarrativeCharacterMovement>(Proxy->GetCharacterMovement());
	if (!TestNotNull(TEXT("Narrative movement"), Movement)) { return false; }
	Movement->SlowWalkSpeed = 1.f; Movement->MaxWalkSpeed = 2.f; Movement->SprintSpeed = 3.f;
	Proxy->SetRole(ROLE_SimulatedProxy);
	Proxy->InitializeExertion();
	const FSovExertionProfile Profile = Proxy->GetExertionComponent()->GetProfile();
	TestTrue(TEXT("A non-authority exertion owner still binds"), Proxy->GetExertionComponent()->IsInitialized());
	// Movement speeds do not replicate, so a predicting client must use the same profile the server moves with.
	TestEqual(TEXT("Proxy walk speed follows the profile"), Movement->SlowWalkSpeed, Profile.WalkSpeed);
	TestEqual(TEXT("Proxy run speed follows the profile"), Movement->MaxWalkSpeed, Profile.RunSpeed);
	TestEqual(TEXT("Proxy sprint speed follows the profile"), Movement->SprintSpeed, Profile.SprintSpeed);

	// A proxy never writes the Exhausted tag; it receives the authority's replicated count.
	auto* ASC = Proxy->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 0.f);
	TestFalse(TEXT("Proxy exhaustion does not author the tag locally"),
		ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Exertion_Exhausted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationExhaustionTagTest, "ProjectVelkorran.Replication.ExhaustionTagReplicatesToProxies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationExhaustionTagTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Player = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Authority character"), Player)) { return false; }
	Player->InitializeExertion();
	auto* ASC = Player->GetNarrativeAbilitySystemComponent();
	const FGameplayTag Exhausted = FSovGameplayTags::Get().State_Exertion_Exhausted;
	// UE 5.7 keeps a loose tag's replication state on its count item and warns when an authority changes it. An
	// existing replicated contribution therefore distinguishes a replicated write from a local-only one.
	ASC->AddLooseGameplayTag(Exhausted, 1, EGameplayTagReplicationState::TagAndCountToAll);
	FSovTagReplicationStateWarnings Warnings;
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 0.f);
	TestEqual(TEXT("Authority exhaustion adds its contribution"), ASC->GetGameplayTagCount(Exhausted), 2);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 50.f);
	TestEqual(TEXT("Recovery removes only its contribution"), ASC->GetGameplayTagCount(Exhausted), 1);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 0.f);
	FSovReplicationReadinessTestAccess::Unbind(Player->GetExertionComponent());
	TestEqual(TEXT("Unbinding releases only the contribution it owned"), ASC->GetGameplayTagCount(Exhausted), 1);
	TestEqual(TEXT("Every exhaustion write uses the replicated tag-and-count state"), Warnings.Mismatches(), 0);
	ASC->RemoveLooseGameplayTag(Exhausted, 1, EGameplayTagReplicationState::TagAndCountToAll);
	TestFalse(TEXT("No exhaustion tag remains"), ASC->HasMatchingGameplayTag(Exhausted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationShieldBreakTest, "ProjectVelkorran.Replication.ShieldBreakPresentsOnProxies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationShieldBreakTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Authority = F.World ? F.Spawn<ASovPassiveDefenseTestActor>() : nullptr;
	auto* Proxy = F.World ? F.Spawn<ASovPassiveDefenseTestActor>(FVector(500., 0., 0.)) : nullptr;
	if (!TestNotNull(TEXT("Authority owner"), Authority) || !TestNotNull(TEXT("Proxy owner"), Proxy)) { return false; }
	Authority->InitializeCombat(); Proxy->InitializeCombat();
	Proxy->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("Both shields are bound"), Authority->Shield->IsInitialized() && Proxy->Shield->IsInitialized());

	Authority->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	// On a client the Shield attribute arrives by replication and fires the same attribute-change delegate.
	Proxy->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	TestEqual(TEXT("The authority presents its break once"), Authority->ShieldBreaks, 1);
	TestEqual(TEXT("A proxy that learns of the break also presents it once"), Proxy->ShieldBreaks, 1);
	TestTrue(TEXT("The proxy's broken state matches"), Proxy->Shield->IsShieldBroken());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationFieldChargesTest, "ProjectVelkorran.Replication.FieldRecoveryChargesReachOwningClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationFieldChargesTest::RunTest(const FString& Parameters)
{
	const auto* Default = GetDefault<USovFieldRecoveryComponent>();
	const FProperty* Charges = FindFProperty<FProperty>(USovFieldRecoveryComponent::StaticClass(), TEXT("Charges"));
	if (!TestNotNull(TEXT("Charges property"), Charges)) { return false; }
	TestTrue(TEXT("The component replicates by default"), Default->GetIsReplicated());
	TestTrue(TEXT("Charges replicate with a notification"), Charges->HasAllPropertyFlags(CPF_Net | CPF_RepNotify));
	// Replication indices are assigned when the net driver first prepares a class; do the same before reading them.
	USovFieldRecoveryComponent::StaticClass()->SetUpRuntimeReplicationData();
	TArray<FLifetimeProperty> Lifetime; static_cast<const UObject*>(Default)->GetLifetimeReplicatedProps(Lifetime);
	const FLifetimeProperty* Entry = Lifetime.FindByPredicate([Charges](const FLifetimeProperty& Item) { return Item.RepIndex == Charges->RepIndex; });
	TestTrue(TEXT("Charges replicate only to the owning player"), Entry && Entry->Condition == COND_OwnerOnly);
	TestTrue(TEXT("Saved charges remain a SaveGame field"), Charges->HasAnyPropertyFlags(CPF_SaveGame));

	FReplicationWorld F;
	auto* Player = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>() : nullptr;
	auto* Recovery = Player ? Player->GetFieldRecoveryComponent() : nullptr;
	if (!TestNotNull(TEXT("Field recovery component"), Recovery)) { return false; }
	TStrongObjectPtr<USovReplicationReadinessObserver> Observer(NewObject<USovReplicationReadinessObserver>());
	Recovery->OnChargesChanged.AddDynamic(Observer.Get(), &USovReplicationReadinessObserver::ObserveCharges);
	FSovReplicationReadinessTestAccess::SetCharges(Recovery, 1);
	FSovReplicationReadinessTestAccess::ReceiveCharges(Recovery);
	TestEqual(TEXT("A received count notifies the owning HUD once"), Observer->ChargeNotifications, 1);
	TestTrue(TEXT("The notification carries the received count and capacity"),
		Observer->LastCharges == 1 && Observer->LastCapacity == Recovery->Capacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationGrenadeProxyTest, "ProjectVelkorran.Replication.StickyGrenadeProxyLeavesCollisionToAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationGrenadeProxyTest::RunTest(const FString& Parameters)
{
	const auto* Authored = GetDefault<ASovCinderStickyGrenadeProjectile>();
	const auto* AuthoredSphere = Authored ? Cast<USphereComponent>(Authored->GetRootComponent()) : nullptr;
	if (!TestNotNull(TEXT("Authored grenade collision"), AuthoredSphere)) { return false; }
	TestTrue(TEXT("The authored grenade collides"), AuthoredSphere->GetCollisionEnabled() != ECollisionEnabled::NoCollision);

	FReplicationWorld F;
	auto* Grenade = F.World ? F.World->SpawnActorDeferred<ASovCinderStickyGrenadeProjectile>(ASovCinderStickyGrenadeProjectile::StaticClass(),
		FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
	if (!TestNotNull(TEXT("Proxy grenade"), Grenade)) { return false; }
	Grenade->SetRole(ROLE_SimulatedProxy);
	Grenade->FinishSpawning(FTransform::Identity);
	// A replicated projectile begins play as soon as it arrives on a client; this world has not started play.
	Grenade->DispatchBeginPlay();
	const auto* Sphere = Cast<USphereComponent>(Grenade->GetRootComponent());
	TestTrue(TEXT("A proxy grenade survives without an authoritative payload"), IsValid(Grenade) && !Grenade->IsActorBeingDestroyed());
	TestTrue(TEXT("A proxy grenade cannot block or stick on its own"), Sphere && Sphere->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationSeverChannelTest, "ProjectVelkorran.Replication.SeverCosmeticsUseUnreliableMulticast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationSeverChannelTest::RunTest(const FString& Parameters)
{
	const UFunction* Sever = USovDismembermentComponent::StaticClass()->FindFunctionByName(TEXT("MulticastPlaySever"));
	const FProperty* Mask = FindFProperty<FProperty>(USovDismembermentComponent::StaticClass(), TEXT("SeveredRegionMask"));
	if (!TestNotNull(TEXT("Sever multicast"), Sever) || !TestNotNull(TEXT("Severed region mask"), Mask)) { return false; }
	TestTrue(TEXT("Sever cosmetics reach every relevant client"), Sever->HasAnyFunctionFlags(FUNC_NetMulticast));
	TestFalse(TEXT("Sever cosmetics cannot saturate the reliable channel"), Sever->HasAnyFunctionFlags(FUNC_NetReliable));
	// Dropping the one-shot is safe only because the lasting state replicates and rebuilds visuals on arrival.
	TestTrue(TEXT("The severed state itself replicates with a notification"), Mask->HasAllPropertyFlags(CPF_Net | CPF_RepNotify));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationSettingsSeamTest, "ProjectVelkorran.Replication.GameplaySettingsResolvePerPlayerAndSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationSettingsSeamTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Player = F.World ? F.Spawn<ASovAxiomRuntimeTestCharacter>() : nullptr;
	auto* Enemy = F.World ? F.Spawn<ASovAxiomRuntimeTestCharacter>(FVector(500., 0., 0.)) : nullptr;
	auto* Remote = F.World ? F.World->SpawnActor<ASovRuntimeTestPlayerController>() : nullptr;
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Enemy"), Enemy) || !TestNotNull(TEXT("Remote controller"), Remote)) { return false; }
	// The single-player campaign resolves both scopes to this machine's settings.
	TestTrue(TEXT("Player-scoped settings are the local settings in single player"),
		UNarrativeGameUserSettings::GetSovPlayerSettings(Player) == UNarrativeGameUserSettings::GetSovSettings());
	TestTrue(TEXT("Session-scoped settings are the local settings in single player"),
		UNarrativeGameUserSettings::GetSovSessionSettings(F.World) == UNarrativeGameUserSettings::GetSovSettings());

	Player->InitializeTestCombat(0); Enemy->InitializeTestCombat(1);
	Enemy->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	Remote->Possess(Player);
	TestFalse(TEXT("A remote player's controller is not local where authority resolves payloads"), Remote->IsLocalController());
	AActor* Target = nullptr; FVector Point;
	TestTrue(TEXT("Projectile assistance acquires for a remote player's possessed shooter"),
		SovAimAssist::FindVisibleTarget(Player, FVector::ZeroVector, FVector::ForwardVector, 1000.f, 8.f, Target, Point) && Target == Enemy);
	Remote->UnPossess();
	TestFalse(TEXT("An unpossessed shooter still receives no assistance"),
		SovAimAssist::FindVisibleTarget(Player, FVector::ZeroVector, FVector::ForwardVector, 1000.f, 8.f, Target, Point));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationCombatantTest, "ProjectVelkorran.Replication.EncounterCombatantsNameThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationCombatantTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Director = F.World ? F.World->SpawnActor<ASovEncounterDirector>() : nullptr;
	auto* Player = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>() : nullptr;
	auto* Other = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>(FVector(300., 0., 0.)) : nullptr;
	if (!TestNotNull(TEXT("Director"), Director) || !TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Other"), Other)) { return false; }
	TArray<ASovPlayerCharacterBase*> Combatants;
	FSovReplicationReadinessTestAccess::ArmEncounter(Director, Player, false);
	Director->GetEncounterCombatants(Combatants);
	TestFalse(TEXT("No entry checkpoint means no combatant"), Director->IsEncounterCombatant(Player));
	TestEqual(TEXT("No entry checkpoint lists nobody"), Combatants.Num(), 0);

	FSovReplicationReadinessTestAccess::ArmEncounter(Director, Player, true);
	Director->GetEncounterCombatants(Combatants);
	TestTrue(TEXT("The encounter player is a combatant"), Director->IsEncounterCombatant(Player));
	TestFalse(TEXT("Another pawn is not"), Director->IsEncounterCombatant(Other));
	TestTrue(TEXT("The combatant list is exactly the encounter player"), Combatants.Num() == 1 && Combatants[0] == Player);
	TestTrue(TEXT("Single-player membership matches checkpoint ownership for both pawns"),
		Director->IsEncounterCombatant(Player) == Director->HasEncounterPlayer(Player)
		&& Director->IsEncounterCombatant(Other) == Director->HasEncounterPlayer(Other));
	TestFalse(TEXT("Null is never a combatant"), Director->IsEncounterCombatant(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReplicationResonanceLookupTest, "ProjectVelkorran.Replication.ResonanceResolvesFromTheActorInvolved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReplicationResonanceLookupTest::RunTest(const FString& Parameters)
{
	FReplicationWorld F;
	auto* Player = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>() : nullptr;
	auto* Uncontrolled = F.World ? F.Spawn<ASovExertionRuntimeTestCharacter>(FVector(300., 0., 0.)) : nullptr;
	auto* Controller = F.World ? F.World->SpawnActor<ASovHandoffRuntimeTestController>() : nullptr;
	auto* State = F.World ? F.World->SpawnActor<ASovPlayerState>() : nullptr;
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Uncontrolled"), Uncontrolled)
		|| !TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Player state"), State)) { return false; }
	TestNull(TEXT("With no possessed player there is no coordinator"), USovResonanceComponent::FindForActor(F.World, Uncontrolled));

	// A world that has begun play registers controllers itself; this one needs it done explicitly.
	F.World->AddController(Controller);
	// A PlayerState alone is not possession: the uncontrolled character shares it and must still not resolve itself.
	Uncontrolled->SetPlayerState(State);
	TestNull(TEXT("A PlayerState without possession is not a player"), USovResonanceComponent::FindForActor(F.World, Uncontrolled));
	Uncontrolled->SetPlayerState(nullptr);
	Controller->SetTestPlayerState(State);
	Controller->Possess(Player);
	const USovResonanceComponent* Own = Player->GetResonanceComponent();
	TestNotNull(TEXT("The player's coordinator exists"), Own);
	TestTrue(TEXT("The player is possessed by its controller"), Controller->GetPawn() == Player);
	TestTrue(TEXT("A player's pawn resolves its own coordinator"), USovResonanceComponent::FindForActor(F.World, Player) == Own);
	TestTrue(TEXT("An uncontrolled character resolves the single player's coordinator, not its own"),
		USovResonanceComponent::FindForActor(F.World, Uncontrolled) == Own);
	TestTrue(TEXT("No context resolves the single player's coordinator"), USovResonanceComponent::FindForActor(F.World, nullptr) == Own);
	TestTrue(TEXT("Every resolution matches the single-player lookup"), USovResonanceComponent::FindActive(F.World) == Own);
	TestNull(TEXT("A missing world resolves nothing"), USovResonanceComponent::FindForActor(nullptr, Player));
	Controller->UnPossess();
	return true;
}
#endif
