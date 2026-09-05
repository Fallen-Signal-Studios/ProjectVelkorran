// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Weapons/WeaponVisual.h"

USovGameplayAbility_SeleneDispatch::USovGameplayAbility_SeleneDispatch()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	ReturningVerityClass = ASovSeleneCombatProjectile::StaticClass();
	OutboundDamageEffectClass = ReturnDamageEffectClass = USovGameplayEffect_SeleneDamage::StaticClass();
	FrozenShatterEffectClass = USovGameplayEffect_SeleneDamage::StaticClass();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_Dispatch;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_Dispatch;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	// Dispatch is Selene's signature and summons Verity even while a firearm is active.
	bRequiresAllowedWeapon = false;
	MaximumActiveDuration = 7.0f;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "DispatchName", "Dispatch");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"DispatchDescription",
		"Cast Verity through a steerable arc, then recall it on command or at the outbound limit; enemies can be struck once on each leg of the path.");
}

bool USovGameplayAbility_SeleneDispatch::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(MaximumOutboundDuration) && MaximumOutboundDuration > 0.0f
		&& FMath::IsFinite(MaximumOutboundDistance) && MaximumOutboundDistance > 0.0f && MaximumOutboundDistance <= 10000.0f
		&& FMath::IsFinite(OutboundSpeed) && OutboundSpeed > 0.0f && OutboundSpeed <= 20000.0f
		&& FMath::IsFinite(ReturnSpeed) && ReturnSpeed > 0.0f && ReturnSpeed <= 20000.0f
		&& FMath::IsFinite(MaximumSteeringDegreesPerSecond) && MaximumSteeringDegreesPerSecond >= 0.0f
		&& MaximumSteeringDegreesPerSecond <= 720.0f
		&& FMath::IsFinite(DamagePerLeg) && DamagePerLeg > 0.0f
		&& FMath::IsFinite(PoiseDamagePerLeg) && PoiseDamagePerLeg >= 0.0f
		&& FMath::IsFinite(ShatterBonusPoise) && ShatterBonusPoise >= 0.0f
		&& FMath::IsFinite(MaximumActiveDuration) && MaximumActiveDuration > MaximumOutboundDuration;
}

