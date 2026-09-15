// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Sovereign/SovEnvironmentDamage.h"
#include "AbilitySystemGlobals.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
bool Admits(const AActor* Source, AActor* Target)
{
	return IsValid(Source) && Source->HasAuthority() && IsValid(Target) && Target != Source
		&& !Target->IsActorBeingDestroyed() && Target->Implements<USovEnvironmentDamageable>()
		// Anything carrying an ability system is a GAS target, never scenery.
		&& !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
}
}

float SovEnvironmentDamage::ApplyPoint(AActor* Source, const FHitResult& Hit, float Damage)
{
	AActor* Target = Hit.GetActor();
	if (!Admits(Source, Target) || !FMath::IsFinite(Damage) || Damage <= 0.f) { return 0.f; }
	FVector Direction = FVector(Hit.ImpactPoint) - FVector(Hit.TraceStart);
	if (Direction.ContainsNaN() || Direction.IsNearlyZero()) { Direction = FVector(Hit.ImpactPoint) - Source->GetActorLocation(); }
	const FPointDamageEvent Event(Damage, Hit, Direction.GetSafeNormal(), nullptr);
	return Target->TakeDamage(Damage, Event, Source->GetInstigatorController(), Source);
}

int32 SovEnvironmentDamage::ApplyRadial(AActor* Source, const FVector& Origin, float Radius, float Damage,
	float MinimumFraction, bool bRequireLineOfSight, const FCollisionQueryParams& Query, TArray<AActor*>* OutDamaged)
{
	UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
	if (!World || !Source->HasAuthority() || Origin.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.f
		|| !FMath::IsFinite(Damage) || Damage <= 0.f || !FMath::IsFinite(MinimumFraction)) { return 0; }
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Query);

	struct FCandidate { TWeakObjectPtr<AActor> Actor; TWeakObjectPtr<UPrimitiveComponent> Component; FVector Point; };
	TArray<FCandidate> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Admits(Source, Target) || !IsValid(Component)
			|| Candidates.ContainsByPredicate([Target](const FCandidate& Candidate) { return Candidate.Actor.Get() == Target; })) { continue; }
		const FVector Point = Component->Bounds.GetBox().GetClosestPointTo(Origin);
		if (bRequireLineOfSight)
		{
			FCollisionQueryParams Sight = Query;
			Sight.AddIgnoredActor(Target);
			FHitResult Blocker;
			if (World->LineTraceSingleByChannel(Blocker, Origin, Point, ECC_Visibility, Sight)) { continue; }
		}
		Candidates.Add({Target, Component, Point});
	}

	const float Minimum = FMath::Clamp(MinimumFraction, 0.f, 1.f);
	int32 Damaged = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		AActor* Target = Candidate.Actor.Get();
		if (!Admits(Source, Target) || !Candidate.Component.IsValid()) { continue; }
		const float Alpha = FMath::Clamp(FVector::Distance(Origin, Candidate.Point) / Radius, 0.f, 1.f);
		const float Amount = Damage * FMath::Lerp(1.f, Minimum, Alpha);
		FVector Direction = Candidate.Point - Origin;
		Direction = Direction.IsNearlyZero() ? FVector::UpVector : Direction.GetSafeNormal();
		FHitResult Hit(Target, Candidate.Component.Get(), Candidate.Point, -Direction);
		Hit.TraceStart = Origin;
		Hit.TraceEnd = Candidate.Point;
		Hit.bBlockingHit = true;
		const FPointDamageEvent Event(Amount, Hit, Direction, nullptr);
		if (Target->TakeDamage(Amount, Event, Source->GetInstigatorController(), Source) > 0.f)
		{
			++Damaged;
			if (OutDamaged) { OutDamaged->Add(Target); }
		}
	}
	return Damaged;
}
