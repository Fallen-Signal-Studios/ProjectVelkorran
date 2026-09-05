// Copyright Fallen Signal Studios. All Rights Reserved.
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "FieldRecovery/SovFieldRecoveryPolicy.h"
#include "FieldRecovery/SovGameplayAbility_FieldRecovery.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/Controller.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Progression/SovTechniqueSafePoint.h"
#include "Sovereign/SovGameplayTags.h"

USovFieldRecoveryComponent::USovFieldRecoveryComponent() { PrimaryComponentTick.bCanEverTick = false; }
void USovFieldRecoveryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner())) { InitializeWithAbilitySystem(Player->GetNarrativeAbilitySystemComponent()); }
}
void USovFieldRecoveryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	++StateEpoch;
	UseId.Invalidate(); UsingAbility.Reset(); ASC.Reset();
	Super::EndPlay(Reason);
}
bool USovFieldRecoveryComponent::ValidProfile() const
{
	return Capacity >= 1 && Capacity <= 10 && FMath::IsFinite(HealthFraction) && HealthFraction > 0.f && HealthFraction <= 1.f
		&& FMath::IsFinite(UseSeconds) && UseSeconds >= .1f && UseSeconds <= 5.f;
}
bool USovFieldRecoveryComponent::InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* AbilitySystem)
{
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	if (!Player || !IsValid(AbilitySystem) || AbilitySystem->GetAvatarActor() != Player
		|| !AbilitySystem->GetSet<UNarrativeAttributeSetBase>() || !ValidProfile()) { return false; }
	const FGameplayTag Identity = Player->GetProtagonistIdentityTag();
	if (Identity != FSovGameplayTags::Get().Character_Player_Tarrik && Identity != FSovGameplayTags::Get().Character_Player_Selene) { return false; }
	if (ASC.IsValid() && ASC.Get() != AbilitySystem && UseId.IsValid()) { return false; }
	if (ASC.Get() == AbilitySystem && IsInitialized()) { return true; }
	++StateEpoch;
	if (!bInitializedOnce)
	{ Charges = Capacity; SavedProtagonist = Identity; bInitializedOnce = true; }
	ASC = AbilitySystem;
	bStateValid = SavedSchemaVersion == 1 && SavedProtagonist == Identity && SovFieldRecoveryPolicy::ValidCharges(Charges, Capacity);
	return IsInitialized();
}
bool USovFieldRecoveryComponent::IsInitialized() const
{
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	return bStateValid && bInitializedOnce && ValidProfile() && ASC.IsValid() && Player
		&& ASC->GetAvatarActor() == Player && Player->GetAbilitySystemComponent() == ASC.Get()
		&& SavedProtagonist == Player->GetProtagonistIdentityTag() && SovFieldRecoveryPolicy::ValidCharges(Charges, Capacity);
}
bool USovFieldRecoveryComponent::HasLiveOwner() const
{
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	return IsInitialized() && Player->HasAuthority() && Player->IsCharacterReady() && Player->IsAlive()
		&& !Player->IsActorBeingDestroyed() && !ASC->IsDead()
		&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f;
}
bool USovFieldRecoveryComponent::CanUse() const
{
	return !bMutating && !UseId.IsValid() && HasLiveOwner()
		&& SovFieldRecoveryPolicy::CanBegin(Charges, Capacity, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),
			ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()), HealthFraction);
}
FGuid USovFieldRecoveryComponent::BeginUse(USovGameplayAbility_FieldRecovery* Ability)
{
	if (!CanUse() || !IsValid(Ability) || !Ability->IsActive() || Ability->GetAvatarActorFromActorInfo() != GetOwner() || !GetWorld()) { return {}; }
	TGuardValue<bool> Mutation(bMutating, true);
	UseId = FGuid::NewGuid(); UsingAbility = Ability; StartedAt = GetWorld()->GetTimeSeconds();
	CommittedDuration = UseSeconds; CommittedFraction = HealthFraction; bChargedAtStart = bConsumeOnStart;
	const FGuid Id = UseId;
	if (bChargedAtStart) { --Charges; OnChargesChanged.Broadcast(Charges, Capacity); }
	if (!IsCurrentUse(Ability, Id)) { CancelUse(Ability, Id); return {}; }
	return Id;
}
bool USovFieldRecoveryComponent::IsCurrentUse(const USovGameplayAbility_FieldRecovery* Ability, FGuid Id) const
{
	return Id.IsValid() && UseId == Id && UsingAbility.Get() == Ability && IsValid(Ability) && Ability->IsActive()
		&& Ability->GetAvatarActorFromActorInfo() == GetOwner() && HasLiveOwner();
}
void USovFieldRecoveryComponent::CancelUse(USovGameplayAbility_FieldRecovery* Ability, FGuid Id)
{
	if (UsingAbility.Get() == Ability && (UseId == Id || !Id.IsValid())) { UseId.Invalidate(); UsingAbility.Reset(); }
}
bool USovFieldRecoveryComponent::CompleteUse(USovGameplayAbility_FieldRecovery* Ability, FGuid Id, float& OutHealed)
{
	OutHealed = 0.f;
	if (bMutating || !IsCurrentUse(Ability, Id) || !GetWorld()
		|| !SovFieldRecoveryPolicy::Completed(GetWorld()->GetTimeSeconds(), StartedAt, CommittedDuration)) { return false; }
	const float Health = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	const float Maximum = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	const float Amount = static_cast<float>(SovFieldRecoveryPolicy::HealAmount(Health, Maximum, CommittedFraction));
	if (Amount <= 0.f || (!bChargedAtStart && Charges <= 0)) { CancelUse(Ability, Id); return false; }
	TGuardValue<bool> Mutation(bMutating, true);
	const auto HealingASC = ASC;
	const TWeakObjectPtr<ASovPlayerCharacterBase> HealingPlayer = Cast<ASovPlayerCharacterBase>(GetOwner());
	const TWeakObjectPtr<AController> HealingController = HealingPlayer->GetController();
	const uint64 HealingEpoch = StateEpoch;
	const int32 HealingReadyEpoch = HealingASC->GetCharacterReadyEpoch();
	if (!bChargedAtStart) { --Charges; }
	UseId.Invalidate(); UsingAbility.Reset(); // A nested completion can never spend or heal twice.
	Ability->bCompleted = true; Ability->CompletedHeal = Amount;
	HealingASC->ApplyModToAttributeUnsafe(UNarrativeAttributeSetBase::GetHealthAttribute(), EGameplayModOp::Additive, Amount);
	if (StateEpoch != HealingEpoch || !HasLiveOwner() || ASC != HealingASC || !HealingPlayer.IsValid()
		|| HealingASC->GetAvatarActor() != HealingPlayer.Get() || HealingPlayer->GetController() != HealingController.Get()
		|| HealingASC->GetCharacterReadyEpoch() != HealingReadyEpoch) { return false; }
	OutHealed = Amount;
	OnChargesChanged.Broadcast(Charges, Capacity);
	return true;
}
bool USovFieldRecoveryComponent::RefillAtSafePoint(ASovTechniqueSafePoint* Point, FString& Error)
{
	Error.Reset();
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	const auto* State = Player ? Player->GetPlayerState<ASovPlayerState>() : nullptr;
	if (bMutating || UseId.IsValid() || !HasLiveOwner() || !IsValid(Point) || !Point->bRefillsFieldRecovery
		|| !Point->AllowsModification(State))
	{ Error = TEXT("Field recovery refills require this protagonist to be at an available marked checkpoint or supply station."); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovFieldRecoveryRefill), false, Point);
	Query.AddIgnoredActor(Player);
	TArray<AActor*> Attached; Player->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetActorLocation(), Point->GetActorLocation(), ECC_Visibility, Query))
	{ Error = TEXT("The field recovery supply point is obstructed."); return false; }
	TGuardValue<bool> Mutation(bMutating, true);
	if (Charges != Capacity) { Charges = Capacity; OnChargesChanged.Broadcast(Charges, Capacity); }
	return true;
}
void USovFieldRecoveryComponent::PrepareForSave_Implementation() {}
void USovFieldRecoveryComponent::Load_Implementation()
{
	++StateEpoch;
	UseId.Invalidate(); UsingAbility.Reset();
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	bInitializedOnce = true;
	bStateValid = Player && SavedSchemaVersion == 1 && SavedProtagonist == Player->GetProtagonistIdentityTag()
		&& ValidProfile() && SovFieldRecoveryPolicy::ValidCharges(Charges, Capacity);
	if (bStateValid) { OnChargesChanged.Broadcast(Charges, Capacity); }
}
void USovFieldRecoveryComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaveGame() && Ar.IsSaving() && (bMutating || UseId.IsValid() || !IsInitialized())) { Ar.SetError(); return; }
	Super::Serialize(Ar);
	if (Ar.IsSaveGame() && Ar.IsLoading())
	{
		++StateEpoch;
		const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
		bInitializedOnce = true;
		bStateValid = Player && SavedSchemaVersion == 1 && ValidProfile() && SovFieldRecoveryPolicy::ValidCharges(Charges, Capacity)
			&& SavedProtagonist == Player->GetProtagonistIdentityTag();
		if (!bStateValid) { Ar.SetError(); }
	}
}
