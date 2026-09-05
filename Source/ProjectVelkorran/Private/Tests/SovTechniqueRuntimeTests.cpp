// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovTechniqueRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "Character/PlayerDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Progression/SovTechniqueComponent.h"
#include "Progression/SovTechniqueRewardSource.h"
#include "Progression/SovTechniqueSafePoint.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

#if WITH_AUTOMATION_TESTS
struct FSovTechniqueTestAccess
{
	static void Configure(USovTechniqueComponent* Component)
	{
		Component->SkillTreeSkills = {
			NewObject<USovTechniqueTestSkillA>(Component), NewObject<USovTechniqueTestSkillB>(Component), NewObject<USovTechniqueTestSkillC>(Component)};
	}
	static UTreePerk* Perk(USovTechniqueComponent* Component) { return Component->GetPerk(USovTechniqueTestPerkA::StaticClass()); }
	static void CorruptPointBalance(USovTechniqueComponent* Component) { ++Component->SkillTreePoints; }
	static void CorruptAugmentSlot(USovTechniqueComponent* Component)
	{ Component->SelectedAugments.Add(FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderJudgement,USovTechniqueTestPerkB::StaticClass()); }
};
namespace
{
struct FTechniqueWorld
{
	UWorld* World = nullptr;
	ASovPlayerState* Player = nullptr;
	ASovHandoffRuntimeTestPawn* Pawn = nullptr;
	ASovHandoffRuntimeTestController* Controller = nullptr;
	USovTechniqueComponent* Techniques = nullptr;
	ASovTechniqueSafePoint* Safe = nullptr;
	ASovTechniqueTestEncounter* Encounter = nullptr;
	USovTechniqueRewardSource* Reward = nullptr;
	FTechniqueWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		Player = World->SpawnActor<ASovPlayerState>();
		Pawn = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
		Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
		Safe = World->SpawnActor<ASovTechniqueSafePoint>();
		Encounter = World->SpawnActor<ASovTechniqueTestEncounter>();
		if (!Player || !Pawn || !Controller || !Safe || !Encounter) { return; }
		auto* Definition=NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
		Pawn->PrepareCampaignInitialization(Definition); Controller->SetTestPlayerState(Player); Controller->Possess(Pawn);
		if (!Pawn->StageTestReadiness(Player,true) || !Pawn->CompleteCampaignDataInitialization(false)) { return; }
		Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(Player->GetAttributeSetBase());
		ASC->InitAbilityActorInfo(Player, Pawn);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
		Safe->SafePointId = TEXT("Test.Reflection");
		Encounter->EncounterId = TEXT("Test.Challenge");
		Encounter->SetTestState(ESovEncounterState::Succeeded);
		Reward = NewObject<USovTechniqueRewardSource>(Encounter);
		Encounter->AddInstanceComponent(Reward); Reward->RegisterComponent();
		Reward->RewardId = TEXT("Test.Challenge.Technique"); Reward->Points = 5;
		Reward->Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
		Reward->Proof = ESovTechniqueRewardProof::EncounterComplete; Reward->Encounter = Encounter;
		Techniques = Cast<USovTechniqueComponent>(Player->GetSkillTreeComponent());
		if (Techniques)
		{
			FSovTechniqueTestAccess::Configure(Techniques);
			Techniques->InitializeNewProtagonist(FSovGameplayTags::Get().Character_Player_Tarrik);
		}
	}
	~FTechniqueWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	float Resistance() const { return Player->GetAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetDamageResistanceAttribute()); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueRuntimeTest,
	"ProjectVelkorran.Campaign.Techniques.RewardsPurchaseRespecAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniqueRuntimeTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!TestNotNull(TEXT("Native Technique subobject"), Fixture.Techniques)) { return false; }
	TestEqual(TEXT("Pawn links to native PlayerState"), Fixture.Player->GetPawn(), static_cast<APawn*>(Fixture.Pawn));
	USkillTreeComponent* ExistingAPI = Fixture.Techniques;
	ExistingAPI->GiveSkillPoints(100);
	TestEqual(TEXT("Existing raw point API cannot mint campaign points"), Fixture.Techniques->GetAvailableTechniquePoints(), 0);
	TestTrue(TEXT("Completed authored challenge reward accepted"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	TestFalse(TEXT("Reward cannot be farmed"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	TestEqual(TEXT("Earned budget is tracked"), Fixture.Techniques->GetEarnedTechniquePoints(), 5);
	const TArray<UTreeSkill*> Branches = Fixture.Techniques->GetActiveTechniqueBranches();
	if (!TestEqual(TEXT("Three active branches"), Branches.Num(), 3)) { return false; }
	TestTrue(TEXT("Native safe-point purchase"), Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]));
	TestEqual(TEXT("Owned infinite modifier granted"), Fixture.Resistance(), 10.f);
	TestTrue(TEXT("Rank upgrade"), Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]));
	TestEqual(TEXT("Upgrade replaces the prior rank effect"), Fixture.Resistance(), 10.f);
	TestEqual(TEXT("Two ranks cost two points"), Fixture.Techniques->GetAvailableTechniquePoints(), 3);
	UTreePerk* Owned = FSovTechniqueTestAccess::Perk(Fixture.Techniques);
	if (!Owned) { return false; }
	Owned->SetPerkLevel(3);
	TestEqual(TEXT("Direct Blueprint perk call cannot bypass points"), Owned->PerkLevel, 1);
	FNarrativeSaveComponent Record;
	if (!TestTrue(TEXT("Narrative captures Technique save record"), USovEncounterSnapshotLibrary::CaptureComponent(Fixture.Techniques, Record))) { return false; }
	TestTrue(TEXT("First restore"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Techniques, Record));
	TestTrue(TEXT("Second restore"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Techniques, Record));
	TestTrue(TEXT("Restored ledger/perks valid"), Fixture.Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Repeated restore does not accumulate modifiers"), Fixture.Resistance(), 10.f);
	Fixture.Pawn->SetActorLocation(FVector(1000.f, 0.f, 0.f));
	TestFalse(TEXT("Caller cannot respec outside the physical safe point"), Fixture.Techniques->RespecAtSafePoint(Fixture.Safe));
	Fixture.Pawn->SetActorLocation(FVector::ZeroVector);
	Fixture.Encounter->SetTestState(ESovEncounterState::Active);
	TestFalse(TEXT("Active encounter blocks safe-point respec"), Fixture.Techniques->RespecAtSafePoint(Fixture.Safe));
	Fixture.Encounter->SetTestState(ESovEncounterState::Succeeded);
	TestTrue(TEXT("Free respec succeeds when safe"), Fixture.Techniques->RespecAtSafePoint(Fixture.Safe));
	TestEqual(TEXT("Respec removes owned perk modifiers"), Fixture.Resistance(), 0.f);
	TestEqual(TEXT("Respec returns exactly earned points"), Fixture.Techniques->GetAvailableTechniquePoints(), 5);
	TestFalse(TEXT("Respec does not reset reward claims"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueInvalidStateTest,
	"ProjectVelkorran.Campaign.Techniques.InvalidProofAndLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniqueInvalidStateTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!Fixture.Techniques) { return false; }
	Fixture.Encounter->SetTestState(ESovEncounterState::Active);
	TestFalse(TEXT("Unfinished encounter provides no reward proof"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	Fixture.Encounter->SetTestState(ESovEncounterState::Succeeded);
	Fixture.Reward->Protagonist = FSovGameplayTags::Get().Character_Player_Selene;
	TestFalse(TEXT("Wrong protagonist cannot claim reward"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	Fixture.Reward->Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
	TestTrue(TEXT("Valid reward still claimable"), Fixture.Techniques->ClaimReward(Fixture.Reward));
	Fixture.Techniques->PrepareForSave_Implementation();
	FSovTechniqueTestAccess::CorruptPointBalance(Fixture.Techniques);
	Fixture.Techniques->Load_Implementation();
	TestFalse(TEXT("Inconsistent saved point total fails closed"), Fixture.Techniques->IsTechniqueStateValid());
	TestFalse(TEXT("Invalid ledger cannot purchase/respec"), Fixture.Techniques->CanModifyTechniques());
	FNarrativeSaveComponent RejectedRecord;
	RejectedRecord.ComponentName = TEXT("LastValidSnapshot");
	TestFalse(TEXT("Known-invalid ledger cannot overwrite a checkpoint"), USovEncounterSnapshotLibrary::CaptureComponent(Fixture.Techniques, RejectedRecord));
	TestEqual(TEXT("Invalid capture preserves the caller's record"), RejectedRecord.ComponentName, FName(TEXT("LastValidSnapshot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniquePublishedSnapshotTest,
	"ProjectVelkorran.Campaign.Techniques.CommittedNotificationSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniquePublishedSnapshotTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!TestNotNull(TEXT("Native Technique subobject"), Fixture.Techniques)
		|| !TestTrue(TEXT("Claim initial points"), Fixture.Techniques->ClaimReward(Fixture.Reward))) { return false; }
	const TArray<UTreeSkill*> Branches = Fixture.Techniques->GetActiveTechniqueBranches();
	if (!TestEqual(TEXT("Configured branches"), Branches.Num(), 3)) { return false; }
	auto* Observer = NewObject<USovTechniqueSnapshotTestObserver>(Fixture.Techniques);
	Observer->Techniques = Fixture.Techniques;
	Fixture.Techniques->OnTechniquesChanged.AddDynamic(Observer, &USovTechniqueSnapshotTestObserver::OnChanged);
	const bool bPurchased = Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]);
	Fixture.Techniques->OnTechniquesChanged.RemoveDynamic(Observer, &USovTechniqueSnapshotTestObserver::OnChanged);
	if (!TestTrue(TEXT("Purchase commits"), bPurchased)) { return false; }
	TestEqual(TEXT("One committed notification"), Observer->Notifications, 1);
	TestEqual(TEXT("Listener sees the spent point"), Observer->AvailableAtNotification, 4);
	if (!TestTrue(TEXT("Synchronous notification can capture a complete record"), Observer->bCaptureSucceeded)) { return false; }
	TestTrue(TEXT("Respec changes the live state before loading captured record"), Fixture.Techniques->RespecAtSafePoint(Fixture.Safe));
	TestEqual(TEXT("Live state now has all five points"), Fixture.Techniques->GetAvailableTechniquePoints(), 5);
	if (!TestTrue(TEXT("Restore notification-time record"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Techniques, Observer->Record))) { return false; }
	TestTrue(TEXT("Restored points and purchased ranks form a valid ledger"), Fixture.Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Captured purchase retained its point cost"), Fixture.Techniques->GetAvailableTechniquePoints(), 4);
	UTreePerk* Restored = FSovTechniqueTestAccess::Perk(Fixture.Techniques);
	if (!TestNotNull(TEXT("Captured purchase retained its perk"), Restored)) { return false; }
	TestEqual(TEXT("Captured perk is rank one"), Restored->PerkLevel, 0);
	TestEqual(TEXT("Captured perk restores exactly one effect"), Fixture.Resistance(), 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueGrantCallbackOwnershipTest,
	"ProjectVelkorran.Campaign.Techniques.GrantCallbackSnapshotAndOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniqueGrantCallbackOwnershipTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!TestNotNull(TEXT("Native Technique subobject"), Fixture.Techniques)
		|| !TestTrue(TEXT("Claim initial points"), Fixture.Techniques->ClaimReward(Fixture.Reward))) { return false; }
	const TArray<UTreeSkill*> Branches = Fixture.Techniques->GetActiveTechniqueBranches();
	if (!TestEqual(TEXT("Configured branches"), Branches.Num(), 3)) { return false; }
	UAbilitySystemComponent* ASC = Fixture.Player->GetAbilitySystemComponent();
	UNarrativeSaveSubsystem* Save = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord LastCommittedActor;
	if (!TestNotNull(TEXT("Narrative save subsystem"), Save)
		|| !TestTrue(TEXT("Capture last committed player record"), Save->CreateActorRecord(Fixture.Player, LastCommittedActor))) { return false; }
	FNarrativeActorRecord DuringActorGrant = LastCommittedActor;
	bool bCallbackObserved = false;
	bool bUncommittedCaptureAccepted = false;
	bool bUncommittedActorCaptureAccepted = false;
	FNarrativeSaveComponent DuringGrant;
	DuringGrant.ComponentName = TEXT("UnchangedOnFailure");
	FGameplayAbilitySpecHandle UnrelatedHandle;
	FActiveGameplayEffectHandle UnrelatedEffectHandle;
	const FDelegateHandle Delegate = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
		{
			if (bCallbackObserved || !Spec.Def || !Spec.Def->IsA<USovTechniqueTestEffect>()) { return; }
			bCallbackObserved = true;
			bUncommittedCaptureAccepted = USovEncounterSnapshotLibrary::CaptureComponent(Fixture.Techniques, DuringGrant);
			bUncommittedActorCaptureAccepted = Save->CreateActorRecord(Fixture.Player, DuringActorGrant);
			// This could be an independent mission/weapon grant responding to the GE.
			UnrelatedHandle = TargetASC->GiveAbility(FGameplayAbilitySpec(USovTechniqueUnrelatedTestAbility::StaticClass(), 1));
			const FGameplayEffectSpecHandle IndependentSpec = TargetASC->MakeOutgoingSpec(
				USovTechniqueTestEffect::StaticClass(), 1.f, TargetASC->MakeEffectContext());
			if (IndependentSpec.IsValid()) { UnrelatedEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*IndependentSpec.Data.Get()); }
		});
	const bool bPurchased = Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]);
	ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Delegate);
	if (!TestTrue(TEXT("Purchase completes after a reentrant grant callback"), bPurchased)) { return false; }
	TestTrue(TEXT("Real GAS grant callback executed"), bCallbackObserved);
	TestFalse(TEXT("Mid-grant capture refuses a partially committed ledger"), bUncommittedCaptureAccepted);
	TestEqual(TEXT("Rejected capture leaves caller record unchanged"), DuringGrant.ComponentName, FName(TEXT("UnchangedOnFailure")));
	TestFalse(TEXT("Stock Narrative actor capture rejects mid-grant component errors"), bUncommittedActorCaptureAccepted);
	TestTrue(TEXT("Failed stock capture preserves last committed actor bytes"), DuringActorGrant.ByteData == LastCommittedActor.ByteData);
	TestEqual(TEXT("Failed stock capture preserves actor identity"), DuringActorGrant.ActorName, LastCommittedActor.ActorName);
	TestTrue(TEXT("Failed stock capture preserves stable identity"), DuringActorGrant.ActorGUID == LastCommittedActor.ActorGUID);
	TestTrue(TEXT("Failed stock capture preserves actor class"), DuringActorGrant.ActorSoftClass == LastCommittedActor.ActorSoftClass);
	TestTrue(TEXT("Failed stock capture preserves transform"), DuringActorGrant.Transform.Equals(LastCommittedActor.Transform));
	if (TestEqual(TEXT("Failed stock capture preserves component count"), DuringActorGrant.SavedComponents.Num(), LastCommittedActor.SavedComponents.Num()))
	{
		for (int32 Index = 0; Index < LastCommittedActor.SavedComponents.Num(); ++Index)
		{
			TestEqual(TEXT("Failed stock capture preserves component identity"), DuringActorGrant.SavedComponents[Index].ComponentName, LastCommittedActor.SavedComponents[Index].ComponentName);
			TestTrue(TEXT("Failed stock capture preserves committed component bytes"), DuringActorGrant.SavedComponents[Index].ByteData == LastCommittedActor.SavedComponents[Index].ByteData);
		}
	}
	TestNotNull(TEXT("Independent callback grant exists"), ASC->FindAbilitySpecFromHandle(UnrelatedHandle));
	TestNotNull(TEXT("Independent callback effect exists"), ASC->GetActiveGameplayEffect(UnrelatedEffectHandle));
	TestEqual(TEXT("Both owned and independent modifiers are active"), Fixture.Resistance(), 20.f);
	const FGameplayAbilitySpec* OwnedSpec = ASC->FindAbilitySpecFromClass(USovTechniqueOwnedTestAbility::StaticClass());
	if (!TestNotNull(TEXT("Perk's own ability exists"), OwnedSpec)) { return false; }
	const FGameplayAbilitySpecHandle OwnedHandle = OwnedSpec->Handle;
	TestTrue(TEXT("Respec succeeds"), Fixture.Techniques->RespecAtSafePoint(Fixture.Safe));
	TestNull(TEXT("Exact perk ability is removed"), ASC->FindAbilitySpecFromHandle(OwnedHandle));
	TestNotNull(TEXT("Unrelated reentrant grant survives respec"), ASC->FindAbilitySpecFromHandle(UnrelatedHandle));
	TestNotNull(TEXT("Unrelated reentrant effect survives respec"), ASC->GetActiveGameplayEffect(UnrelatedEffectHandle));
	TestEqual(TEXT("Only the perk's modifier is removed"), Fixture.Resistance(), 10.f);
	TestEqual(TEXT("Respec refunds the committed cost"), Fixture.Techniques->GetAvailableTechniquePoints(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueAvatarChangedDuringGrantTest,
	"ProjectVelkorran.Campaign.Techniques.AvatarChangedDuringGrant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniqueAvatarChangedDuringGrantTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!TestNotNull(TEXT("Native Technique subobject"), Fixture.Techniques)
		|| !TestTrue(TEXT("Claim initial points"), Fixture.Techniques->ClaimReward(Fixture.Reward))) { return false; }
	const TArray<UTreeSkill*> Branches = Fixture.Techniques->GetActiveTechniqueBranches();
	if (!TestEqual(TEXT("Configured branches"), Branches.Num(), 3)) { return false; }
	AActor* Replacement = Fixture.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Replacement avatar"), Replacement)) { return false; }
	UAbilitySystemComponent* ASC = Fixture.Player->GetAbilitySystemComponent();
	bool bCallbackObserved = false;
	const FDelegateHandle Delegate = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
		{
			if (bCallbackObserved || !Spec.Def || !Spec.Def->IsA<USovTechniqueTestEffect>()) { return; }
			bCallbackObserved = true;
			TargetASC->InitAbilityActorInfo(Fixture.Player, Replacement);
		});
	const bool bPurchased = Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]);
	ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Delegate);
	TestTrue(TEXT("Real GAS callback rebound the shared ASC"), bCallbackObserved);
	TestFalse(TEXT("Invalidated purchase reports failure"), bPurchased);
	TestFalse(TEXT("Invalidated ledger requires explicit restore"), Fixture.Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Rollback does not take the replacement avatar away"), ASC->GetAvatarActor(), Replacement);
	TestNull(TEXT("Subsequent perk ability never reaches replacement avatar"), ASC->FindAbilitySpecFromClass(USovTechniqueOwnedTestAbility::StaticClass()));
	TestEqual(TEXT("Returned effect handle is cleaned after source invalidation"), Fixture.Resistance(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueProfileInitializationTest,
	"ProjectVelkorran.Campaign.Techniques.ProfileInitializationOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTechniqueProfileInitializationTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld Fixture;
	if (!TestNotNull(TEXT("Native Technique subobject"), Fixture.Techniques)
		|| !TestTrue(TEXT("Claim initial points"), Fixture.Techniques->ClaimReward(Fixture.Reward))) { return false; }
	const TArray<UTreeSkill*> Branches = Fixture.Techniques->GetActiveTechniqueBranches();
	if (!TestEqual(TEXT("Configured branches"), Branches.Num(), 3)
		|| !TestTrue(TEXT("Purchase the outgoing profile's modifier"), Fixture.Techniques->BuyPerk(USovTechniqueTestPerkA::StaticClass(), Branches[0]))) { return false; }
	const auto& Tags = FSovGameplayTags::Get();
	TestFalse(TEXT("First-entry API cannot erase an existing active profile"), Fixture.Techniques->InitializeNewProtagonist(Tags.Character_Player_Tarrik));
	TestFalse(TEXT("Inactive identity cannot be initialized on the current avatar"), Fixture.Techniques->InitializeNewProtagonist(Tags.Character_Player_Selene));
	TestEqual(TEXT("Rejected initialization preserves point balance"), Fixture.Techniques->GetAvailableTechniquePoints(), 4);
	TestEqual(TEXT("Rejected initialization preserves active grants"), Fixture.Resistance(), 10.f);

	AActor* Replacement = Fixture.World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Replacement avatar"), Replacement)) { return false; }
	UAbilitySystemComponent* ASC = Fixture.Player->GetAbilitySystemComponent();
	ASC->RemoveLooseGameplayTag(Tags.Character_Player_Tarrik);
	ASC->AddLooseGameplayTag(Tags.Character_Player_Selene);
	bool bCallbackObserved = false;
	const FDelegateHandle Delegate = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[&](const FActiveGameplayEffect& Effect)
		{
			if (bCallbackObserved || !Effect.Spec.Def || !Effect.Spec.Def->IsA<USovTechniqueTestEffect>()) { return; }
			bCallbackObserved = true;
			ASC->InitAbilityActorInfo(Fixture.Player, Replacement);
		});
	const bool bInitialized = Fixture.Techniques->InitializeNewProtagonist(Tags.Character_Player_Selene);
	ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Delegate);
	TestTrue(TEXT("Real outgoing-effect removal callback changed ownership"), bCallbackObserved);
	TestFalse(TEXT("First-entry initialization stops after avatar replacement"), bInitialized);
	TestFalse(TEXT("Partially cleared outgoing state requires restore"), Fixture.Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Failed initialization preserves earned reward ledger"), Fixture.Techniques->GetEarnedTechniquePoints(), 5);
	TestEqual(TEXT("Failed initialization does not mint a fresh balance"), Fixture.Techniques->GetAvailableTechniquePoints(), 4);
	TestEqual(TEXT("Failed initialization leaves replacement ownership intact"), ASC->GetAvatarActor(), Replacement);
	TestEqual(TEXT("Removed outgoing modifier is not replayed on replacement"), Fixture.Resistance(), 0.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueAugmentSelectionTest,
	"ProjectVelkorran.Campaign.Techniques.AugmentSelectionOwnedGrantsAndRestore",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTechniqueAugmentSelectionTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld F; if (!F.Techniques) { return false; }
	const auto& T=FSovGameplayTags::Get(); const auto Slot=T.Ability_Echo_Tarrik_CinderSlam; FText Reason;
	const auto Branches=F.Techniques->GetActiveTechniqueBranches(); if (Branches.Num()!=3) { return false; }
	auto* ASC=F.Player->GetAbilitySystemComponent();
	const auto Core=ASC->GiveAbility(FGameplayAbilitySpec(USovTechniqueCoreTestAbility::StaticClass(),1));
	TestFalse(TEXT("An unpurchased option cannot be selected"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason));
	TestTrue(TEXT("Authored reward unlocks the point budget"),F.Techniques->ClaimReward(F.Reward));
	TestTrue(TEXT("First augment is purchased in the existing tree"),F.Techniques->BuyPerk(USovTechniqueTestPerkB::StaticClass(),Branches[0]));
	TestTrue(TEXT("Alternative augment is purchased in the existing tree"),F.Techniques->BuyPerk(USovTechniqueTestPerkD::StaticClass(),Branches[1]));
	TestEqual(TEXT("Unlocked options do not auto-equip or stack effects"),F.Resistance(),0.f);
	TestEqual(TEXT("The existing purchased ledger exposes both options"),F.Techniques->GetUnlockedAugments(Slot).Num(),2);
	TestTrue(TEXT("First selection activates its real owned effect"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason));
	TestEqual(TEXT("Exactly one augment modifier is active"),F.Resistance(),10.f);
	TestTrue(TEXT("Alternative selection replaces only this slot"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkD::StaticClass(),Reason));
	TestEqual(TEXT("Alternative does not accumulate a second modifier"),F.Resistance(),10.f);
	TestNotNull(TEXT("Core ability grant remains unchanged"),ASC->FindAbilitySpecFromHandle(Core));
	TestEqual(TEXT("Changing selection spends no Technique points"),F.Techniques->GetAvailableTechniquePoints(),3);
	FNarrativeSaveComponent Saved;
	TestTrue(TEXT("Selected augment is part of the existing component snapshot"),USovEncounterSnapshotLibrary::CaptureComponent(F.Techniques,Saved));
	TestTrue(TEXT("Repeated restore recreates the selected owned grant once"),USovEncounterSnapshotLibrary::RestoreComponent(F.Techniques,Saved));
	TestTrue(TEXT("Second restore remains valid"),USovEncounterSnapshotLibrary::RestoreComponent(F.Techniques,Saved));
	TestEqual(TEXT("Save replay does not accumulate effects"),F.Resistance(),10.f);
	TestTrue(TEXT("Saved selected class survives replay"),F.Techniques->GetSelectedAugment(Slot)&&F.Techniques->GetSelectedAugment(Slot)->IsA<USovTechniqueTestPerkD>());
	ASC->AddLooseGameplayTag(T.State_Weapon_VerityAbsent);
	TestFalse(TEXT("Unavailable equipment blocks changes"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason));
	ASC->RemoveLooseGameplayTag(T.State_Weapon_VerityAbsent);
	F.Pawn->SetTestVisualReady(false);
	TestFalse(TEXT("Pending pawn readiness blocks safe-point changes"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason));
	F.Pawn->SetTestVisualReady(true);
	F.Encounter->SetTestState(ESovEncounterState::Active);
	TestFalse(TEXT("Combat blocks augment selection"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason));
	F.Encounter->SetTestState(ESovEncounterState::Succeeded);
	const auto Independent=ASC->MakeOutgoingSpec(USovTechniqueTestEffect::StaticClass(),1,ASC->MakeEffectContext());
	const auto IndependentHandle=ASC->ApplyGameplayEffectSpecToSelf(*Independent.Data.Get());
	TestTrue(TEXT("Safe-point respec clears selected augmentation"),F.Techniques->RespecAtSafePoint(F.Safe));
	TestNull(TEXT("Selection clears with its purchased option"),F.Techniques->GetSelectedAugment(Slot));
	TestNotNull(TEXT("Independent effect survives tracked augment cleanup"),ASC->GetActiveGameplayEffect(IndependentHandle));
	TestEqual(TEXT("Only the independently owned modifier remains"),F.Resistance(),10.f);
	TestNotNull(TEXT("Respec never removes the core ability"),ASC->FindAbilitySpecFromHandle(Core));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueAugmentSavedSlotTest,
	"ProjectVelkorran.Campaign.Techniques.AugmentSavedSlotValidation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTechniqueAugmentSavedSlotTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld F; if (!F.Techniques) { return false; }
	const auto Branches=F.Techniques->GetActiveTechniqueBranches(); if (Branches.Num()!=3) { return false; }
	F.Techniques->ClaimReward(F.Reward); F.Techniques->BuyPerk(USovTechniqueTestPerkB::StaticClass(),Branches[0]);
	F.Techniques->PrepareForSave_Implementation(); FSovTechniqueTestAccess::CorruptAugmentSlot(F.Techniques); F.Techniques->Load_Implementation();
	TestFalse(TEXT("A purchased option cannot be loaded into a different ability slot"),F.Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Invalid load does not grant augment effects"),F.Resistance(),0.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTechniqueAugmentRollbackFailureTest,
	"ProjectVelkorran.Campaign.Techniques.AugmentRollbackFailureRequiresRecovery",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTechniqueAugmentRollbackFailureTest::RunTest(const FString& Parameters)
{
	FTechniqueWorld F; if (!F.Techniques) { return false; }
	const auto Branches=F.Techniques->GetActiveTechniqueBranches(); if (Branches.Num()!=3) { return false; }
	auto* ASC=F.Player->GetAbilitySystemComponent(); const auto Slot=FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderSlam;
	ASC->GiveAbility(FGameplayAbilitySpec(USovTechniqueCoreTestAbility::StaticClass(),1));
	F.Techniques->ClaimReward(F.Reward); F.Techniques->BuyPerk(USovTechniqueTestPerkB::StaticClass(),Branches[0]);
	F.Techniques->BuyPerk(USovTechniqueTestPerkD::StaticClass(),Branches[1]); FText Reason;
	if (!TestTrue(TEXT("Establish committed first selection"),F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkB::StaticClass(),Reason))) { return false; }
	const auto Independent=ASC->MakeOutgoingSpec(USovTechniqueTestEffect::StaticClass(),1,ASC->MakeEffectContext());
	const auto IndependentHandle=ASC->ApplyGameplayEffectSpecToSelf(*Independent.Data.Get());
	AActor* Replacement=F.World->SpawnActor<AActor>(); if (!Replacement) { return false; }
	int32 Stage=0;
	const auto Listener=ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent* Target,const FGameplayEffectSpec& Spec,FActiveGameplayEffectHandle)
		{
			const UObject* Source=Spec.GetContext().GetSourceObject();
			if (Stage==0 && Source && Source->IsA<USovTechniqueTestPerkD>())
			{ Stage=1; Target->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy); }
			else if (Stage==1 && Source && Source->IsA<USovTechniqueTestPerkB>())
			{ Stage=2; Target->InitAbilityActorInfo(F.Player,Replacement); }
		});
	const bool bChanged=F.Techniques->SelectAugmentAtSafePoint(F.Safe,Slot,USovTechniqueTestPerkD::StaticClass(),Reason);
	ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Listener);
	TestEqual(TEXT("Both real apply and rollback callbacks executed"),Stage,2);
	TestFalse(TEXT("A failed rollback does not report success"),bChanged);
	TestFalse(TEXT("Partial grant state requires an explicit checkpoint restore"),F.Techniques->IsTechniqueStateValid());
	TestFalse(TEXT("Failed recovery blocks further progression"),F.Techniques->CanModifyTechniques());
	TestTrue(TEXT("Failure feedback requests a checkpoint rather than claiming rollback succeeded"),Reason.ToString().Contains(TEXT("checkpoint")));
	TestEqual(TEXT("Both affected augment grant sets are cleaned"),F.Resistance(),10.f);
	TestNotNull(TEXT("Independent owner is preserved even on failed rollback"),ASC->GetActiveGameplayEffect(IndependentHandle));
	FNarrativeSaveComponent Rejected;
	TestFalse(TEXT("An invalid loadout cannot overwrite a checkpoint"),USovEncounterSnapshotLibrary::CaptureComponent(F.Techniques,Rejected));
	return true;
}
#endif
