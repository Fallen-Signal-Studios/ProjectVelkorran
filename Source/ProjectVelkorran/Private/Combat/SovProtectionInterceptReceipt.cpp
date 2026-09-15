// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Combat/SovProtectionInterceptReceipt.h"
#include "Resonance/SovResonanceComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "ArsenalStatics.h"
#include "ArsenalSettings.h"
#include "Character/NarrativeCharacterVisual.h"
#include "CollisionQueryParams.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/WeaponVisual.h"

namespace
{
UAbilitySystemComponent* ResolveASC(AActor* Actor)
{
	TSet<AActor*> Seen;
	for (int32 Depth = 0; IsValid(Actor) && Depth < 6 && !Seen.Contains(Actor); ++Depth)
	{
		Seen.Add(Actor);
		// A crate, carried actor or projectile is still cover even when its Owner
		// happens to be the intended ally. Only known body/weapon presentation
		// proxies may stand in for a character in this geometric proof.
		if (!Actor->IsA<ANarrativeCharacter>() && !Actor->IsA<ANarrativeCharacterVisual>()
			&& !Actor->IsA<AWeaponVisual>()) { return nullptr; }
		if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) { return ASC; }
		if (auto* Provider = Cast<INarrativeCharacterOwner>(Actor))
		{
			if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Provider->GetNarrativeCharacter())) { return ASC; }
		}
		Actor = Actor->GetOwner();
	}
	return nullptr;
}
bool IsLivingProtectionParticipant(UAbilitySystemComponent* ASC)
{
	return IsValid(ASC) && ASC->GetSet<UNarrativeAttributeSetBase>()
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER;
}
bool FirstBlocker(UWorld* World, const FVector& Start, const FVector& End, float Radius,
	ECollisionChannel Channel, const FCollisionQueryParams& Query, FHitResult& Out)
{
	TArray<FHitResult> Hits;
	if (FMath::IsNearlyZero(Radius)) { World->LineTraceMultiByChannel(Hits, Start, End, Channel, Query); }
	else { World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), Query); }
	for (const FHitResult& Hit : Hits) { if (Hit.bBlockingHit) { Out = Hit; return true; } }
	return false;
}
void IgnoreProtectorVisuals(FCollisionQueryParams& Query, ANarrativeCharacter* Protector, UAbilitySystemComponent* ASC)
{
	Query.AddIgnoredActor(Protector);
	TArray<AActor*> Candidates;
	Protector->GetAttachedActors(Candidates, true, true);
	if (auto* Visual = Protector->GetCharacterVisual())
	{
		Candidates.Add(Visual);
		Visual->GetAttachedActors(Candidates, false, true);
	}
	Candidates.Add(Protector->GetWieldedWeaponVisual(true));
	Candidates.Add(Protector->GetWieldedWeaponVisual(false));
	for (AActor* Candidate : Candidates)
	{
		// Never ignore an arbitrary owned world blocker, carried actor or projectile.
		if (IsValid(Candidate) && (Candidate->IsA<ANarrativeCharacterVisual>() || Candidate->IsA<AWeaponVisual>())
			&& ResolveASC(Candidate) == ASC) { Query.AddIgnoredActor(Candidate); }
	}
}
}

