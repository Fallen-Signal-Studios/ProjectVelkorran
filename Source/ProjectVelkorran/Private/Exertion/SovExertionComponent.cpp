// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Exertion/SovExertionComponent.h"
#include "Exertion/SovExertionPolicy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeCombatAbility.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

namespace
{
	float CostScale(const AActor* Player)
	{
		const auto* Settings = UNarrativeGameUserSettings::GetSovPlayerSettings(Player);
		const float Scale = Settings ? Settings->GetExertionCostScale() : 1.f;
		return FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.1f, 1.f) : 1.f;
	}
	bool BlocksMovement(const UAbilitySystemComponent* ASC)
	{
		if (!ASC) { return true; }
		const auto& N = FNarrativeGameplayTags::Get();
		const auto& S = FSovGameplayTags::Get();
		return ASC->HasMatchingGameplayTag(N.State_IsDead) || ASC->HasMatchingGameplayTag(N.State_Busy)
			|| ASC->HasMatchingGameplayTag(N.State_SequencerControlled) || ASC->HasMatchingGameplayTag(N.State_Interacting)
			|| ASC->HasMatchingGameplayTag(N.State_Movement_Ragdoll) || ASC->HasMatchingGameplayTag(S.State_Fatal)
			|| ASC->HasMatchingGameplayTag(S.State_Status_Frozen) || ASC->HasMatchingGameplayTag(S.State_Poise_Broken);
	}
}

bool FSovExertionProfile::IsValid() const
{
	const float Positive[] = { MaximumStamina, IdleRegenRate, EvadeDistance, EvadeDuration,
		EvadeInvulnerability, WalkSpeed, RunSpeed, SprintSpeed };
	for (const float Value : Positive) { if (!FMath::IsFinite(Value) || Value <= 0.f) { return false; } }
	return FMath::IsFinite(RegenDelay) && RegenDelay >= 0.f
		&& FMath::IsFinite(ActiveRegenScale) && ActiveRegenScale >= 0.f && ActiveRegenScale <= 1.f
		&& FMath::IsFinite(CombatSprintDrain) && CombatSprintDrain > 0.f
		&& FMath::IsFinite(EvadeCost) && EvadeCost >= 0.f
		&& EvadeInvulnerability <= EvadeDuration && WalkSpeed <= RunSpeed && RunSpeed <= SprintSpeed;
}

USovExertionComponent::USovExertionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);
	SeleneProfile.MaximumStamina = 100.f;
	SeleneProfile.IdleRegenRate = 38.f;
	SeleneProfile.RegenDelay = 0.55f;
	SeleneProfile.CombatSprintDrain = 14.f;
	SeleneProfile.EvadeCost = 20.f;
	SeleneProfile.EvadeDistance = 420.f;
	SeleneProfile.EvadeDuration = 0.42f;
	SeleneProfile.EvadeInvulnerability = 0.27f;
	SeleneProfile.WalkSpeed = 225.f;
	SeleneProfile.RunSpeed = 600.f;
	SeleneProfile.SprintSpeed = 820.f;
}

void USovExertionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Character = Cast<ASovPlayerCharacterBase>(GetOwner()))
	{
		InitializeWithAbilitySystem(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character));
		if (auto* Movement = Character->GetCharacterMovement()) { Movement->AddTickPrerequisiteComponent(this); }
	}
}

void USovExertionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Uninitialize();
	Super::EndPlay(Reason);
}

