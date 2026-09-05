// Copyright Fallen Signal Studios. All Rights Reserved.
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/SovCombatInputPolicy.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"

void UNarrativeAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	// Input callbacks may end and reactivate the same instanced ability with the same server prediction key.
	++InputActivationSerial;
	Super::NotifyAbilityActivated(Handle, Ability);
}

bool UNarrativeAbilitySystemComponent::IsCombatInputWindowValid() const
{
	const auto* Player = Cast<ANarrativePlayerCharacter>(GetAvatarActor());
	if (!CombatInputWindowId.IsValid() || !CombatInputOwner.IsValid() || !CombatInputAvatar.IsValid()
		|| CombatInputAvatar.Get() != GetAvatarActor() || CombatInputReadyEpoch != CharacterReadyEpoch
		|| !Player || !Player->IsCharacterReady() || !IsOwnerActorAuthoritative() || !GetWorld()
		|| GetWorld()->GetTimeSeconds() > CombatInputClosesAt) { return false; }
	const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
	if (HasMatchingGameplayTag(N.State_IsDead) || HasMatchingGameplayTag(N.State_Interacting)
		|| HasMatchingGameplayTag(N.State_SequencerControlled) || HasMatchingGameplayTag(N.State_Movement_Ragdoll)
		|| HasMatchingGameplayTag(S.State_Fatal) || HasMatchingGameplayTag(S.State_Poise_Broken)
		|| HasMatchingGameplayTag(S.State_Status_Frozen)
		|| GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f) { return false; }
	FGuid CurrentId;
	if (!CombatInputOwner->GetSovAttackIdentity(GetAvatarActor(), CurrentId) || CurrentId != CombatInputAttackId) { return false; }
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(CombatInputSpec);
	return Spec && Spec->IsActive() && Spec->GetAbilityInstances().Contains(CombatInputOwner.Get());
}

FGuid UNarrativeAbilitySystemComponent::RegisterCombatInputWindow(UNarrativeCombatAbility* Ability,
	const FGameplayTagContainer& AllowedInputs, float OpensAfter, float ClosesAfter)
{
	if (!SovCombatInputPolicy::ValidWindow(OpensAfter, ClosesAfter) || AllowedInputs.IsEmpty()
		|| !IsValid(Ability) || !GetWorld() || !IsOwnerActorAuthoritative()) { return {}; }
	FGuid AttackId;
	if (!Ability->GetSovAttackIdentity(GetAvatarActor(), AttackId)) { return {}; }
	if (IsCombatInputWindowValid()) { return {}; } // Cannot replace a live node's scope or discard its pending press.
	ClearCombatInputBuffer();
	FGameplayAbilitySpecHandle OwnerSpec;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.GetAbilityInstances().Contains(Ability)) { OwnerSpec = Spec.Handle; break; }
	}
	if (!OwnerSpec.IsValid()) { return {}; }
	const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Input"), false);
	for (const FGameplayTag& Input : AllowedInputs)
	{
		if (!InputRoot.IsValid() || !Input.MatchesTag(InputRoot)
			|| Input == FNarrativeGameplayTags::Get().Narrative_Input_Confirm
			|| Input == FNarrativeGameplayTags::Get().Narrative_Input_Cancel) { return {}; }
	}
	CombatInputOwner = Ability; CombatInputAvatar = GetAvatarActor(); CombatInputSpec = OwnerSpec;
	CombatInputAttackId = AttackId; CombatInputWindowId = FGuid::NewGuid();
	CombatInputAllowedTags = AllowedInputs;
	CombatInputReadyEpoch = CharacterReadyEpoch;
	CombatInputOpensAt = GetWorld()->GetTimeSeconds() + OpensAfter;
	CombatInputClosesAt = GetWorld()->GetTimeSeconds() + ClosesAfter;
	if (!IsCombatInputWindowValid()) { ClearCombatInputBuffer(); return {}; }
	return CombatInputWindowId;
}

bool UNarrativeAbilitySystemComponent::BufferCombatInput(const FGameplayTag& InputTag)
{
	if (!IsCombatInputWindowValid()) { ClearCombatInputBuffer(); return false; }
	if (!CombatInputAllowedTags.HasTagExact(InputTag)) { return false; }
	BufferedCombatInput = InputTag;
	CombatInputPressedAt = GetWorld()->GetTimeSeconds();
	bBufferedCombatInputHeld = true;
	return true;
}

bool UNarrativeAbilitySystemComponent::ConsumeCombatInputWindow(UNarrativeCombatAbility* Ability, FGuid WindowId,
	FGameplayTag& OutInput, bool& bOutStillHeld)
{
	OutInput = FGameplayTag(); bOutStillHeld = false;
	if (CombatInputOwner.Get() != Ability || CombatInputWindowId != WindowId || !WindowId.IsValid()) { return false; }
	if (!IsCombatInputWindowValid()) { ClearCombatInputBuffer(); return false; }
	if (!BufferedCombatInput.IsValid()) { return false; }
	const auto* Settings = UNarrativeGameUserSettings::GetSovSettings();
	const float Assist = Settings ? Settings->GetInputBufferAssistanceSeconds() : 0.f;
	const double Lifetime = 0.22 + (FMath::IsFinite(Assist) ? FMath::Clamp(Assist, 0.f, 0.2f) : 0.f);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - CombatInputPressedAt > Lifetime) { BufferedCombatInput = FGameplayTag(); bBufferedCombatInputHeld = false; return false; }
	if (!SovCombatInputPolicy::CanConsume(Now, CombatInputPressedAt, CombatInputOpensAt, CombatInputClosesAt, Lifetime)) { return false; }
	OutInput = BufferedCombatInput; bOutStillHeld = bBufferedCombatInputHeld;
	// Clear before returning to callers that may broadcast, cancel or advance a node.
	BufferedCombatInput = FGameplayTag(); bBufferedCombatInputHeld = false;
	return true;
}

void UNarrativeAbilitySystemComponent::ClearCombatInputWindow(UNarrativeCombatAbility* Ability, FGuid WindowId)
{
	if (CombatInputOwner.Get() == Ability && CombatInputWindowId == WindowId) { ClearCombatInputBuffer(); }
}

void UNarrativeAbilitySystemComponent::ClearCombatInputBuffer()
{
	CombatInputOwner.Reset(); CombatInputAvatar.Reset(); CombatInputSpec = FGameplayAbilitySpecHandle();
	CombatInputAttackId.Invalidate(); CombatInputWindowId.Invalidate(); CombatInputAllowedTags.Reset();
	BufferedCombatInput = FGameplayTag(); bBufferedCombatInputHeld = false;
	CombatInputOpensAt = 0.; CombatInputClosesAt = 0.; CombatInputPressedAt = 0.; CombatInputReadyEpoch = 0;
}
