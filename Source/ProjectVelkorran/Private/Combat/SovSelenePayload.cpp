// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Combat/SovSelenePayload.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Components/SovCorruptionComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Controller.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"

namespace
{
	bool Alive(const UAbilitySystemComponent* ASC)
	{
		return IsValid(ASC) && !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
			&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
				|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER);
	}
	bool Immune(const UAbilitySystemComponent* ASC)
	{
		const auto& Tags = FSovGameplayTags::Get();
		return !IsValid(ASC) || ASC->HasMatchingGameplayTag(Tags.State_Invulnerable)
			|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
			|| ASC->HasMatchingGameplayTag(Tags.State_Damage_Immune)
			|| ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_All)
			|| (ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_Echo)
				&& ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_Thermal));
	}
	FGameplayEffectSpecHandle MakeSpec(const FSovSelenePayloadContext& Context,
		TSubclassOf<UGameplayEffect> Class, UObject* Identity = nullptr, const FHitResult* Hit = nullptr)
	{
		if (!SovSelenePayload::ValidSource(Context)) { return {}; }
		FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(Identity ? Identity : Context.SourceObject.Get());
		EffectContext.AddOrigin(Context.SourceAvatar->GetActorLocation());
		if (Hit) { EffectContext.AddHitResult(*Hit, true); }
		FGameplayEffectSpecHandle Spec = Context.SourceASC->MakeOutgoingSpec(Class, Context.Level, EffectContext);
		if (Spec.IsValid()) { Spec.Data->AddDynamicAssetTag(Context.AbilityTag); }
		return Spec;
	}
	bool GrantDuration(const FSovSelenePayloadContext& Context, UAbilitySystemComponent* Target,
		const FGameplayTagContainer& Grants, float Duration)
	{
		if (!IsValid(Target) || !SovSelenePayload::EligibleTarget(Context, Target->GetAvatarActor())
			|| !FMath::IsFinite(Duration) || Duration <= 0.0f) { return false; }
		FGameplayEffectSpecHandle Spec = MakeSpec(Context, USovGameplayEffect_SeleneControl::StaticClass());
		if (!Spec.IsValid()) { return false; }
		Spec.Data->DynamicGrantedTags.AppendTags(Grants);
		Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
		Spec.Data->SetDuration(Duration, true);
		return Context.SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target).IsValid();
	}
	void ConfigureDamage(FGameplayEffectSpec& Spec, float Damage, float Poise, float Scalar)
	{
		const auto& Tags = FSovGameplayTags::Get();
		Spec.AddDynamicAssetTag(Tags.Damage_Channel_Echo);
		Spec.AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
		Spec.SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, Poise);
		Spec.SetSetByCallerMagnitude(Tags.SetByCaller_Damage_AbilityScalar, Scalar);
	}
}

void USovNativeDamageReceipt::ReceiveResult(const FSovDamageResult& Result)
{
	if (Result.TargetActor != ExpectedTarget.Get()
		|| (ExpectedContext ? Result.EffectContext.Get() != ExpectedContext : Result.EffectContext.GetSourceObject() != this)) { return; }
	bAppliedDamage = Result.AppliedHealthDamage > 0.0f || Result.AppliedShieldDamage > 0.0f || Result.AppliedPoiseDamage > 0.0f;
	bAcceptedControl = Result.bStatusApplicationRequested;
	bPoiseBroken = Result.bPoiseBroken;
}