void USovGameplayAbility_SeleneDispatch::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint32 Epoch = NativePayloadEpoch + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (NativePayloadEpoch != Epoch || !IsActive() || !CurrentActorInfo) { return; }
	DispatchTaskEpoch = Epoch;
	bDispatchRecallPending = false;
	// GAS replicates the subsequent press. A held activation input must not immediately recall.
	RecallInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (RecallInputTask)
	{
		RecallInputTask->OnPress.AddDynamic(this, &ThisClass::HandleDispatchRecallPressed);
		RecallInputTask->ReadyForActivation();
	}
	if (NativePayloadEpoch != Epoch || !IsActive() || !CurrentActorInfo->IsNetAuthority()) { return; }
	if (!ContinueNativePayload(Epoch)) { return; }
	FSovSeleneProjectileParameters Parameters;
	Parameters.Context = MakeNativePayloadContext();
	DispatchSourceASC = Parameters.Context.SourceASC;
	if (const auto* Character = Cast<ANarrativeCharacter>(Parameters.Context.SourceAvatar.Get()))
	{
		DispatchMainWeapon = Character->GetWeapon(true);
		DispatchOffWeapon = Character->GetWeapon(false);
	}
	FVector Origin, Direction;
	if (!SovSelenePayload::Aim(Parameters.Context, Origin, Direction)) { FinishEchoAbility(true); return; }
	Parameters.Mode = ESovSeleneProjectileMode::Dispatch;
	Parameters.Direction = Direction;
	Parameters.Damage = DamagePerLeg;
	Parameters.Poise = PoiseDamagePerLeg;
	Parameters.ShatterBonusPoise = ShatterBonusPoise;
	Parameters.Range = MaximumOutboundDistance;
	Parameters.Speed = OutboundSpeed;
	Parameters.ReturnSpeed = ReturnSpeed;
	Parameters.OutboundDuration = MaximumOutboundDuration;
	Parameters.SteeringDegrees = MaximumSteeringDegreesPerSecond;
	Parameters.MaximumLifetime = MaximumActiveDuration;
	auto* Projectile = ASovSeleneCombatProjectile::SpawnNativePayload(ReturningVerityClass, Origin, Parameters);
	if (!ContinueNativePayload(Epoch)) { if (IsValid(Projectile)) { Projectile->Destroy(); } return; }
	if (!Projectile) { FinishEchoAbility(true); return; }
	ActiveDispatch = Projectile;
	Projectile->OnPayloadFinished.AddDynamic(this, &ThisClass::HandleDispatchFinished);
	// Independently owned finite tag: cleanup removes exactly this handle, never somebody else's absence.
	UAbilitySystemComponent* ASC = DispatchSourceASC.Get();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_SeleneControl::StaticClass(),
		GetAbilityLevel(), ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent);
		Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, MaximumActiveDuration);
		Spec.Data->SetDuration(MaximumActiveDuration, true);
		// Applying the tag can synchronously cancel this activation. Retain and remove the returned handle safely.
		const FActiveGameplayEffectHandle Applied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (NativePayloadEpoch != Epoch || !IsActive())
		{
			if (IsValid(ASC) && Applied.IsValid()) { ASC->RemoveActiveGameplayEffect(Applied); }
			return;
		}
		VerityAbsentEffect = Applied;
	}
	if (!ContinueNativePayload(Epoch)) { return; }
	UpdateDispatch(Epoch);
	if (!ContinueNativePayload(Epoch)) { return; }
	GetWorld()->GetTimerManager().SetTimer(DispatchWatchdog, FTimerDelegate::CreateWeakLambda(this,
		[this, Epoch]() { UpdateDispatch(Epoch); }), 0.05f, true);
	if (bDispatchRecallPending)
	{
		bDispatchRecallPending = false;
		Projectile->Recall();
		if (!ContinueNativePayload(Epoch)) { return; }
	}
	ReceiveNativeSelenePayloadReleased(Projectile, Origin, Direction);
}
void USovGameplayAbility_SeleneDispatch::HandleDispatchRecallPressed(float TimeWaited)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !ContinueNativePayload(DispatchTaskEpoch)) { return; }
	if (IsValid(ActiveDispatch)) { ActiveDispatch->Recall(); }
	else { bDispatchRecallPending = true; }
}
void USovGameplayAbility_SeleneDispatch::HandleDispatchFinished(ASovSeleneCombatProjectile* Projectile, bool bReturned)
{
	if (Projectile == ActiveDispatch && NativePayloadEpoch == DispatchTaskEpoch && IsActive())
	{
		FinishEchoAbility(!bReturned);
	}
}
void USovGameplayAbility_SeleneDispatch::UpdateDispatch(uint32 Epoch)
{
	if (!ContinueNativePayload(Epoch)) { return; }
	auto* Character = Cast<ANarrativeCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(ActiveDispatch) || ActiveDispatch->IsActorBeingDestroyed() || !Character
		|| Character->GetWeapon(true) != DispatchMainWeapon.Get() || Character->GetWeapon(false) != DispatchOffWeapon.Get())
	{
		FinishEchoAbility(true); return;
	}
	TArray<AActor*> Attached;
	Character->GetAttachedActors(Attached, true, true);
	if (auto* Visual = Character->GetCharacterVisual())
	{
		TArray<AActor*> VisualAttachments;
		Visual->GetAttachedActors(VisualAttachments, true, true);
		Attached.Append(VisualAttachments);
	}
	for (AActor* Actor : Attached)
	{
		auto* WeaponVisual = Cast<AWeaponVisual>(Actor);
		if (!WeaponVisual || WeaponVisual->GetNarrativeCharacter() != Character
			|| !IsValid(WeaponVisual->WeaponOwner) || HiddenVerityVisuals.Contains(WeaponVisual)) { continue; }
		const bool bVerity = VerityWeaponClasses.ContainsByPredicate([WeaponVisual](const TSubclassOf<UWeaponItem>& Class)
		{
			return Class && WeaponVisual->WeaponOwner->IsA(Class);
		});
		if (!bVerity) { continue; }
		HiddenVerityVisuals.Add(WeaponVisual, WeaponVisual->IsHidden());
		WeaponVisual->SetActorHiddenInGame(true);
	}
}
void USovGameplayAbility_SeleneDispatch::RestoreDispatchPresentation()
{
	for (const auto& Pair : HiddenVerityVisuals)
	{
		if (Pair.Key.IsValid() && Pair.Key->GetNarrativeCharacter() == GetAvatarActorFromActorInfo())
		{
			Pair.Key->SetActorHiddenInGame(Pair.Value);
		}
	}
	HiddenVerityVisuals.Empty();
	if (DispatchSourceASC.IsValid() && VerityAbsentEffect.IsValid())
	{
		DispatchSourceASC->RemoveActiveGameplayEffect(VerityAbsentEffect);
	}
	VerityAbsentEffect.Invalidate();
	DispatchSourceASC.Reset();
}
void USovGameplayAbility_SeleneDispatch::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEndingDispatch || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	if (ScopeLockCount > 0)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	TGuardValue<bool> Ending(bEndingDispatch, true);
	// Invalidate callbacks and detach delegates before removal/destruction can broadcast synchronously.
	DispatchTaskEpoch = 0;
	bDispatchRecallPending = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DispatchWatchdog); }
	if (RecallInputTask)
	{
		RecallInputTask->OnPress.RemoveDynamic(this, &ThisClass::HandleDispatchRecallPressed);
		RecallInputTask->EndTask();
		RecallInputTask = nullptr;
	}
	if (IsValid(ActiveDispatch))
	{
		auto* Projectile = ActiveDispatch.Get();
		ActiveDispatch = nullptr;
		Projectile->OnPayloadFinished.RemoveDynamic(this, &ThisClass::HandleDispatchFinished);
		if (!Projectile->IsActorBeingDestroyed()) { Projectile->Destroy(); }
	}
	RestoreDispatchPresentation();
	DispatchMainWeapon.Reset();
	DispatchOffWeapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
