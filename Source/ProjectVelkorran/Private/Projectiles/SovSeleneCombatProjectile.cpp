// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Combat/SovSelenePayloadMath.h"
#include "Targeting/SovAimAssistPolicy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "Sovereign/SovEnvironmentDamage.h"

ASovSeleneCombatProjectile::ASovSeleneCombatProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PayloadRoot"));
	PresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PresentationMesh"));
	PresentationMesh->SetupAttachment(RootComponent);
	PresentationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ASovSeleneCombatProjectile::BeginPlay()
{
	Super::BeginPlay();
	PresentPhase();
}
ASovSeleneCombatProjectile* ASovSeleneCombatProjectile::SpawnNativePayload(
	TSubclassOf<ANarrativeProjectile> AuthoredClass, const FVector& Origin, const FSovSeleneProjectileParameters& Parameters)
{
	if (!SovSelenePayload::ValidSource(Parameters.Context) || Origin.ContainsNaN()) { return nullptr; }
	UClass* Class = AuthoredClass.Get();
	if (!Class || !Class->IsChildOf(StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract)) { Class = StaticClass(); }
	AActor* Source = Parameters.Context.SourceAvatar.Get();
	const FTransform Transform(Parameters.Direction.Rotation(), Origin);
	auto* Projectile = Source->GetWorld()->SpawnActorDeferred<ASovSeleneCombatProjectile>(
		Class, Transform, Source, Cast<APawn>(Source), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) { return nullptr; }
	if (!Projectile->InitializePayload(Parameters)) { Projectile->Destroy(); return nullptr; }
	Projectile->FinishSpawning(Transform);
	return IsValid(Projectile) && !Projectile->IsActorBeingDestroyed() ? Projectile : nullptr;
}
bool ASovSeleneCombatProjectile::InitializePayload(const FSovSeleneProjectileParameters& Parameters)
{
	const auto Positive = [](float Value) { return FMath::IsFinite(Value) && Value > 0.0f; };
	const auto NonNegative = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f; };
	if (bInitialized || !HasAuthority() || !SovSelenePayload::ValidSource(Parameters.Context)
		|| (Parameters.Mode != ESovSeleneProjectileMode::Stillpoint && Parameters.Mode != ESovSeleneProjectileMode::Wake
			&& Parameters.Mode != ESovSeleneProjectileMode::Dispatch)
		|| Parameters.Direction.ContainsNaN() || Parameters.Direction.IsNearlyZero()
		|| !Positive(Parameters.Speed) || !Positive(Parameters.Range) || !Positive(Parameters.Radius)
		|| !Positive(Parameters.ControlDuration) || !Positive(Parameters.MaximumLifetime)
		|| !Positive(Parameters.ReturnSpeed) || !Positive(Parameters.OutboundDuration)
		|| !NonNegative(Parameters.Fuse) || !NonNegative(Parameters.Damage) || !NonNegative(Parameters.Poise)
		|| !NonNegative(Parameters.DamagePerSecond) || !NonNegative(Parameters.RefreezeLockout)
		|| !NonNegative(Parameters.CenterlineWidth) || !NonNegative(Parameters.SteeringDegrees)
		|| !NonNegative(Parameters.GravityScale) || Parameters.GravityScale > 10.f
		|| !NonNegative(Parameters.ShatterBonusPoise) || Parameters.SteeringDegrees > 720.0f
		|| Parameters.Radius > 10000.0f || Parameters.Range > 10000.0f || Parameters.MaximumLifetime > 120.0f) { return false; }
	Tuning = Parameters;
	Tuning.Direction.Normalize();
	Mode = Tuning.Mode;
	ReleaseOrigin = GetActorLocation();
	Velocity = Tuning.Direction * Tuning.Speed;
	bInitialized = true;
	SetLifeSpan(Tuning.MaximumLifetime + 0.1f);
	return true;
}
void ASovSeleneCombatProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASovSeleneCombatProjectile, Mode);
	DOREPLIFETIME(ASovSeleneCombatProjectile, Phase);
}
void ASovSeleneCombatProjectile::OnRep_Phase()
{
	PresentPhase();
	ReceivePayloadPhaseChanged(Mode, Phase);
}
void ASovSeleneCombatProjectile::PresentPhase()
{
	if (GetNetMode() == NM_DedicatedServer) { return; }
	if (ActivePhaseNiagara)
	{
		ActivePhaseNiagara->Deactivate();
		ActivePhaseNiagara = nullptr;
	}
	UNiagaraSystem* System = nullptr;
	if (Phase == ESovSeleneProjectilePhase::Outbound) { System = FlightNiagaraSystem; }
	else if (Phase == ESovSeleneProjectilePhase::Field) { System = FieldNiagaraSystem; }
	else if (Phase == ESovSeleneProjectilePhase::Recalling) { System = RecallNiagaraSystem; }
	if (System && GetWorld())
	{
		ActivePhaseNiagara = UNiagaraFunctionLibrary::SpawnSystemAttached(System, RootComponent,
			NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
}
void ASovSeleneCombatProjectile::MulticastPayloadHit_Implementation(FVector_NetQuantize Point)
{
	if (GetNetMode() != NM_DedicatedServer && HitNiagaraSystem && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitNiagaraSystem, Point);
	}
}
void ASovSeleneCombatProjectile::SetPhase(ESovSeleneProjectilePhase NewPhase)
{
	Phase = NewPhase;
	ForceNetUpdate();
	OnRep_Phase();
}
bool ASovSeleneCombatProjectile::Recall()
{
	if (!HasAuthority() || !bInitialized || bFinished || Mode != ESovSeleneProjectileMode::Dispatch
		|| Phase != ESovSeleneProjectilePhase::Outbound) { return false; }
	SetPhase(ESovSeleneProjectilePhase::Recalling);
	return !bFinished && !IsActorBeingDestroyed();
}
void ASovSeleneCombatProjectile::Finish(bool bReturned)
{
	if (bFinished) { return; }
	bFinished = true;
	SetPhase(bReturned ? ESovSeleneProjectilePhase::Returned : ESovSeleneProjectilePhase::Expired);
	OnPayloadFinished.Broadcast(this, bReturned);
	Destroy();
}
void ASovSeleneCombatProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && !bFinished)
	{
		bFinished = true;
		OnPayloadFinished.Broadcast(this, false);
	}
	Super::EndPlay(Reason);
}
void ASovSeleneCombatProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bInitialized || bFinished || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) { return; }
	if (!SovSelenePayload::ValidSource(Tuning.Context)) { Finish(false); return; }
	// Bounded substeps keep fuse, steering and swept collision stable through hitches.
	float Remaining = FMath::Min(DeltaSeconds, 0.5f);
	const float UnsimulatedTime = FMath::Max(0.0f, DeltaSeconds - Remaining);
	Age += UnsimulatedTime; // Never slow fuse/recall/lifetime clocks to limit collision work.
	if (Phase == ESovSeleneProjectilePhase::Field) { FieldAge += UnsimulatedTime; }
	while (Remaining > KINDA_SMALL_NUMBER && !bFinished && !IsActorBeingDestroyed())
	{
		const float Step = FMath::Min(Remaining, 1.0f / 30.0f);
		Remaining -= Step;
		Age += Step;
		if (Age >= Tuning.MaximumLifetime) { Finish(false); break; }
		if (Mode == ESovSeleneProjectileMode::Stillpoint && Age >= Tuning.Fuse)
		{
			if (Phase != ESovSeleneProjectilePhase::Field)
			{
				Velocity = FVector::ZeroVector;
				SetPhase(ESovSeleneProjectilePhase::Field);
				if (bFinished || IsActorBeingDestroyed()) { break; }
			}
			FieldAge += Step;
			FieldQueryTime -= Step;
			if (FieldQueryTime <= 0.0f) { FieldQueryTime = 0.1f; ApplyField(); }
			if (FieldAge >= Tuning.ControlDuration) { Finish(false); }
			continue;
		}
		Advance(Step);
	}
}
void ASovSeleneCombatProjectile::Advance(float DeltaSeconds)
{
	const FVector Start = GetActorLocation();
	if (Mode == ESovSeleneProjectileMode::Dispatch)
	{
		if (Phase == ESovSeleneProjectilePhase::Outbound
			&& SovSelenePayloadMath::ShouldRecall(Age, Travelled, Tuning.OutboundDuration, Tuning.Range)) { Recall(); }
		if (bFinished || IsActorBeingDestroyed()) { return; }
		FVector AimOrigin, AimDirection;
		if (!SovSelenePayload::Aim(Tuning.Context, AimOrigin, AimDirection)) { Finish(false); return; }
		if (Phase == ESovSeleneProjectilePhase::Recalling)
		{
			if (FVector::DistSquared(Start, AimOrigin) <= FMath::Square(FMath::Max(60.0f, Tuning.ReturnSpeed * DeltaSeconds)))
			{
				Finish(true); return;
			}
			Velocity = (AimOrigin - Start).GetSafeNormal() * Tuning.ReturnSpeed;
		}
		else
		{
			const FRotator Steered = FMath::RInterpConstantTo(Velocity.Rotation(), AimDirection.Rotation(), DeltaSeconds, Tuning.SteeringDegrees);
			Velocity = Steered.Vector() * Tuning.Speed;
		}
	}
	FVector End = Start + Velocity * DeltaSeconds;
	if (Mode == ESovSeleneProjectileMode::Stillpoint)
	{
		const FVector Gravity(0., 0., GetWorld()->GetGravityZ() * Tuning.GravityScale);
		const auto EndPoint = SovAimAssistPolicy::PositionAtTime({Start.X, Start.Y, Start.Z},
			{Velocity.X, Velocity.Y, Velocity.Z}, {Gravity.X, Gravity.Y, Gravity.Z}, DeltaSeconds);
		End = FVector(EndPoint.X, EndPoint.Y, EndPoint.Z);
		Velocity += Gravity * DeltaSeconds;
	}
	if (Mode == ESovSeleneProjectileMode::Wake
		|| (Mode == ESovSeleneProjectileMode::Dispatch && Phase == ESovSeleneProjectilePhase::Outbound))
	{
		End = Start + Velocity.GetSafeNormal() * SovSelenePayloadMath::RemainingStep(Velocity.Size(), DeltaSeconds, Travelled, Tuning.Range);
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SelenePayloadSweep), false, this);
	SovSelenePayload::IgnoreSource(Query, Tuning.Context.SourceAvatar.Get());
	TArray<FHitResult> Hits;
	const float Radius = Mode == ESovSeleneProjectileMode::Wake ? Tuning.Radius : 18.0f;
	const FCollisionShape Shape = Mode == ESovSeleneProjectileMode::Wake
		? FCollisionShape::MakeBox(FVector(18.0f, Radius, 35.0f)) : FCollisionShape::MakeSphere(Radius);
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, Velocity.Rotation().Quaternion(),
		FCollisionObjectQueryParams::AllObjects, Shape, Query);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	// A side wall can overlap the wide target lane without blocking the wave's forward path.
	// Individual targets still need line of sight in HitTarget, so cover remains protective.
	TArray<FHitResult> CenterHits;
	if (Mode == ESovSeleneProjectileMode::Wake)
	{
		GetWorld()->SweepMultiByObjectType(CenterHits, Start, End, Velocity.Rotation().Quaternion(),
			FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(18.0f), Query);
	}
	const TArray<FHitResult>& BlockingHits = Mode == ESovSeleneProjectileMode::Wake ? CenterHits : Hits;
	float WallTime = 1.0f;
	bool bWall = false;
	for (const FHitResult& Hit : BlockingHits)
	{
		AActor* Actor = Hit.GetActor();
		if (IsValid(Actor) && !Actor->IsA<ANarrativeProjectile>() && !SovSelenePayload::ResolveTarget(Actor)
			&& Hit.Component.IsValid() && Hit.Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
		{
			WallTime = FMath::Min(WallTime, Hit.Time); bWall = true;
		}
	}
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.Time > WallTime || bFinished || IsActorBeingDestroyed() || !SovSelenePayload::ValidSource(Tuning.Context)) { break; }
		if (Mode != ESovSeleneProjectileMode::Stillpoint) { HitTarget(SovSelenePayload::ResolveTarget(Hit.GetActor()), Hit, Start); }
	}
	// Authored scenery stops this segment like any wall and receives one base packet per projectile.
	if (bWall && Mode != ESovSeleneProjectileMode::Stillpoint && !bFinished && !IsActorBeingDestroyed()
		&& SovSelenePayload::ValidSource(Tuning.Context))
	{
		const FHitResult* WallHit = BlockingHits.FindByPredicate([WallTime](const FHitResult& Hit)
			{ return Hit.Time == WallTime && IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<USovEnvironmentDamageable>(); });
		if (WallHit && !SceneryTargets.Contains(TWeakObjectPtr<AActor>(WallHit->GetActor())))
		{
			SceneryTargets.Add(TWeakObjectPtr<AActor>(WallHit->GetActor()));
			SovEnvironmentDamage::ApplyPoint(Tuning.Context.SourceAvatar.Get(), *WallHit, Tuning.Damage);
		}
	}
	if (bFinished || IsActorBeingDestroyed()) { return; }
	const FVector Destination = FMath::Lerp(Start, End, WallTime);
	Travelled += FVector::Distance(Start, Destination);
	SetActorLocation(Destination);
	SetActorRotation(Velocity.Rotation());
	if (bWall)
	{
		if (Mode == ESovSeleneProjectileMode::Stillpoint) { Velocity = FVector::ZeroVector; }
		else if (Mode == ESovSeleneProjectileMode::Wake || Phase == ESovSeleneProjectilePhase::Recalling) { Finish(false); }
		else { Recall(); }
	}
	else if (Mode == ESovSeleneProjectileMode::Wake && Travelled >= Tuning.Range - KINDA_SMALL_NUMBER) { Finish(false); }
}
void ASovSeleneCombatProjectile::ApplyField()
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SeleneStasisField), false, this);
	SovSelenePayload::IgnoreSource(Query, Tuning.Context.SourceAvatar.Get());
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(Tuning.Radius), Query);
	for (const FOverlapResult& Hit : Hits)
	{
		if (bFinished || IsActorBeingDestroyed() || !SovSelenePayload::ValidSource(Tuning.Context)) { break; }
		AActor* Target = SovSelenePayload::ResolveTarget(Hit.GetActor());
		if (!SovSelenePayload::EligibleTarget(Tuning.Context, Target) || OutboundTargets.Contains(Target)
			|| FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(Tuning.Radius)
			|| !SovSelenePayload::Visible(Tuning.Context, GetActorLocation(), Target, Target->GetActorLocation())) { continue; }
		OutboundTargets.Add(Target); // Reserve before any synchronous gameplay delegate.
		if (Tuning.Damage > 0.0f) { SovSelenePayload::Damage(Tuning.Context, Target, nullptr, Tuning.Damage, 0.0f); }
		if (bFinished || IsActorBeingDestroyed() || !SovSelenePayload::EligibleTarget(Tuning.Context, Target)) { continue; }
		const float RemainingDuration = FMath::Max(0.01f, Tuning.ControlDuration - FieldAge);
		const bool bFrozen = SovSelenePayload::Control(Tuning.Context, Target, RemainingDuration, Tuning.RefreezeLockout, true);
		if (bFinished || IsActorBeingDestroyed()) { return; }
		SovSelenePayload::FrostDOT(Tuning.Context, Target, Tuning.DamagePerSecond * (bFrozen ? 1.0f : 0.5f), RemainingDuration, bFrozen);
		ReceivePayloadHit(Target, false, bFrozen);
		if (!IsActorBeingDestroyed() && IsValid(Target)) { MulticastPayloadHit(Target->GetActorLocation()); }
	}
}
void ASovSeleneCombatProjectile::HitTarget(AActor* Target, const FHitResult& Hit, const FVector& SegmentStart)
{
	const bool bReturn = Phase == ESovSeleneProjectilePhase::Recalling;
	auto& Ledger = bReturn ? ReturnTargets : OutboundTargets;
	const FVector HitPoint = Hit.bStartPenetrating && IsValid(Target) ? Target->GetActorLocation() : FVector(Hit.ImpactPoint);
	if (!SovSelenePayload::EligibleTarget(Tuning.Context, Target) || Ledger.Contains(Target)
		|| !SovSelenePayload::Visible(Tuning.Context, SegmentStart, Target, HitPoint)) { return; }
	Ledger.Add(Target);
	const auto& GameplayTags = FSovGameplayTags::Get();
	const bool bWasChilled = SovSelenePayload::HasStatus(Target, GameplayTags.State_Status_Chilled);
	const bool bWasFrozen = SovSelenePayload::HasStatus(Target, GameplayTags.State_Status_Frozen);
	const bool bWake = Mode == ESovSeleneProjectileMode::Wake;
	// Shatter augments this one transaction, never reapplies its base health damage.
	const float Poise = Tuning.Poise + (!bWake && bReturn && (bWasChilled || bWasFrozen) ? Tuning.ShatterBonusPoise : 0.0f);
	const bool bAccepted = SovSelenePayload::Damage(Tuning.Context, Target, &Hit, Tuning.Damage, Poise, 1.0f, bWake);
	if (bFinished || IsActorBeingDestroyed() || !SovSelenePayload::EligibleTarget(Tuning.Context, Target)) { return; }
	bool bFrozen = false;
	if (bWake && bAccepted)
	{
		const FVector Offset = Target->GetActorLocation() - ReleaseOrigin;
		const FVector HorizontalForward = FVector(Tuning.Direction.X, Tuning.Direction.Y, 0.0f).GetSafeNormal();
		bFrozen = SovSelenePayload::Control(Tuning.Context, Target, Tuning.ControlDuration, Tuning.RefreezeLockout,
			bWasChilled || SovSelenePayloadMath::IsInCenterline(Offset.X, Offset.Y,
				HorizontalForward.X, HorizontalForward.Y, Tuning.CenterlineWidth));
		if (bFinished || IsActorBeingDestroyed()) { return; }
		SovSelenePayload::FrostDOT(Tuning.Context, Target, Tuning.DamagePerSecond, Tuning.ControlDuration, false);
	}
	ReceivePayloadHit(Target, bReturn, bFrozen);
	if (!IsActorBeingDestroyed()) { MulticastPayloadHit(HitPoint); }
}