bool USovExertionComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* AbilitySystem)
{
	if (!IsValid(AbilitySystem) || !Cast<ASovPlayerCharacterBase>(GetOwner())
		|| AbilitySystem->GetAvatarActor() != GetOwner() || !AbilitySystem->GetSet<UNarrativeAttributeSetBase>()) { return false; }
	if (ASC == AbilitySystem && StaminaChangedHandle.IsValid()) { return true; }
	Uninitialize();
	ASC = AbilitySystem;
	const FSovExertionProfile Profile = GetProfile();
	if (!Profile.IsValid()) { Uninitialize(); return false; }
	if (GetOwner()->HasAuthority() && bApplyPrototypeDefaults)
	{
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), FMath::Max(Profile.MaximumStamina, 0.f));
		if (!IsValid(ASC) || ASC->GetAvatarActor() != GetOwner()) { Uninitialize(); return false; }
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute(), FMath::Max(Profile.IdleRegenRate, 0.f));
		if (!IsValid(ASC) || ASC->GetAvatarActor() != GetOwner()) { Uninitialize(); return false; }
	}
	// Movement speeds are not replicated. An owning client predicts with them, so every role applies the profile;
	// only the attribute bases above are authoritative writes.
	if (bApplyPrototypeDefaults)
	{
		if (auto* Character = Cast<ACharacter>(GetOwner()))
		{
			if (auto* Movement = Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()))
			{
				Movement->SlowWalkSpeed = Profile.WalkSpeed;
				Movement->MaxWalkSpeed = Profile.RunSpeed;
				Movement->SprintSpeed = Profile.SprintSpeed;
			}
		}
	}
	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	RemainingRegenDelay = Profile.RegenDelay;
	RefreshExhaustionTag();
	return IsInitialized();
}

void USovExertionComponent::Uninitialize()
{
	UAbilitySystemComponent* OldASC = ASC;
	ASC = nullptr;
	if (IsValid(OldASC))
	{
		OldASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		if (bOwnsExhaustedTag)
		{ OldASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Exertion_Exhausted, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	}
	bOwnsExhaustedTag = false;
	StaminaChangedHandle.Reset();
	RemainingRegenDelay = 0.f;
}

bool USovExertionComponent::IsInitialized() const
{
	return IsValid(ASC) && ASC->GetAvatarActor() == GetOwner() && StaminaChangedHandle.IsValid();
}

float USovExertionComponent::GetStamina() const
{
	return IsInitialized() ? ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()) : 0.f;
}

FSovExertionProfile USovExertionComponent::GetProfile() const
{
	const auto* Character = Cast<ASovPlayerCharacterBase>(GetOwner());
	return Character && Character->GetProtagonistIdentityTag() == FSovGameplayTags::Get().Character_Player_Selene
		? SeleneProfile : TarrikProfile;
}

bool USovExertionComponent::CanMutate() const
{
	const auto* Character = Cast<ASovPlayerCharacterBase>(GetOwner());
	return IsInitialized() && Character && Character->HasAuthority() && Character->IsCharacterReady()
		&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}

bool USovExertionComponent::CanSpendExertion(float Cost) const
{
	return !bChangingResource && CanMutate() && SovExertionPolicy::ValidCost(Cost)
		&& SovExertionPolicy::CanPay(GetStamina(), Cost * CostScale(GetOwner()));
}

bool USovExertionComponent::TrySpendExertion(float Cost)
{
	if (!CanSpendExertion(Cost)) { return false; }
	return TrySpendScaledCost(Cost * CostScale(GetOwner()));
}

bool USovExertionComponent::TrySpendScaledCost(float PaidCost)
{
	if (bChangingResource || !CanMutate() || !SovExertionPolicy::CanPay(GetStamina(), PaidCost)) { return false; }
	if (PaidCost == 0.f) { return true; }
	TGuardValue<bool> Mutation(bChangingResource, true);
	TWeakObjectPtr<UAbilitySystemComponent> PaidASC = ASC;
	RemainingRegenDelay = FMath::Max(GetProfile().RegenDelay, 0.f);
	ASC->ApplyModToAttributeUnsafe(UNarrativeAttributeSetBase::GetStaminaAttribute(), EGameplayModOp::Additive, -PaidCost);
	if (!CanMutate() || ASC != PaidASC.Get()) { return false; }
	OnStaminaSpent.Broadcast(PaidCost, GetStamina());
	return CanMutate() && ASC == PaidASC.Get();
}

bool USovExertionComponent::IsCombatActive() const
{
	const auto* Character = Cast<ASovPlayerCharacterBase>(GetOwner());
	const auto* Echo = Character ? Character->GetEchoComponent() : nullptr;
	return Echo && Echo->IsEncounterActive();
}

bool USovExertionComponent::CanSprint() const
{
	return CanMutate() && !BlocksMovement(ASC) && (!IsCombatActive() || !IsExhausted());
}

