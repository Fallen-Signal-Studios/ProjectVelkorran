// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Corruption/SovCorruptionSourceComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "Corruption/SovCorruptionMath.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Sovereign/SovGameplayTags.h"

namespace
{
	bool IsLivingCorruptionActor(AActor* Actor)
	{
		const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
		return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && IsValid(ASC) && ASC->GetAvatarActor() == Actor
			&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
			&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.0f;
	}
}
USovCorruptionSourceComponent::USovCorruptionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}
void USovCorruptionSourceComponent::BeginPlay() { Super::BeginPlay(); BindOwner(); }
void USovCorruptionSourceComponent::BindOwner()
{
	auto* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
	if (ASC == BoundASC.Get()) { return; }
	if (BoundASC.IsValid())
	{
		BoundASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleDamageAsSource);
		BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamageAsTarget);
		BoundASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeathState);
	}
	BoundASC = ASC;
	if (ASC && GetOwner()->HasAuthority())
	{
		ASC->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::HandleDamageAsSource);
		ASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamageAsTarget);
		ASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeathState);
	}
}
bool USovCorruptionSourceComponent::IsResolvedFor(AActor* Target) const
{
	const auto* Pawn = Cast<APawn>(Target);
	const auto* Campaign = Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	return !Campaign || !Campaign->IsStateValid() || !Campaign->GetActiveMission()
		|| ResolvedMissions.Contains(Campaign->GetActiveMission()->MissionId);
}
bool USovCorruptionSourceComponent::ValidateSpatialTarget(AActor* Target, float& OutFalloff) const
{
	OutFalloff = 0.0f;
	FString Error;
	if (bEnding || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || GetOwner()->IsActorBeingDestroyed()
		|| !IsValid(Profile) || !Profile->ValidateProfile(Error) || !IsLivingCorruptionActor(Target)
		|| Target == GetOwner() || Target->GetWorld() != GetWorld() || IsResolvedFor(Target)) { return false; }
	OutFalloff = SovCorruptionMath::Falloff(FVector::Distance(GetOwner()->GetActorLocation(), Target->GetActorLocation()),
		Profile->Radius, Profile->bLinearFalloff);
	if (OutFalloff <= 0.0f) { return false; }
	if (Profile->bRequiresLineOfSight)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(CorruptionProducerLOS), false, GetOwner());
		Query.AddIgnoredActor(Target);
		TArray<AActor*> Attached;
		GetOwner()->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
		Attached.Reset(); Target->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, GetOwner()->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query)) { return false; }
	}
	return true;
}
bool USovCorruptionSourceComponent::ValidateContact(AActor* Target, float& OutFalloff) const
{
	if (!ValidateSpatialTarget(Target, OutFalloff)) { return false; }
	if (Profile->SourceKind == ESovCorruptionSourceKind::CommandLink)
	{
		const auto* Link = GetOwner()->FindComponentByClass<USovCommandLinkComponent>();
		return Link && Link->IsCommandLinkActive() && Link->ContainsLinkedActor(Target);
	}
	if (Profile->SourceKind == ESovCorruptionSourceKind::ContaminatedAlly)
	{
		const auto* Team = Cast<INarrativeTeamAgentInterface>(Target);
		const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
		return IsLivingCorruptionActor(GetOwner()) && Team && Team->GetTeamAttitudeTowards(*GetOwner()) == ETeamAttitude::Friendly
			&& ASC && ASC->HasMatchingGameplayTag(Profile->ContaminatedAllyState);
	}
	return false;
}
void USovCorruptionSourceComponent::HandleDamageAsSource(const FSovDamageResult& Result)
{
	if (bEnding || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Profile)
		|| Profile->SourceKind != ESovCorruptionSourceKind::EnemyAttack || !BoundASC.IsValid()
		|| BoundASC->GetAvatarActor() != GetOwner()
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != BoundASC.Get() || Result.SourceActor != GetOwner()
		|| !Result.TransactionId.IsValid() || SeenTransactions.Contains(Result.TransactionId)
		|| !Result.EffectContext.IsValid() || Result.EffectContext.GetOriginalInstigatorAbilitySystemComponent() != BoundASC.Get()
		|| !Result.DamageChannels.HasTagExact(FSovGameplayTags::Get().Damage_Channel_Corruption)
		|| Result.RejectedDamageChannels.HasTagExact(FSovGameplayTags::Get().Damage_Channel_Corruption)
		|| !SovCorruptionMath::AcceptedHit(Result.bPeriodicDamage, Result.DefenseKind != ESovDefenseKind::None,
			Result.AppliedHealthDamage, Result.AppliedShieldDamage, Result.bStatusApplicationRequested)) { return; }
	const auto* Team = Cast<INarrativeTeamAgentInterface>(GetOwner());
	if (!IsValid(Result.TargetActor) || !Team || Team->GetTeamAttitudeTowards(*Result.TargetActor) != ETeamAttitude::Hostile) { return; }
	float Falloff;
	if (!ValidateSpatialTarget(Result.TargetActor, Falloff)) { return; }
	SeenTransactions.Add(Result.TransactionId); TransactionOrder.Add(Result.TransactionId);
	if (TransactionOrder.Num() > 256) { SeenTransactions.Remove(TransactionOrder[0]); TransactionOrder.RemoveAt(0); }
	if (auto* Target = Result.TargetActor->FindComponentByClass<USovCorruptionComponent>())
	{
		Target->ApplyVerifiedPulse(Profile, GetOwner(), Falloff);
	}
}
void USovCorruptionSourceComponent::HandleDamageAsTarget(const FSovDamageResult& Result)
{
	if (Result.TargetActor == GetOwner() && Result.TransactionId.IsValid()
		&& Result.AppliedHealthDamage + Result.AppliedShieldDamage > 0.0f) { ++DamageSequence; }
	if (!bEnding && IsValid(Profile) && Profile->Escape == ESovCorruptionEscape::DestroyNode
		&& Result.TargetActor == GetOwner() && Result.TransactionId.IsValid() && Result.bFatal
		&& Result.AppliedHealthDamage > 0.0f && BoundASC.IsValid() && BoundASC->GetAvatarActor() == GetOwner())
	{
		ResolveAll(ESovCorruptionEscape::DestroyNode);
	}
}
void USovCorruptionSourceComponent::HandleDeathState(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead)
{
	if (bIsDead && Actor == GetOwner() && ASC == BoundASC.Get() && ASC && ASC->GetAvatarActor() == Actor
		&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.0f
		&& IsValid(Profile) && Profile->Escape == ESovCorruptionEscape::DestroyNode)
	{
		ResolveAll(ESovCorruptionEscape::DestroyNode);
	}
}
bool USovCorruptionSourceComponent::ResolveRemedy(AActor* Player, ESovCorruptionEscape Remedy)
{
	if (bEnding || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Profile) || Profile->Escape != Remedy) { return false; }
	auto* Target = IsValid(Player) ? Player->FindComponentByClass<USovCorruptionComponent>() : nullptr;
	ESovCorruptionBand Cap; FName Mission;
	if (!Target || Target->bMutating || !Target->ValidateProfilePermission(Profile, Cap, Mission)) { return false; }
	ResolvedMissions.Add(Mission); // Commit the disabled source before cleansing emits callbacks.
	Target->ApplyVerifiedRemedy(Profile, Remedy);
	return true;
}
void USovCorruptionSourceComponent::ResolveAll(ESovCorruptionEscape Remedy)
{
	if (!GetWorld()) { return; }
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		ResolveRemedy(*It, Remedy);
		if (bEnding || !IsValid(GetOwner())) { return; }
	}
	ReleaseContacts();
}
void USovCorruptionSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (bEnding || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	BindOwner();
	if (!IsValid(Profile)) { ReleaseContacts(); return; }
	if (Profile->SourceKind == ESovCorruptionSourceKind::CommandLink)
	{
		auto* Link = GetOwner()->FindComponentByClass<USovCommandLinkComponent>();
		if (Link && Link == ObservedLink.Get() && Link->GetLinkInstanceId() == ObservedLinkInstance
			&& Link->GetCommandLinkState() == ESovCommandLinkState::Severed && Profile->Escape == ESovCorruptionEscape::BreakLink)
		{
			ResolveAll(ESovCorruptionEscape::BreakLink);
		}
		if (Link && Link->IsCommandLinkActive()) { ObservedLink = Link; ObservedLinkInstance = Link->GetLinkInstanceId(); }
	}
	if (Profile->SourceKind != ESovCorruptionSourceKind::CommandLink && Profile->SourceKind != ESovCorruptionSourceKind::ContaminatedAlly)
	{
		ReleaseContacts(); return;
	}
	TSet<USovCorruptionComponent*> Present;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		float Falloff;
		if (!ValidateContact(*It, Falloff)) { continue; }
		auto* Target = It->FindComponentByClass<USovCorruptionComponent>();
		if (!Target) { continue; }
		const auto Handle = Target->AcquireProducer(this);
		if (bEnding) { return; }
		if (Handle.IsValid()) { Present.Add(Target); Contacts.Add(Target, Handle); }
	}
	for (auto It = Contacts.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Present.Contains(It.Key().Get()))
		{
			if (It.Key().IsValid()) { It.Key()->ReleaseProducer(It.Value(), this); }
			It.RemoveCurrent();
		}
	}
}
void USovCorruptionSourceComponent::ReleaseContacts()
{
	const auto Previous = MoveTemp(Contacts); Contacts.Empty();
	for (const auto& Pair : Previous) { if (Pair.Key.IsValid()) { Pair.Key->ReleaseProducer(Pair.Value, this); } }
}
void USovCorruptionSourceComponent::Load_Implementation()
{
	ReleaseContacts(); SeenTransactions.Empty(); TransactionOrder.Empty(); ObservedLink.Reset(); ObservedLinkInstance.Invalidate();
}
void USovCorruptionSourceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding = true; ReleaseContacts();
	if (BoundASC.IsValid())
	{
		BoundASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleDamageAsSource);
		BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamageAsTarget);
		BoundASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeathState);
	}
	Super::EndPlay(Reason);
}
