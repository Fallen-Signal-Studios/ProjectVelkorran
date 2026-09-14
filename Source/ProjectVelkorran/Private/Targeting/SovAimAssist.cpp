// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Targeting/SovAimAssist.h"
#include "Targeting/SovAimAssistPolicy.h"
#include "ArsenalStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
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
	// Any player's possessed shooter qualifies, not only this machine's local controller: projectile lead is resolved
	// by authority-side payload code, which for a remote player runs where that player's controller is not local.
	if (!IsValid(Shooter) || !Shooter->GetWorld() || !PC || PC->GetPawn() != Shooter
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
	FProjectileLeadRequest Request;
	Request.AimDirection = InitialDirection;
	Request.InitialVelocity = InitialDirection.GetSafeNormal() * Speed;
	Request.Range = Range;
	FVector Velocity;
	if (!GetBallisticProjectileLead(Shooter, Muzzle, Request, Velocity)) { return false; }
	OutDirection = Velocity.GetSafeNormal();
	return true;
}
bool SovAimAssist::GetBallisticProjectileLead(AActor* Shooter, const FVector& Muzzle,
	const FProjectileLeadRequest& Request, FVector& OutVelocity)
{
	OutVelocity = Request.InitialVelocity;
	const auto* Settings = UNarrativeGameUserSettings::GetSovPlayerSettings(Shooter);
	if (!Settings || !Settings->UseProjectileLead()) { return false; }
	const float Speed = Request.InitialVelocity.Size();
	if (Request.InitialVelocity.ContainsNaN() || Request.Gravity.ContainsNaN() || !FMath::IsFinite(Speed) || Speed <= 0.f
		|| !FMath::IsFinite(Request.MaximumFlightSeconds) || Request.MaximumFlightSeconds <= 0.f
		|| !FMath::IsFinite(Request.MaximumCorrectionDegrees) || Request.MaximumCorrectionDegrees <= 0.f
		|| !FMath::IsFinite(Request.CollisionRadius) || Request.CollisionRadius < 0.f || Request.CollisionRadius > 200.f
		|| !FMath::IsFinite(Request.MaximumFlightSpeed) || Request.MaximumFlightSpeed < 0.f
		|| Request.CollisionChannel < ECC_WorldStatic || Request.CollisionChannel >= ECC_MAX) { return false; }
	const float Cone = FMath::Min(Request.MaximumCorrectionDegrees, static_cast<float>(SovAimAssistPolicy::MaximumCorrectionDegrees));
	AActor* Target = nullptr; FVector TargetPoint;
	if (!FindVisibleTarget(Shooter, Muzzle, Request.AimDirection, Request.Range, Cone, Target, TargetPoint)) { return false; }
	const FVector Relative = TargetPoint - Muzzle;
	const FVector Velocity = Target->GetVelocity();
	const auto ToPolicy = [](const FVector& Value) { return SovAimAssistPolicy::Vector{Value.X, Value.Y, Value.Z}; };
	SovAimAssistPolicy::Vector Solution;
	double Time = 0.;
	if (!SovAimAssistPolicy::SolveBallisticIntercept(ToPolicy(Relative), ToPolicy(Velocity), ToPolicy(Request.Gravity), Speed,
		FMath::Min(static_cast<double>(Request.MaximumFlightSeconds), SovAimAssistPolicy::MaximumLeadSeconds),
		Request.bPreferHighArc, Solution, Time)) { return false; }
	const FVector AssistedVelocity(Solution.X, Solution.Y, Solution.Z);
	const FVector Predicted = TargetPoint + Velocity * Time;
	if (FVector::DistSquared(Muzzle, Predicted) > FMath::Square(FMath::Min(Request.Range, 5000.f))
		|| FVector::DotProduct(AssistedVelocity.GetSafeNormal(), Request.InitialVelocity.GetSafeNormal())
		< FMath::Cos(FMath::DegreesToRadians(Cone))
		|| !Visible(Shooter, Target, Muzzle, Predicted)) { return false; }
	// A speed-limited movement component would no longer follow the solved parabola.
	// Speed squared is convex in time, so testing both endpoints bounds the whole flight.
	if (Request.MaximumFlightSpeed > 0.f && (Speed > Request.MaximumFlightSpeed + .01f
		|| (AssistedVelocity + Request.Gravity * Time).Size() > Request.MaximumFlightSpeed + .01f)) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAimAssistArc), false, Shooter);
	TArray<AActor*> Attached; Shooter->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	// The intended target can be hit before the predicted point. No other collision body is ignored.
	Query.AddIgnoredActor(Target); Attached.Reset(); Target->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Time * 60.), 1, 36);
	const double Step = Time / Steps;
	// Inflate by the parabola/chord sagitta so every point on the curved body path is covered.
	const float Radius = Request.CollisionRadius + static_cast<float>(Request.Gravity.Size() * Step * Step / 8.);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Radius, .01f));
	FVector Start = Muzzle;
	for (int32 I = 1; I <= Steps; ++I)
	{
		const auto EndPoint = SovAimAssistPolicy::PositionAtTime(ToPolicy(Muzzle), Solution, ToPolicy(Request.Gravity), I * Step);
		const FVector End(EndPoint.X, EndPoint.Y, EndPoint.Z);
		FHitResult Hit;
		if (Shooter->GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Request.CollisionChannel,
			Shape, Query, FCollisionResponseParams(Request.CollisionResponses))) { return false; }
		Start = End;
	}
	OutVelocity = AssistedVelocity; return true;
}