void USovExertionComponent::ResetForCheckpoint()
{
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return; }
	RemainingRegenDelay = FMath::Max(GetProfile().RegenDelay, 0.f);
	if (auto* Character = Cast<ACharacter>(GetOwner()))
	{
		if (auto* Movement = Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement())) { Movement->StopSprinting(); }
	}
	RefreshExhaustionTag();
}

bool USovExertionComponent::HasCompetingRegenEffect() const
{
	if (!ASC) { return false; }
	for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (!Effect || Effect->Spec.GetPeriod() <= 0.f || !Effect->Spec.Def) { continue; }
		for (const FGameplayModifierInfo& Modifier : Effect->Spec.Def->Modifiers)
		{
			if (Modifier.Attribute == UNarrativeAttributeSetBase::GetStaminaAttribute()) { return true; }
		}
	}
	return false;
}

void USovExertionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	UpdateExertion(DeltaTime);
}

void USovExertionComponent::UpdateExertion(float DeltaTime)
{
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f || bChangingResource) { return; }
	auto* Character = Cast<ACharacter>(GetOwner());
	auto* Movement = Character ? Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()) : nullptr;
	if (!CanMutate()) { if (Movement) { Movement->StopSprinting(); } return; }
	if (Movement && !CanSprint()) { Movement->StopSprinting(); }
	const bool bSprinting = Movement && Movement->bWantsSprint && Movement->IsMovingOnGround()
		&& !Movement->IsCrouching() && (!Movement->Velocity.IsNearlyZero() || !Character->GetLastMovementInputVector().IsNearlyZero());
	const FSovExertionProfile Profile = GetProfile();
	const float Drain = static_cast<float>(SovExertionPolicy::SprintDrain(GetStamina(), Profile.CombatSprintDrain * CostScale(GetOwner()),
		DeltaTime, IsCombatActive(), bSprinting));
	if (Drain > 0.f)
	{
		// Continuous drain may consume the final fraction. Discrete actions require their entire cost.
		TrySpendScaledCost(Drain);
		if (Movement && IsExhausted()) { Movement->StopSprinting(); }
		return;
	}
	bool bAttacking = false;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Cast<UNarrativeCombatAbility>(Spec.Ability)) { bAttacking = true; break; }
	}
	const auto& Tags = FSovGameplayTags::Get();
	const bool bExerting = bAttacking || bSprinting || ASC->HasMatchingGameplayTag(Tags.State_Guarding)
		|| ASC->HasMatchingGameplayTag(Tags.State_Deflecting) || ASC->HasMatchingGameplayTag(Tags.State_Evading);
	const float Current = GetStamina();
	const float Maximum = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxStaminaAttribute());
	const float Rate = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute());
	const float NewStamina = static_cast<float>(SovExertionPolicy::FrameRegen(Current, Maximum, Rate,
		DeltaTime, RemainingRegenDelay, bExerting, Profile.ActiveRegenScale));
	RemainingRegenDelay = FMath::Max(0.f, RemainingRegenDelay - DeltaTime);
	if (NewStamina > Current && !HasCompetingRegenEffect())
	{
		TGuardValue<bool> Mutation(bChangingResource, true);
		ASC->ApplyModToAttributeUnsafe(UNarrativeAttributeSetBase::GetStaminaAttribute(), EGameplayModOp::Additive, NewStamina - Current);
	}
}

void USovExertionComponent::RefreshExhaustionTag()
{
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return; }
	const bool bExhausted = IsExhausted();
	if (bExhausted == bOwnsExhaustedTag) { return; }
	bOwnsExhaustedTag = bExhausted;
	// Authored HUD, animation and Blueprint readers observe Exhausted on the owning client and simulated proxies.
	const FGameplayTag Exhausted = FSovGameplayTags::Get().State_Exertion_Exhausted;
	if (bExhausted) { ASC->AddLooseGameplayTag(Exhausted, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	else { ASC->RemoveLooseGameplayTag(Exhausted, 1, EGameplayTagReplicationState::TagAndCountToAll); }
}

void USovExertionComponent::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue < Data.OldValue) { RemainingRegenDelay = FMath::Max(GetProfile().RegenDelay, 0.f); }
	RefreshExhaustionTag();
	OnStaminaChanged.Broadcast(Data.OldValue, Data.NewValue, Data.NewValue <= KINDA_SMALL_NUMBER);
}
