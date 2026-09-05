// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Combat/SovTarrikPayloadSupport.h"
#include "Combat/SovCinderLineMath.h"
#include "Effects/SovGameplayEffect_CinderWard.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"

USovGameplayAbility_TarrikCinderSlam::USovGameplayAbility_TarrikCinderSlam()
{
	const auto& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = EchoCost = 90.f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderSlam;
	FGameplayTagContainer AssetTags = GetAssetTags(); AssetTags.AddTag(EchoSpendTag); SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderSlam;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Velkorran;
	RadialDamageEffectClass = USovGameplayEffect_CinderJudgementDamage::StaticClass();
	ProtectiveWardEffectClass = USovGameplayEffect_CinderWard::StaticClass();
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderSlamName", "Cinder Slam");
	AbilityDescription = NSLOCTEXT("SovTarrikEcho", "CinderSlamDescription",
		"Drive Velkorran into the ground, releasing a radial Cinder blast with heavy Poise pressure and a brief protective ward around Tarrik.");
}

bool USovGameplayAbility_TarrikCinderSlam::HasRequiredPayloadConfiguration() const
{
	return FMath::IsFinite(SlamRadius) && SlamRadius > 0.f && SlamRadius <= 5000.f
		&& FMath::IsFinite(SlamDamage) && SlamDamage > 0.f
		&& FMath::IsFinite(SlamPoiseDamage) && SlamPoiseDamage >= 0.f
		&& FMath::IsFinite(KnockbackStrength) && KnockbackStrength >= 0.f
		&& FMath::IsFinite(MinimumSlamDamageFraction) && MinimumSlamDamageFraction >= 0.f && MinimumSlamDamageFraction <= 1.f
		&& FMath::IsFinite(WardDuration) && WardDuration > 0.f
		&& SovCinderLine::ValidTiming(PayloadReleaseDelay, PostReleaseRecovery, MaximumActiveDuration);
}

void USovGameplayAbility_TarrikCinderSlam::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bSlamReleaseAttempted = false;
	const uint64 Activation = GetTarrikActivationSerial() + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!bSlamReleaseAttempted) { ArmTarrikPayload(PayloadReleaseDelay, Activation); }
}
void USovGameplayAbility_TarrikCinderSlam::ExecuteAutomaticTarrikPayload()
{
	const uint64 Activation = GetTarrikActivationSerial();
	if (!ReleaseCinderSlam() && IsActive() && GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
}

bool USovGameplayAbility_TarrikCinderSlam::ReleaseCinderSlam()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || bSlamReleaseAttempted) { return false; }
	bSlamReleaseAttempted = true;
	if (!IsTarrikReleaseContextValid() || !HasRequiredPayloadConfiguration() || !GetWorld()) { FinishEchoAbility(true); return false; }
	const uint64 Activation = GetTarrikActivationSerial();
	UAbilitySystemComponent* Source = CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	const FVector Origin = Avatar->GetActorLocation();
	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.SetAbility(this); Context.AddInstigator(Avatar, Avatar); Context.AddOrigin(Origin);
	Context.AddSourceObject(GetCurrentSourceObject());

	TSubclassOf<UGameplayEffect> WardClass = USovGameplayEffect_CinderWard::StaticClass();
	const UGameplayEffect* Ward = ProtectiveWardEffectClass.Get() ? ProtectiveWardEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (Ward && ProtectiveWardEffectClass->IsChildOf(USovGameplayEffect_CinderWard::StaticClass())
		&& Ward->DurationPolicy == EGameplayEffectDurationType::HasDuration && Ward->Executions.IsEmpty()
		&& Ward->Modifiers.Num() == 1 && Ward->Modifiers[0].Attribute == UNarrativeAttributeSetBase::GetDamageResistanceAttribute()
		&& Ward->Modifiers[0].ModifierOp == EGameplayModOp::Additive) { WardClass = ProtectiveWardEffectClass; }
	FGameplayEffectSpecHandle WardSpec = Source->MakeOutgoingSpec(WardClass, 1.f, Context);
	if (WardSpec.IsValid())
	{
		WardSpec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, WardDuration);
		WardSpec.Data->SetDuration(WardDuration, true);
		Source->ApplyGameplayEffectSpecToSelf(*WardSpec.Data.Get());
	}
	if (!IsActive() || GetTarrikActivationSerial() != Activation) { return true; }

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic); Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCinderSlam), false, Avatar);
	SovTarrikPayload::IgnoreActorAndAttachments(Query, Avatar);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(SlamRadius), Query);
	TSet<UAbilitySystemComponent*> Seen;
	int32 Resolved = 0;
	const auto& Tags = FSovGameplayTags::Get();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!IsActive() || GetTarrikActivationSerial() != Activation) { break; }
		UAbilitySystemComponent* Target = SovTarrikPayload::ResolveASC(Overlap.GetActor());
		if (!Target || Seen.Contains(Target)) { continue; }
		Seen.Add(Target);
		if (!SovTarrikPayload::Hostile(Source, Avatar, Target)
			|| !SovTarrikPayload::Visible(GetWorld(), Origin, Avatar, Avatar, Target)) { continue; }
		AActor* TargetActor = Target->GetAvatarActor();
		const float Distance = FVector::Distance(Origin, TargetActor->GetActorLocation());
		if (Distance > SlamRadius) { continue; }
		bool bPayloadBrokePoise = false;
		const bool bRejectKnockback = Target->HasMatchingGameplayTag(Tags.State_Poise_SuperArmor)
			|| Target->HasMatchingGameplayTag(Tags.Character_Enemy_Boss);
		const float Falloff = FMath::Lerp(1.f, MinimumSlamDamageFraction, Distance / SlamRadius);
		if (SovTarrikPayload::ApplyDamage(Source, Avatar, Target, Context, RadialDamageEffectClass,
			EchoSpendTag, SlamDamage, SlamPoiseDamage, Falloff, &bPayloadBrokePoise))
		{
			++Resolved;
			if (SovTarrikPayload::Alive(Target) && bPayloadBrokePoise && !bRejectKnockback
				&& Target->HasMatchingGameplayTag(Tags.State_Poise_Broken))
			{
				if (ACharacter* Character = Cast<ACharacter>(TargetActor))
				{
					FVector Direction = (TargetActor->GetActorLocation() - Origin).GetSafeNormal2D();
					Character->LaunchCharacter((Direction + FVector(0.f, 0.f, 0.25f)).GetSafeNormal() * KnockbackStrength, false, false);
				}
			}
		}
	}
	if (IsActive() && GetTarrikActivationSerial() == Activation)
	{
		ReceiveCinderSlamReleased(Origin, SlamRadius, Resolved);
		BeginTarrikRecovery(PostReleaseRecovery, Activation);
	}
	return true;
}