bool SovSelenePayload::ValidSource(const FSovSelenePayloadContext& Context)
{
	const auto& Tags = FSovGameplayTags::Get();
	return Context.SourceAvatar.IsValid() && Context.SourceAvatar->HasAuthority()
		&& !Context.SourceAvatar->IsActorBeingDestroyed() && Alive(Context.SourceASC.Get())
		&& Context.SourceASC->GetAvatarActor() == Context.SourceAvatar.Get()
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Context.SourceAvatar.Get()) == Context.SourceASC.Get()
		&& Context.SourceASC->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		&& !Context.SourceASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik);
}
AActor* SovSelenePayload::ResolveTarget(AActor* Actor)
{
	TSet<AActor*> Visited;
	for (int32 Depth = 0; IsValid(Actor) && Depth < 8 && !Visited.Contains(Actor); ++Depth)
	{
		Visited.Add(Actor);
		if (Actor->IsA<ANarrativeProjectile>()) { return nullptr; }
		if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
		{
			return IsValid(ASC->GetAvatarActor()) ? ASC->GetAvatarActor() : Actor;
		}
		if (auto* Provider = Cast<INarrativeCharacterOwner>(Actor))
		{
			if (auto* Character = Provider->GetNarrativeCharacter()) { return Character; }
		}
		Actor = Actor->GetOwner();
	}
	return nullptr;
}
bool SovSelenePayload::EligibleTarget(const FSovSelenePayloadContext& Context, AActor* Target)
{
	if (!ValidSource(Context) || !IsValid(Target) || Target == Context.SourceAvatar.Get()
		|| Target->IsOwnedBy(Context.SourceAvatar.Get()) || Target->IsActorBeingDestroyed()) { return false; }
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	const auto* Team = Cast<INarrativeTeamAgentInterface>(Context.SourceAvatar.Get());
	return Alive(ASC) && !Immune(ASC) && Team && Team->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
}
void SovSelenePayload::IgnoreSource(FCollisionQueryParams& Params, AActor* Source)
{
	if (!IsValid(Source)) { return; }
	Params.AddIgnoredActor(Source);
	TArray<AActor*> Attached;
	Source->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	if (const auto* Character = Cast<ANarrativeCharacter>(Source))
	{
		if (auto* Visual = Character->GetCharacterVisual())
		{
			Params.AddIgnoredActor(Visual);
			Visual->GetAttachedActors(Attached, true, true);
			Params.AddIgnoredActors(Attached);
		}
	}
}
bool SovSelenePayload::Aim(const FSovSelenePayloadContext& Context, FVector& Origin, FVector& Direction)
{
	if (!ValidSource(Context)) { return false; }
	FRotator Rotation;
	Context.SourceAvatar->GetActorEyesViewPoint(Origin, Rotation);
	if (const auto* Pawn = Cast<APawn>(Context.SourceAvatar.Get()))
	{
		if (Pawn->GetController()) { Rotation = Pawn->GetController()->GetControlRotation(); }
	}
	Direction = Rotation.Vector().GetSafeNormal();
	return !Origin.ContainsNaN() && !Direction.ContainsNaN() && !Direction.IsNearlyZero()
		&& FVector::DistSquared(Origin, Context.SourceAvatar->GetActorLocation()) <= FMath::Square(400.0f);
}
bool SovSelenePayload::Visible(const FSovSelenePayloadContext& Context, FVector Origin, AActor* Target, FVector Point)
{
	if (!EligibleTarget(Context, Target) || Origin.ContainsNaN() || Point.ContainsNaN()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SelenePayloadLOS), false);
	IgnoreSource(Params, Context.SourceAvatar.Get());
	FHitResult Hit;
	return !Target->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Point, ECC_Visibility, Params)
		|| ResolveTarget(Hit.GetActor()) == Target;
}
bool SovSelenePayload::Damage(const FSovSelenePayloadContext& Context, AActor* Target, const FHitResult* Hit,
	float BaseDamage, float Poise, float Scalar, bool bRequestControl)
{
	if (!EligibleTarget(Context, Target) || !FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f
		|| !FMath::IsFinite(Poise) || Poise < 0.0f || !FMath::IsFinite(Scalar) || Scalar <= 0.0f) { return false; }
	auto* Source = Cast<UNarrativeAbilitySystemComponent>(Context.SourceASC.Get());
	auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!Source || !TargetASC) { return false; }
	TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
	Receipt->ExpectedTarget = Target;
	auto Spec = MakeSpec(Context, USovGameplayEffect_SeleneDamage::StaticClass(), nullptr, Hit);
	if (!Spec.IsValid()) { return false; }
	Receipt->ExpectedContext = Spec.Data->GetContext().Get();
	ConfigureDamage(*Spec.Data.Get(), BaseDamage, Poise, Scalar);
	if (bRequestControl)
	{
		Spec.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Status_Application_NativeOwned);
		Spec.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Status_Apply_Freeze);
		Spec.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Status_Apply_Chill);
	}
	Source->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	if (IsValid(Source)) { Source->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
	return bRequestControl ? Receipt->bAcceptedControl : Receipt->bAppliedDamage;
}
bool SovSelenePayload::HasStatus(AActor* Target, FGameplayTag Tag)
{
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(Tag);
}
bool SovSelenePayload::Control(const FSovSelenePayloadContext& Context, AActor* Target,
	float Duration, float RefreezeLockout, bool bRequestFreeze)
{
	if (!EligibleTarget(Context, Target) || !FMath::IsFinite(Duration) || Duration <= 0.0f
		|| !FMath::IsFinite(RefreezeLockout) || RefreezeLockout < 0.0f) { return false; }
	auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	const auto& Tags = FSovGameplayTags::Get();
	const auto& Narrative = FNarrativeGameplayTags::Get();
	FGameplayTagContainer Owned;
	ASC->GetOwnedGameplayTags(Owned);
	if (Owned.HasTagExact(Tags.Status_Immunity) || Owned.HasTag(Tags.Status_Immunity_All)) { return false; }
	const bool bFreeze = bRequestFreeze && !ASC->HasMatchingGameplayTag(Tags.Status_Immunity_Freeze)
		&& !ASC->HasMatchingGameplayTag(Tags.Character_Enemy_Boss)
		&& !ASC->HasMatchingGameplayTag(Tags.State_InterruptProtected)
		&& !ASC->HasMatchingGameplayTag(Tags.State_Poise_Recovering)
		&& !ASC->HasMatchingGameplayTag(Tags.State_Poise_SuperArmor)
		&& !ASC->HasMatchingGameplayTag(Tags.State_Status_Frozen);
	if (!bFreeze && ASC->HasMatchingGameplayTag(Tags.Status_Immunity_Chill)) { return false; }
	FGameplayTagContainer Grants;
	Grants.AddTag(bFreeze ? Tags.State_Status_Frozen : Tags.State_Status_Chilled);
	Grants.AddTag(bFreeze ? Narrative.State_Movement_Lock : Narrative.State_Movement_SlowWalking);
	if (bFreeze) { Grants.AddTag(Narrative.State_Busy); }
	const float ResolvedDuration = USovCorruptionComponent::ResolveIncomingStatusDuration(Target, Duration);
	if (!GrantDuration(Context, ASC, Grants, ResolvedDuration)) { return false; }
	if (bFreeze && EligibleTarget(Context, Target))
	{
		// Separate owned count survives thaw; overlapping casts never reset another cast's duration.
		FGameplayTagContainer Lockout(Tags.Status_Immunity_Freeze);
		GrantDuration(Context, ASC, Lockout, ResolvedDuration + RefreezeLockout);
		FGameplayTagContainer Cancel;
		Cancel.AddTag(Narrative.Ability_WeaponFire);
		Cancel.AddTag(Narrative.Ability_MeleeAttack);
		Cancel.AddTag(Narrative.Ability_MagicAttack);
		Cancel.AddTag(Tags.Ability_NPC_ReformationDrone_Gunfire);
		Cancel.AddTag(Tags.Ability_NPC_ReformationDrone_RocketLauncher);
		Cancel.AddTag(Tags.Ability_NPC_ReformationDrone_SelfDestruct);
		Cancel.AddTag(Tags.Ability_NPC_DominionHound_Bite);
		Cancel.AddTag(Tags.Ability_NPC_DominionHound_HornCharge);
		Cancel.AddTag(Tags.Ability_NPC_DominionHound_Pounce);
		Cancel.AddTag(Tags.Ability_NPC_DominionHandler_CommandHound);
		if (IsValid(ASC)) { ASC->CancelAbilities(&Cancel); }
	}
	return bFreeze && HasStatus(Target, Tags.State_Status_Frozen);
}
void SovSelenePayload::FrostDOT(const FSovSelenePayloadContext& Context, AActor* Target,
	float DamagePerTick, float Duration, bool bFrozenOnly)
{
	if (!EligibleTarget(Context, Target) || !FMath::IsFinite(DamagePerTick) || DamagePerTick <= 0.0f
		|| !FMath::IsFinite(Duration) || Duration <= 0.0f) { return; }
	auto Spec = MakeSpec(Context, bFrozenOnly ? USovGameplayEffect_SeleneFrozenDOT::StaticClass()
		: USovGameplayEffect_SeleneFrostDOT::StaticClass());
	if (!Spec.IsValid()) { return; }
	ConfigureDamage(*Spec.Data.Get(), DamagePerTick, 0.0f, 1.0f);
	Spec.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Damage_BypassGuard);
	Spec.Data->AddDynamicAssetTag(FSovGameplayTags::Get().Damage_BypassDeflection);
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
	Spec.Data->SetDuration(Duration, true);
	Context.SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
}
