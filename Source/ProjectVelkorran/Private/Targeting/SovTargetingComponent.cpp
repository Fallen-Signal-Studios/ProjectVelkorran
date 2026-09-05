// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Targeting/SovTargetingComponent.h"
#include "Targeting/SovAimAssist.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "ArsenalStatics.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "NarrativeGameplayTags.h"
#include "GameFramework/SpringArmComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

USovTargetingComponent::USovTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}
void USovTargetingComponent::BeginPlay() { Super::BeginPlay(); ResolveController(); }
void USovTargetingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	if (Controller.IsValid()) { Controller->OnSemanticInputChanged.RemoveDynamic(this, &ThisClass::HandleSemanticInput); }
	const bool bLostTarget = bHasPublishedLock;
	LockedTarget.Reset(); Controller.Reset(); bHasPublishedLock = false;
	if (bLostTarget) { OnLockTargetChanged.Broadcast(nullptr, ESovLockLossReason::OwnerUnavailable); }
	Super::EndPlay(Reason);
}
bool USovTargetingComponent::ResolveController()
{
	if (bEndingPlay) { return false; }
	const APawn* Pawn = Cast<APawn>(GetOwner());
	ANarrativePlayerController* Current = Pawn ? Cast<ANarrativePlayerController>(Pawn->GetController()) : nullptr;
	if (Current && (!Current->IsLocalController() || Current->GetPawn() != Pawn)) { Current = nullptr; }
	if (Controller.Get() != Current)
	{
		if (Controller.IsValid()) { Controller->OnSemanticInputChanged.RemoveDynamic(this, &ThisClass::HandleSemanticInput); }
		const bool bLostPublishedTarget = bHasPublishedLock;
		Controller = Current; LockedTarget.Reset(); bHasPublishedLock = false; bWasAiming = false; OccludedFor = 0.f;
		if (Current) { Current->OnSemanticInputChanged.AddDynamic(this, &ThisClass::HandleSemanticInput); }
		if (bLostPublishedTarget) { OnLockTargetChanged.Broadcast(nullptr, ESovLockLossReason::OwnerUnavailable); }
		// A listener can start another possession during the loss notification.
		if (Controller.IsValid() && Controller->GetPawn() != GetOwner())
		{ Controller->OnSemanticInputChanged.RemoveDynamic(this, &ThisClass::HandleSemanticInput); Controller.Reset(); }
	}
	return Controller.IsValid();
}
bool USovTargetingComponent::CanControlCamera() const
{
	const ANarrativePlayerController* PC = Controller.Get();
	const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner());
	const UNarrativeAbilitySystemComponent* ASC = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
	return !bEndingPlay && PC && PC->GetPawn() == GetOwner() && PC->GetViewTarget() == GetOwner() && !PC->IsLookInputIgnored()
		&& !PC->IsMoveInputIgnored() && GetWorld() && !GetWorld()->IsPaused() && ASC && ASC->GetCharacterReadyEpoch() > 0
		&& !ASC->IsDead() && !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming);
}
bool USovTargetingComponent::HasLineOfSight(ANarrativeCharacter* Target) const
{
	if (!Controller.IsValid() || !IsValid(Target)) { return false; }
	FVector Origin; FRotator Rotation; Controller->GetPlayerViewPoint(Origin, Rotation);
	FVector Destination = Target->GetActorLocation(); Destination.Z += Target->GetSimpleCollisionHalfHeight() * .5f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovHardLockLOS), false, GetOwner());
	TArray<AActor*> Attached; GetOwner()->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Destination, ECC_Visibility, Query)) { return true; }
	const AActor* HitActor = Hit.GetActor();
	return HitActor == Target || (HitActor && HitActor->GetOwner() == Target);
}
bool USovTargetingComponent::HasNavigationRelationship(ANarrativeCharacter* Target) const
{
	if (!bRequireNavigationRelationship) { return true; }
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation || !IsValid(Target)) { return false; }
	const ANavigationData* NavData = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	FNavLocation From, To;
	if (!NavData || !Navigation->ProjectPointToNavigation(GetOwner()->GetActorLocation(), From, FVector(100.f, 100.f, 200.f), NavData)
		|| !Navigation->ProjectPointToNavigation(Target->GetActorLocation(), To, FVector(100.f, 100.f, 200.f), NavData)) { return false; }
	return Navigation->TestPathSync(FPathFindingQuery(Controller.Get(), *NavData, From.Location, To.Location));
}
bool USovTargetingComponent::IsValidTarget(ANarrativeCharacter* Target, bool bCheckLOS, bool bCheckNavigation, ESovLockLossReason& Reason) const
{
	Reason = ESovLockLossReason::InvalidTarget;
	const UNarrativeAbilitySystemComponent* ASC = IsValid(Target) ? Target->GetNarrativeAbilitySystemComponent() : nullptr;
	const float Health = ASC ? ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) : 0.f;
	if (!ASC || Target == GetOwner() || Target->IsHidden() || !Target->ActorHasTag(HardLockPermissionTag()) || ASC->IsDead()
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		|| !FMath::IsFinite(Health) || Health <= 0.f
		|| UArsenalStatics::GetAttitude(GetOwner(), Target) != ETeamAttitude::Hostile) { return false; }
	if (!FMath::IsFinite(MaximumLockDistance) || MaximumLockDistance < 100.f
		|| FVector::DistSquared(GetOwner()->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaximumLockDistance))
	{ Reason = ESovLockLossReason::Distance; return false; }
	if (bCheckLOS && !HasLineOfSight(Target)) { Reason = ESovLockLossReason::Occluded; return false; }
	if (bCheckNavigation && !HasNavigationRelationship(Target)) { Reason = ESovLockLossReason::Navigation; return false; }
	Reason = ESovLockLossReason::None; return true;
}
TArray<ANarrativeCharacter*> USovTargetingComponent::CollectTargets() const
{
	TArray<ANarrativeCharacter*> Candidates;
	if (!CanControlCamera() || !FMath::IsFinite(MaximumLockDistance) || MaximumLockDistance < 100.f) { return Candidates; }
	TArray<FOverlapResult> Hits;
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovHardLockCandidates), false, GetOwner());
	GetWorld()->OverlapMultiByObjectType(Hits, GetOwner()->GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(MaximumLockDistance), Query);
	FVector View; FRotator Rotation; Controller->GetPlayerViewPoint(View, Rotation);
	for (const FOverlapResult& Hit : Hits)
	{
		ANarrativeCharacter* Candidate = Cast<ANarrativeCharacter>(Hit.GetActor()); ESovLockLossReason Reason;
		if (!Candidates.Contains(Candidate) && IsValidTarget(Candidate, true, true, Reason)
			&& FVector::DotProduct((Candidate->GetActorLocation() - View).GetSafeNormal(), Rotation.Vector()) > .25f)
		{ Candidates.Add(Candidate); }
	}
	return Candidates;
}
void USovTargetingComponent::SetTarget(ANarrativeCharacter* Target, ESovLockLossReason Reason)
{
	if (bEndingPlay || (LockedTarget.Get() == Target && Target) || (!Target && !bHasPublishedLock)) { return; }
	LockedTarget = Target; bHasPublishedLock = Target != nullptr; OccludedFor = 0.f; NavigationElapsed = 0.f;
	if (!Target && Reason != ESovLockLossReason::Cancelled)
	{ USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::LockFailure, TEXT("HardLock"), NAME_None, static_cast<float>(Reason)); }
	OnLockTargetChanged.Broadcast(Target, Reason);
}
void USovTargetingComponent::ClearHardLock() { SetTarget(nullptr, ESovLockLossReason::Cancelled); }
bool USovTargetingComponent::ToggleHardLock()
{
	if (LockedTarget.IsValid()) { ClearHardLock(); return true; }
	if (!ResolveController() || !CanControlCamera()) { return false; }
	FVector Origin; FRotator Rotation; Controller->GetPlayerViewPoint(Origin, Rotation);
	ANarrativeCharacter* Best = nullptr; float BestScore = -1.f;
	for (ANarrativeCharacter* Candidate : CollectTargets())
	{
		const float Score = FVector::DotProduct((Candidate->GetActorLocation() - Origin).GetSafeNormal(), Rotation.Vector());
		if (Score > BestScore) { Best = Candidate; BestScore = Score; }
	}
	if (!Best) { USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::LockFailure, TEXT("HardLock.NoCandidate")); return false; }
	SetTarget(Best, ESovLockLossReason::None); return true;
}
bool USovTargetingComponent::CycleTarget(bool bRight)
{
	if (!LockedTarget.IsValid()) { return ToggleHardLock(); }
	if (!CanControlCamera()) { return false; }
	FVector Origin; FRotator Rotation; Controller->GetPlayerViewPoint(Origin, Rotation);
	const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
	const auto ScreenAngle = [&](const AActor* Target)
	{
		const FVector To = (Target->GetActorLocation() - Origin).GetSafeNormal();
		return FMath::Atan2(FVector::DotProduct(To, Right), FVector::DotProduct(To, Rotation.Vector()));
	};
	const float CurrentAngle = ScreenAngle(LockedTarget.Get());
	ANarrativeCharacter* Best = nullptr; float BestDelta = TNumericLimits<float>::Max();
	for (ANarrativeCharacter* Candidate : CollectTargets())
	{
		if (Candidate == LockedTarget.Get()) { continue; }
		const float Delta = (ScreenAngle(Candidate) - CurrentAngle) * (bRight ? 1.f : -1.f);
		if (Delta > KINDA_SMALL_NUMBER && Delta < BestDelta) { BestDelta = Delta; Best = Candidate; }
	}
	if (!Best) { return false; }
	SetTarget(Best, ESovLockLossReason::None); return true;
}
void USovTargetingComponent::RotateCameraToward(const FVector& Point, float Delta, float Strength)
{
	if (!CanControlCamera() || !FMath::IsFinite(Delta) || Delta <= 0.f || Point.ContainsNaN()) { return; }
	FVector Origin; FRotator View; Controller->GetPlayerViewPoint(Origin, View);
	const FRotator Desired = (Point - Origin).Rotation();
	FRotator Current = Controller->GetControlRotation();
	const float Limit = (FMath::IsFinite(MaximumCameraDegreesPerSecond) ? FMath::Clamp(MaximumCameraDegreesPerSecond, 1.f, 180.f) : 90.f) * Delta * FMath::Clamp(Strength, 0.f, 1.f);
	Current.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Current.Yaw, Desired.Yaw), -Limit, Limit);
	Current.Pitch += FMath::Clamp(FMath::FindDeltaAngleDegrees(Current.Pitch, Desired.Pitch), -Limit, Limit);
	Current.Roll = 0.f; Controller->SetControlRotation(Current);
}
void USovTargetingComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, Type, TickFunction);
	if (!ResolveController()) { return; }
	const auto* OwnerCharacter = Cast<ANarrativeCharacter>(GetOwner());
	const auto* OwnerASC = OwnerCharacter ? OwnerCharacter->GetNarrativeAbilitySystemComponent() : nullptr;
	const bool bAiming = OwnerASC && OwnerASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming);
	const bool bAimStarted = bAiming && !bWasAiming;
	bWasAiming = bAiming;
	if (bAimStarted) { TryAimSnap(); }
	if (bHasPublishedLock && !LockedTarget.IsValid()) { SetTarget(nullptr, ESovLockLossReason::InvalidTarget); }
	if (!CanControlCamera())
	{
		if (LockedTarget.IsValid()) { SetTarget(nullptr, ESovLockLossReason::Cinematic); }
		return;
	}
	// Existing Narrative spring arm remains collision authority. Authored composition cannot disable its safety test.
	if (USpringArmComponent* Arm = GetOwner()->FindComponentByClass<USpringArmComponent>()) { Arm->bDoCollisionTest = true; }
	const float ElapsedDelta = FMath::IsFinite(Delta) ? FMath::Max(Delta, 0.f) : 0.f;
	const float SafeDelta = FMath::Min(ElapsedDelta, .1f);
	if (LockedTarget.IsValid())
	{
		ESovLockLossReason Reason;
		NavigationElapsed += ElapsedDelta;
		if (!IsValidTarget(LockedTarget.Get(), false, NavigationElapsed >= .25f, Reason)) { SetTarget(nullptr, Reason); return; }
		if (NavigationElapsed >= .25f) { NavigationElapsed = 0.f; }
		OccludedFor = HasLineOfSight(LockedTarget.Get()) ? 0.f : OccludedFor + ElapsedDelta;
		if (OccludedFor > (FMath::IsFinite(OcclusionTimeoutSeconds) ? FMath::Clamp(OcclusionTimeoutSeconds, 0.f, 2.f) : .4f)) { SetTarget(nullptr, ESovLockLossReason::Occluded); return; }
		if (OccludedFor <= 0.f) { RotateCameraToward(LockedTarget->GetActorLocation() + FVector(0.f, 0.f, LockedTarget->GetSimpleCollisionHalfHeight() * .5f), SafeDelta, 1.f); }
		return;
	}
	const UNarrativeGameUserSettings* Settings = UNarrativeGameUserSettings::GetSovSettings();
	if (!Settings || Settings->GetAutoCameraStrength() <= 0.f
		|| GetWorld()->GetTimeSeconds() - Controller->GetLastManualLookTime() < 1.f) { return; }
	const FVector Velocity = GetOwner()->GetVelocity();
	if (Velocity.SizeSquared2D() > FMath::Square(100.f))
	{
		FVector Ahead = GetOwner()->GetActorLocation() + Velocity.GetSafeNormal2D() * 1000.f;
		FVector Origin; FRotator View; Controller->GetPlayerViewPoint(Origin, View);
		Ahead.Z = Origin.Z; RotateCameraToward(Ahead, SafeDelta, Settings->GetAutoCameraStrength());
	}
}
void USovTargetingComponent::HandleSemanticInput(FGameplayTag Tag, bool bPressed)
{
	if (!bPressed) { return; }
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (Tag == Tags.Input_ThreatFocus) { ToggleHardLock(); }
	else if (Tag == Tags.Input_CycleTargetLeft) { CycleTarget(false); }
	else if (Tag == Tags.Input_CycleTargetRight) { CycleTarget(true); }
}

