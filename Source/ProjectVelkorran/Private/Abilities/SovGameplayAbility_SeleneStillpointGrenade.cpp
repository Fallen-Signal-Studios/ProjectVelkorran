// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "AbilitySystemComponent.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "Targeting/SovAimAssist.h"

USovGameplayAbility_SeleneStillpointGrenade::USovGameplayAbility_SeleneStillpointGrenade()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	GrenadeClass = ASovSeleneCombatProjectile::StaticClass();
	ChillEffectClass = FreezeEffectClass = USovGameplayEffect_SeleneControl::StaticClass();
	FrozenDamageOverTimeEffectClass = USovGameplayEffect_SeleneFrozenDOT::StaticClass();
	ResistantTargetDamageOverTimeEffectClass = USovGameplayEffect_SeleneFrostDOT::StaticClass();
	DetonationDamageEffectClass = USovGameplayEffect_SeleneDamage::StaticClass();
	MinimumEchoRequired = 35.0f;
	EchoCost = 35.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StillpointGrenade;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StillpointGrenade;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Universal;
	WeaponGatePolicy = ESovEchoWeaponGatePolicy::AnyAllowedWielded;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StillpointGrenadeName", "Stillpoint Grenade");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StillpointGrenadeDescription",
		"Open a cryothermal Stasis field that locks down standard enemies and damages them while Frozen; resistant targets are Chilled instead.");
}

bool USovGameplayAbility_SeleneStillpointGrenade::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(FuseDuration) && FuseDuration >= 0.0f && FuseDuration <= 10.0f
		&& FMath::IsFinite(StasisRadius) && StasisRadius > 0.0f && StasisRadius <= 10000.0f
		&& FMath::IsFinite(StasisDuration) && StasisDuration > 0.0f && StasisDuration <= 30.0f
		&& FMath::IsFinite(RefreezeLockout) && RefreezeLockout >= 0.0f && RefreezeLockout <= 60.0f
		&& FMath::IsFinite(DetonationDamage) && DetonationDamage >= 0.0f
		&& FMath::IsFinite(FrozenDamagePerSecond) && FrozenDamagePerSecond >= 0.0f;
}

void USovGameplayAbility_SeleneStillpointGrenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	Parameters.Mode = ESovSeleneProjectileMode::Stillpoint;
	Parameters.Direction = Direction;
	Parameters.Speed = 1500.0f;
	Parameters.Fuse = FuseDuration;
	Parameters.Radius = StasisRadius;
	Parameters.ControlDuration = StasisDuration;
	Parameters.RefreezeLockout = RefreezeLockout;
	Parameters.Damage = DetonationDamage;
	Parameters.DamagePerSecond = FrozenDamagePerSecond;
	Parameters.MaximumLifetime = FuseDuration + StasisDuration + 0.25f;
	SovAimAssist::FProjectileLeadRequest Lead;
	Lead.AimDirection = Direction;
	Lead.InitialVelocity = Direction * Parameters.Speed;
	Lead.Gravity = FVector(0., 0., GetWorld()->GetGravityZ() * Parameters.GravityScale);
	Lead.Range = Parameters.Range;
	Lead.MaximumFlightSeconds = Parameters.Fuse;
	Lead.CollisionRadius = 18.f; // Same sphere as ASovSeleneCombatProjectile::Advance.
	FVector AssistedVelocity;
	if (SovAimAssist::GetBallisticProjectileLead(Parameters.Context.SourceAvatar.Get(), Origin, Lead, AssistedVelocity))
	{
		Direction = AssistedVelocity.GetSafeNormal();
		Parameters.Direction = Direction;
	}
	auto* Projectile = ASovSeleneCombatProjectile::SpawnNativePayload(GrenadeClass, Origin, Parameters);
	if (!ContinueNativePayload(Epoch)) { if (IsValid(Projectile)) { Projectile->Destroy(); } return; }
	if (!Projectile) { FinishEchoAbility(true); return; }
	ReceiveNativeSelenePayloadReleased(Projectile, Origin, Direction);
	if (NativePayloadEpoch == Epoch && IsActive()) { FinishEchoAbility(false); }
}