USovProtectionInterceptReceipt* USovProtectionInterceptReceipt::TryCreateForDroneShot(ANarrativeCharacter* Threat,
	AActor* IntendedFocus, const FHitResult& ActualHit, const FVector& Start, const FVector& End, float Radius)
{
	if (!IsValid(Threat) || !Threat->HasAuthority() || !Threat->GetWorld() || !ActualHit.bBlockingHit
		|| ActualHit.bStartPenetrating || Start.ContainsNaN() || End.ContainsNaN() || !FMath::IsFinite(Radius)
		|| Radius < 0.f || Start.Equals(End)) { return nullptr; }
	auto* Source = ResolveASC(Threat);
	auto* Target = ResolveASC(ActualHit.GetActor());
	auto* Ally = ResolveASC(IntendedFocus);
	auto* Protector = Target ? Cast<ANarrativeCharacter>(Target->GetAvatarActor()) : nullptr;
	AActor* Protected = Ally ? Ally->GetAvatarActor() : nullptr;
	auto* Controller = Cast<AAIController>(Threat->GetController());
	auto* ThreatTeam = Cast<INarrativeTeamAgentInterface>(Threat);
	auto* ProtectorTeam = Cast<INarrativeTeamAgentInterface>(Protector);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (!IsLivingProtectionParticipant(Source) || !IsLivingProtectionParticipant(Target) || !IsLivingProtectionParticipant(Ally)
		|| Source->GetAvatarActor() != Threat
		|| !IsValid(Protector) || !IsValid(Protected) || Protected == Protector || Target == Source || Ally == Source
		|| !Controller || ResolveASC(Controller->GetFocusActor()) != Ally
		|| !Target->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)
		|| Target->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		|| !ThreatTeam || !ProtectorTeam || ThreatTeam->GetTeamAttitudeTowards(*Protector) != ETeamAttitude::Hostile
		|| ThreatTeam->GetTeamAttitudeTowards(*Protected) != ETeamAttitude::Hostile
		|| ProtectorTeam->GetTeamAttitudeTowards(*Protected) != ETeamAttitude::Friendly) { return nullptr; }
	// Recreate Narrative's original query exactly before removing the protector's collision.
	FCollisionQueryParams Query = Threat->GetIgnoreCharacterParams();
	Query.bTraceComplex = true; Query.bReturnPhysicalMaterial = true;
	const ECollisionChannel Channel = UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel;
	FHitResult Confirmed;
	if (!FirstBlocker(Threat->GetWorld(), Start, End, Radius, Channel, Query, Confirmed)
		|| ResolveASC(Confirmed.GetActor()) != Target || !Confirmed.ImpactPoint.Equals(ActualHit.ImpactPoint, 1.f)) { return nullptr; }
	IgnoreProtectorVisuals(Query, Protector, Target);
	FHitResult Counterfactual;
	if (!FirstBlocker(Threat->GetWorld(), Start, End, Radius, Channel, Query, Counterfactual)
		|| ResolveASC(Counterfactual.GetActor()) != Ally
		|| Counterfactual.Distance <= Confirmed.Distance + KINDA_SMALL_NUMBER) { return nullptr; }
	auto* Receipt = NewObject<USovProtectionInterceptReceipt>();
	Receipt->ThreatActor = Threat; Receipt->ProtectorActor = Protector; Receipt->ProtectedActor = Protected;
	Receipt->ThreatASC = Source; Receipt->ProtectorASC = Target;
	return Receipt;
}
bool USovProtectionInterceptReceipt::ArmForDamage(const FGameplayEffectContextHandle& Context,
	UAbilitySystemComponent* Source, UAbilitySystemComponent* Target)
{
	if (bDelivered || bConsumed || ExpectedContext.IsValid() || !Context.IsValid() || Source != ThreatASC.Get()
		|| Target != ProtectorASC.Get() || !IsValid(Source) || !IsValid(Target)
		|| Source->GetAvatarActor() != ThreatActor.Get() || Target->GetAvatarActor() != ProtectorActor.Get()
		|| Context.GetOriginalInstigatorAbilitySystemComponent() != Source) { return false; }
	ExpectedContext = Context; return true;
}
bool USovProtectionInterceptReceipt::Matches(const FSovDamageResult& Result) const
{
	return ExpectedContext.IsValid() && Result.TransactionId.IsValid() && Result.EffectContext.Get() == ExpectedContext.Get()
		&& ThreatActor.IsValid() && ProtectorActor.IsValid() && ThreatASC.IsValid() && ProtectorASC.IsValid()
		&& ThreatASC->GetAvatarActor() == ThreatActor.Get() && ProtectorASC->GetAvatarActor() == ProtectorActor.Get()
		&& Result.SourceActor == ThreatActor.Get() && Result.TargetActor == ProtectorActor.Get();
}
void USovProtectionInterceptReceipt::ReceiveResult(const FSovDamageResult& Result)
{
	if (bDelivered || bConsumed || !Matches(Result) || Result.bPeriodicDamage || Result.bFromEchoAbility
		|| Result.BaseDamage <= KINDA_SMALL_NUMBER) { return; }
	const bool bAcceptedDefense = (Result.DefenseKind == ESovDefenseKind::Guard && Result.bGuarded)
		|| (Result.DefenseKind == ESovDefenseKind::Deflection && Result.bDeflected && Result.bPerfectDefense);
	const bool bAcceptedDamage = Result.AppliedHealthDamage > KINDA_SMALL_NUMBER || Result.AppliedShieldDamage > KINDA_SMALL_NUMBER;
	if (!bAcceptedDefense && !bAcceptedDamage) { return; }
	bDelivered = true; CommittedTransaction = Result.TransactionId;
	if (auto* Resonance = USovResonanceComponent::FindForActor(ProtectorActor->GetWorld(), ProtectorActor.Get()))
	{ Resonance->NotifyProtectionIntercept(this, Result); }
	if (auto* Generator = ProtectorActor->FindComponentByClass<USovTarrikEchoGenerationComponent>())
	{
		Generator->ConsumeProtectionIntercept(this, Result);
	}
}
bool USovProtectionInterceptReceipt::ConsumeForProtector(AActor* Protector, const FSovDamageResult& Result,
	AActor*& OutThreat, AActor*& OutProtected)
{
	OutThreat = nullptr; OutProtected = nullptr;
	if (bConsumed || !bDelivered || !Matches(Result) || Result.TransactionId != CommittedTransaction
		|| Protector != ProtectorActor.Get() || !Protector->HasAuthority() || !ProtectedActor.IsValid()) { return false; }
	bConsumed = true;
	OutThreat = ThreatActor.Get(); OutProtected = ProtectedActor.Get(); return true;
}
void USovProtectionInterceptReceipt::Disarm()
{
	bConsumed = true; ExpectedContext = FGameplayEffectContextHandle();
}

bool USovProtectionInterceptReceipt::MatchesCommittedForProtector(AActor* Protector,
	const FSovDamageResult& Result, AActor*& OutProtected) const
{
	OutProtected = nullptr;
	if (!bDelivered || !Matches(Result) || Result.TransactionId != CommittedTransaction
		|| Protector != ProtectorActor.Get() || !IsValid(Protector) || !Protector->HasAuthority() || !ProtectedActor.IsValid()) { return false; }
	OutProtected = ProtectedActor.Get(); return true;
}