void USovTargetingComponent::TryAimSnap()
{
	const auto* Settings = UNarrativeGameUserSettings::GetSovSettings();
	ANarrativePlayerController* PC = Controller.Get();
	const auto* Character = Cast<ANarrativeCharacter>(GetOwner());
	const auto* ASC = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
	if (bEndingPlay || !Settings || !Settings->UseAimSnap() || !PC || PC->GetPawn() != GetOwner()
		|| PC->GetViewTarget() != GetOwner() || PC->IsLookInputIgnored() || PC->IsMoveInputIgnored()
		|| !GetWorld() || GetWorld()->IsPaused() || !ASC || ASC->IsDead() || ASC->GetAvatarActor() != GetOwner()
		|| !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)) { return; }
	FVector Origin; FRotator View; PC->GetPlayerViewPoint(Origin, View);
	AActor* Target = nullptr; FVector Point;
	if (!SovAimAssist::FindVisibleTarget(GetOwner(), Origin, View.Vector(), 5000.f, 8.f, Target, Point)) { return; }
	const FRotator Desired = (Point - Origin).Rotation();
	const FQuat Current = FRotator(PC->GetControlRotation().Pitch, PC->GetControlRotation().Yaw, 0.f).Quaternion();
	const float Angle = FMath::RadiansToDegrees(Current.AngularDistance(Desired.Quaternion()));
	const float Fraction = Angle <= KINDA_SMALL_NUMBER ? 1.f : FMath::Min(1.f, 8.f / Angle);
	FRotator Result = FQuat::Slerp(Current, Desired.Quaternion(), Fraction).Rotator(); Result.Roll = 0.f;
	PC->SetControlRotation(Result);
}
