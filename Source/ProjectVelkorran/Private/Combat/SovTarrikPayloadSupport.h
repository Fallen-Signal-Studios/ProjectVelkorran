// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Combat/SovNativeDamageReceipt.h"
#include "UObject/StrongObjectPtr.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Effects/SovGameplayEffect_CinderJudgement.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"

namespace SovTarrikPayload
{
inline UAbilitySystemComponent* ResolveASC(AActor* Actor)
{
	TSet<AActor*> Visited;
	for (int32 Depth = 0; IsValid(Actor) && Depth < 8 && !Visited.Contains(Actor); ++Depth)
	{
		Visited.Add(Actor);
		if (Actor->IsA<ANarrativeProjectile>()) { return nullptr; }
		if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) { return ASC; }
		if (auto* Owner = Cast<INarrativeCharacterOwner>(Actor))
		{
			if (auto* Character = Owner->GetNarrativeCharacter())
			{
				if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character)) { return ASC; }
			}
		}
		Actor = Actor->GetOwner();
	}
	return nullptr;
}
inline bool Alive(const UAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC) || ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)) { return false; }
	return ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER;
}
inline bool Hostile(UAbilitySystemComponent* Source, AActor* SourceActor, UAbilitySystemComponent* Target)
{
	AActor* Avatar = IsValid(Target) ? Target->GetAvatarActor() : nullptr;
	auto* Team = Cast<INarrativeTeamAgentInterface>(SourceActor);
	return IsValid(Source) && IsValid(SourceActor) && IsValid(Target) && IsValid(Avatar)
		&& Source != Target && Target->GetSet<UNarrativeAttributeSetBase>() && Team
		&& Team->GetTeamAttitudeTowards(*Avatar) == ETeamAttitude::Hostile && Alive(Target);
}
inline void IgnoreActorAndAttachments(FCollisionQueryParams& Query, AActor* Actor)
{
	if (!IsValid(Actor)) { return; }
	Query.AddIgnoredActor(Actor);
	TArray<AActor*> Attachments;
	Actor->GetAttachedActors(Attachments, true, true);
	Query.AddIgnoredActors(Attachments);
	if (auto* Character = Cast<ANarrativeCharacter>(Actor))
	{
		if (auto* Visual = Character->GetCharacterVisual())
		{
			Query.AddIgnoredActor(Visual);
			Visual->GetAttachedActors(Attachments, true, true);
			Query.AddIgnoredActors(Attachments);
		}
	}
}
inline bool Visible(UWorld* World, const FVector& Origin, AActor* Source, AActor* Causer, UAbilitySystemComponent* Target)
{
	AActor* Avatar = IsValid(Target) ? Target->GetAvatarActor() : nullptr;
	if (!IsValid(World) || !IsValid(Avatar)) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovTarrikPayloadSight), false);
	IgnoreActorAndAttachments(Query, Source);
	Query.AddIgnoredActor(Causer);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, Avatar->GetActorLocation(), ECC_Visibility, Query)) { return true; }
	return ResolveASC(Hit.GetActor()) == Target;
}
inline TSubclassOf<UGameplayEffect> DamageEffect(TSubclassOf<UGameplayEffect> Configured)
{
	const UGameplayEffect* Effect = Configured.Get() ? Configured->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Instant && Effect->Modifiers.IsEmpty()
		&& Effect->Executions.Num() == 1
		&& Effect->Executions[0].CalculationClass == UNarrativeDamageExecCalc::StaticClass()) { return Configured; }
	return USovGameplayEffect_CinderJudgementDamage::StaticClass();
}
inline TSubclassOf<UGameplayEffect> BurnEffect(TSubclassOf<UGameplayEffect> Configured)
{
	const UGameplayEffect* Effect = Configured.Get() ? Configured->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (Configured.Get() && Configured->IsChildOf(USovGameplayEffect_CinderGrenadeBurn::StaticClass())
		&& Effect->DurationPolicy == EGameplayEffectDurationType::HasDuration && Effect->Modifiers.IsEmpty()
		&& Effect->Executions.Num() == 1
		&& Effect->Executions[0].CalculationClass == UNarrativeDamageExecCalc::StaticClass()
		&& Effect->Period.GetValueAtLevel(1.f) > 0.f) { return Configured; }
	return USovGameplayEffect_CinderGrenadeBurn::StaticClass();
}
inline bool ApplyDamage(UAbilitySystemComponent* Source, AActor* Instigator, UAbilitySystemComponent* Target,
	const FGameplayEffectContextHandle& Context, TSubclassOf<UGameplayEffect> Effect,
	FGameplayTag AbilityTag, float Damage, float Poise, float Falloff = 1.f, bool* OutPoiseBroken = nullptr, bool bRequestBurn = false)
{
	if (OutPoiseBroken) { *OutPoiseBroken = false; }
	if (!Hostile(Source, Instigator, Target)) { return false; }
	auto* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(Source);
	if (!NarrativeSource) { return false; }
	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(DamageEffect(Effect), 1.f, Context.Duplicate());
	if (!Spec.IsValid()) { return false; }
	const auto& Tags = FSovGameplayTags::Get();
	Spec.Data->AddDynamicAssetTag(AbilityTag);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Kinetic);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_GuardClass_Heavy);
	if (bRequestBurn) { Spec.Data->AddDynamicAssetTag(Tags.Status_Apply_Burn); }
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_SourceModifier, Falloff);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, Poise * Falloff);
	TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
	Receipt->ExpectedTarget = Target->GetAvatarActor();
	Receipt->ExpectedContext = Spec.Data->GetContext().Get();
	NarrativeSource->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target);
	if (IsValid(NarrativeSource))
	{
		NarrativeSource->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
	}
	if (OutPoiseBroken) { *OutPoiseBroken = Receipt->bPoiseBroken; }
	return Receipt->bAppliedDamage && (!bRequestBurn || Receipt->bAcceptedControl);
}
inline void ApplyBurn(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target,
	const FGameplayEffectContextHandle& Context, TSubclassOf<UGameplayEffect> Effect,
	FGameplayTag AbilityTag, float Damage, float Duration)
{
	const auto& Tags = FSovGameplayTags::Get();
	if (!IsValid(Source) || !Alive(Target) || Target->HasMatchingGameplayTag(Tags.Status_Immunity_Burn)) { return; }
	FGameplayTagContainer Owned; Target->GetOwnedGameplayTags(Owned);
	if (Owned.HasTagExact(Tags.Status_Immunity)) { return; }
	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(BurnEffect(Effect), 1.f, Context);
	if (!Spec.IsValid()) { return; }
	Spec.Data->AddDynamicAssetTag(AbilityTag);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_BypassGuard);
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target);
}
}
