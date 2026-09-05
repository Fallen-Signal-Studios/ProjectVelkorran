// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Targeting/SovAimAssist.h"
#include "Targeting/SovAimAssistPolicy.h"
#include "ArsenalStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/PlayerController.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

namespace
{
	bool Visible(AActor* Shooter, AActor* Target, const FVector& Origin, const FVector& Point)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAimAssistLOS), false, Shooter);
		TArray<AActor*> Attached; Shooter->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
		FHitResult Hit;
		return !Shooter->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Point, ECC_Visibility, Query)
			|| Hit.GetActor() == Target || (Hit.GetActor() && Hit.GetActor()->GetOwner() == Target);
	}
}
bool SovAimAssist::FindVisibleTarget(AActor* Shooter, const FVector& Origin, const FVector& Direction,
	float Range, float ConeDegrees, AActor*& OutTarget, FVector& OutPoint)
{
	OutTarget = nullptr; OutPoint = FVector::ZeroVector;
	const APawn* Pawn = Cast<APawn>(Shooter);
	const auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!IsValid(Shooter) || !Shooter->GetWorld() || !PC || !PC->IsLocalController() || PC->GetPawn() != Shooter
		|| Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero() || !FMath::IsFinite(Range)
		|| !FMath::IsFinite(ConeDegrees) || Range <= 0.f || ConeDegrees <= 0.f || ConeDegrees > 15.f) { return false; }
	Range = FMath::Min(Range, 5000.f);
	const FVector Forward = Direction.GetSafeNormal();
	float BestDot = FMath::Cos(FMath::DegreesToRadians(ConeDegrees));
	double BestDistance = TNumericLimits<double>::Max();
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAimAssistCandidates), false, Shooter);
	TArray<FOverlapResult> Hits;
	Shooter->GetWorld()->OverlapMultiByObjectType(Hits, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Range), Query);
	for (const FOverlapResult& Hit : Hits)
	{
		auto* Candidate = Cast<ANarrativeCharacter>(Hit.GetActor());
		const auto* ASC = Candidate ? Candidate->GetNarrativeAbilitySystemComponent() : nullptr;
		const float Health = ASC ? ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) : 0.f;
		if (!ASC || Candidate == Shooter || Candidate->IsHidden() || ASC->GetAvatarActor() != Candidate || ASC->IsDead()
			|| !FMath::IsFinite(Health) || Health <= 0.f
			|| UArsenalStatics::GetAttitude(Shooter, Candidate) != ETeamAttitude::Hostile) { continue; }
		const FVector Point = Candidate->GetActorLocation() + FVector(0.f, 0.f, Candidate->GetSimpleCollisionHalfHeight() * .3f);
		const FVector Offset = Point - Origin;
		const double Distance = Offset.SizeSquared();
		const float Dot = FVector::DotProduct(Forward, Offset.GetSafeNormal());
		if (Distance > FMath::Square(Range) || Dot < BestDot
			|| (FMath::IsNearlyEqual(Dot, BestDot) && Distance >= BestDistance) || !Visible(Shooter, Candidate, Origin, Point)) { continue; }
		BestDot = Dot; BestDistance = Distance; OutTarget = Candidate; OutPoint = Point;
	}
	return IsValid(OutTarget);
}
bool SovAimAssist::GetProjectileLead(AActor* Shooter, const FVector& Muzzle, const FVector& InitialDirection,
	float Speed, float Range, FVector& OutDirection)
{
	OutDirection = InitialDirection;
	const auto* Settings = UNarrativeGameUserSettings::GetSovSettings();
	if (!Settings || !Settings->UseProjectileLead()) { return false; }
	AActor* Target = nullptr; FVector TargetPoint;
	if (!FindVisibleTarget(Shooter, Muzzle, InitialDirection, Range,
		static_cast<float>(SovAimAssistPolicy::MaximumCorrectionDegrees), Target, TargetPoint)) { return false; }
	const FVector Relative = TargetPoint - Muzzle;
	const FVector Velocity = Target->GetVelocity();
	double Time = 0.;
	if (Velocity.ContainsNaN() || !SovAimAssistPolicy::SolveIntercept(Relative.SizeSquared(),
		FVector::DotProduct(Relative, Velocity), Velocity.SizeSquared(), Speed, Time)) { return false; }
	const FVector Predicted = TargetPoint + Velocity * Time;
	const FVector Direction = (Predicted - Muzzle).GetSafeNormal();
	if (Direction.IsNearlyZero() || FVector::DotProduct(Direction, InitialDirection.GetSafeNormal())
		< FMath::Cos(FMath::DegreesToRadians(SovAimAssistPolicy::MaximumCorrectionDegrees))
		|| !Visible(Shooter, Target, Muzzle, Predicted)) { return false; }
	OutDirection = Direction; return true;
}
