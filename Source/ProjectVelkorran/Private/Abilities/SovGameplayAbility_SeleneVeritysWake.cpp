// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "AbilitySystemComponent.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Sovereign/SovGameplayTags.h"

USovGameplayAbility_SeleneVeritysWake::USovGameplayAbility_SeleneVeritysWake()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	WaveClass = ASovSeleneCombatProjectile::StaticClass();
	WaveDamageEffectClass = USovGameplayEffect_SeleneDamage::StaticClass();
	ChillEffectClass = FreezeEffectClass = USovGameplayEffect_SeleneControl::StaticClass();
	FrostDamageOverTimeEffectClass = USovGameplayEffect_SeleneFrostDOT::StaticClass();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_VeritysWake;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_VeritysWake;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "VeritysWakeName", "Verity's Wake");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"VeritysWakeDescription",
		"Drive a fast frost wave down a combat lane, dealing Echo damage and Chill; centerline or already-Chilled targets Freeze and suffer Frost damage over time.");
}

bool USovGameplayAbility_SeleneVeritysWake::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(WaveRange) && WaveRange > 0.0f && WaveRange <= 10000.0f
		&& FMath::IsFinite(WaveWidth) && WaveWidth > 0.0f && WaveWidth <= 2000.0f
		&& FMath::IsFinite(GuaranteedFreezeCenterlineWidth) && GuaranteedFreezeCenterlineWidth >= 0.0f
		&& GuaranteedFreezeCenterlineWidth <= WaveWidth
		&& FMath::IsFinite(WaveDamage) && WaveDamage >= 0.0f
		&& FMath::IsFinite(WavePoiseDamage) && WavePoiseDamage >= 0.0f
		&& (WaveDamage > 0.0f || WavePoiseDamage > 0.0f)
		&& FMath::IsFinite(ControlDuration) && ControlDuration > 0.0f && ControlDuration <= 30.0f
		&& FMath::IsFinite(FrostDamagePerSecond) && FrostDamagePerSecond >= 0.0f;
}

void USovGameplayAbility_SeleneVeritysWake::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint32 Epoch = NativePayloadEpoch + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (NativePayloadEpoch != Epoch || !IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) { return; }
	if (!CanExecuteNativePayload(Epoch)) { FinishEchoAbility(true); return; }
	FSovSeleneProjectileParameters Parameters;
	Parameters.Context = MakeNativePayloadContext();
	FVector Origin, Direction;
	if (!SovSelenePayload::Aim(Parameters.Context, Origin, Direction)) { FinishEchoAbility(true); return; }
	Direction.Z = 0.0f;
	if (!Direction.Normalize()) { Direction = Parameters.Context.SourceAvatar->GetActorForwardVector().GetSafeNormal2D(); }
	Parameters.Mode = ESovSeleneProjectileMode::Wake;
	Parameters.Direction = Direction;
	Parameters.Damage = WaveDamage;
	Parameters.Poise = WavePoiseDamage;
	Parameters.Range = WaveRange;
	Parameters.Radius = WaveWidth * 0.5f;
	Parameters.CenterlineWidth = GuaranteedFreezeCenterlineWidth;
	Parameters.Speed = 2200.0f;
	Parameters.ControlDuration = ControlDuration;
	Parameters.DamagePerSecond = FrostDamagePerSecond;
	Parameters.MaximumLifetime = WaveRange / Parameters.Speed + 0.5f;
	auto* Projectile = ASovSeleneCombatProjectile::SpawnNativePayload(WaveClass, Origin, Parameters);
	if (!ContinueNativePayload(Epoch)) { if (IsValid(Projectile)) { Projectile->Destroy(); } return; }
	if (!Projectile) { FinishEchoAbility(true); return; }
	ReceiveNativeSelenePayloadReleased(Projectile, Origin, Direction);
	if (NativePayloadEpoch == Epoch && IsActive()) { FinishEchoAbility(false); }
}
