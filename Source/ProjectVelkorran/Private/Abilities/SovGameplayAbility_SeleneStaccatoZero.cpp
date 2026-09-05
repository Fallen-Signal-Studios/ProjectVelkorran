// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "AbilitySystemComponent.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Sovereign/SovGameplayTags.h"

USovGameplayAbility_SeleneStaccatoZero::USovGameplayAbility_SeleneStaccatoZero()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	EmpoweredShotDamageEffectClass = USovGameplayEffect_SeleneDamage::StaticClass();
	FreezeEffectClass = ResistantTargetChillEffectClass = USovGameplayEffect_SeleneControl::StaticClass();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StaccatoZero;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StaccatoZero;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Staccato;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StaccatoZeroName", "Staccato Zero");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StaccatoZeroDescription",
		"Fire one overdriven precision shot with extreme direct damage and a deterministic Freeze against a valid target.");
}

bool USovGameplayAbility_SeleneStaccatoZero::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(MaximumRange) && MaximumRange > 0.0f && MaximumRange <= 100000.0f
		&& FMath::IsFinite(DamageMultiplier) && DamageMultiplier > 0.0f
		&& FMath::IsFinite(BaseShotDamage) && BaseShotDamage > 0.0f
		&& FMath::IsFinite(ShotPoiseDamage) && ShotPoiseDamage >= 0.0f
		&& FMath::IsFinite(FreezeDuration) && FreezeDuration > 0.0f && FreezeDuration <= 30.0f
		&& FMath::IsFinite(RefreezeLockout) && RefreezeLockout >= 0.0f && RefreezeLockout <= 60.0f;
}

void USovGameplayAbility_SeleneStaccatoZero::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint32 Epoch = NativePayloadEpoch + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (NativePayloadEpoch != Epoch || !IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) { return; }
	if (!CanExecuteNativePayload(Epoch)) { FinishEchoAbility(true); return; }
	const FSovSelenePayloadContext Context = MakeNativePayloadContext();
	FVector Origin, Direction;
	if (!SovSelenePayload::Aim(Context, Origin, Direction)) { FinishEchoAbility(true); return; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SeleneStaccatoZero), true);
	SovSelenePayload::IgnoreSource(Query, Context.SourceAvatar.Get());
	FHitResult Hit;
	AActor* Target = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MaximumRange, ECC_Visibility, Query))
	{
		Target = SovSelenePayload::ResolveTarget(Hit.GetActor());
		// Scalar is authored once in the damage spec; precision HitResult keeps weak-point routing intact.
		const bool bAccepted = SovSelenePayload::Damage(Context, Target, &Hit, BaseShotDamage, ShotPoiseDamage, DamageMultiplier, true);
		if (!ContinueNativePayload(Epoch)) { return; }
		if (bAccepted) { SovSelenePayload::Control(Context, Target, FreezeDuration, RefreezeLockout, true); }
	}
	if (!ContinueNativePayload(Epoch)) { return; }
	ReceiveNativeSelenePayloadReleased(Target, Origin, Direction);
	if (NativePayloadEpoch == Epoch && IsActive()) { FinishEchoAbility(false); }
}
