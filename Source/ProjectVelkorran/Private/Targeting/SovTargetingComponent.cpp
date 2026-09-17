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
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Components/SovStatusComponent.h"
#include "Framework/SovPlayerController.h"
#include "NarrativeGameplayTags.h"
#include "GameFramework/SpringArmComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

AActor* USovTargetingComponent::GetLockedTarget() const { return LockedTarget.Get(); }

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
	LockedTarget.Reset(); Controller.Reset(); bHasPublishedLock = false; FramingSuspension = ESovFramingSuspension::None;
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
		FramingSuspension = ESovFramingSuspension::None;
		if (Current) { Current->OnSemanticInputChanged.AddDynamic(this, &ThisClass::HandleSemanticInput); }
		if (bLostPublishedTarget) { OnLockTargetChanged.Broadcast(nullptr, ESovLockLossReason::OwnerUnavailable); }
		// A listener can start another possession during the loss notification.
		if (Controller.IsValid() && Controller->GetPawn() != GetOwner())
		{ Controller->OnSemanticInputChanged.RemoveDynamic(this, &ThisClass::HandleSemanticInput); Controller.Reset(); }
	}
	return Controller.IsValid();
}
bool USovTargetingComponent::CanHoldFocus() const
{
	const ANarrativePlayerController* PC = Controller.Get();
	const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner());
	const UNarrativeAbilitySystemComponent* ASC = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
	return !bEndingPlay && PC && PC->GetPawn() == GetOwner() && PC->GetViewTarget() == GetOwner() && !PC->IsLookInputIgnored()
		&& !PC->IsMoveInputIgnored() && GetWorld() && !GetWorld()->IsPaused() && ASC && ASC->GetCharacterReadyEpoch() > 0
		&& !ASC->IsDead() && !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}
bool USovTargetingComponent::IsAiming() const
{
	const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner());
	const UNarrativeAbilitySystemComponent* ASC = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming);
}
// Aiming is a framing handover, not a lock loss: the weapon owns the camera while the focus is retained.
bool USovTargetingComponent::CanControlCamera() const { return CanHoldFocus() && !IsAiming(); }
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
	if (!CanHoldFocus() || !FMath::IsFinite(MaximumLockDistance) || MaximumLockDistance < 100.f) { return Candidates; }
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
	FramingSuspension = Target && IsAiming() ? ESovFramingSuspension::Aiming : ESovFramingSuspension::None;
	if (!Target && Reason != ESovLockLossReason::Cancelled)
	{ USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::LockFailure, TEXT("HardLock"), NAME_None, static_cast<float>(Reason)); }
	OnLockTargetChanged.Broadcast(Target, Reason);
}
void USovTargetingComponent::ClearHardLock() { SetTarget(nullptr, ESovLockLossReason::Cancelled); }
bool USovTargetingComponent::ToggleHardLock()
{
	if (LockedTarget.IsValid()) { ClearHardLock(); return true; }
	if (!ResolveController() || !CanHoldFocus()) { return false; }
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
	if (!CanHoldFocus()) { return false; }
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
	if (!ResolveController()) { FramingSuspension = ESovFramingSuspension::None; return; }
	const bool bAiming = IsAiming();
	const bool bAimStarted = bAiming && !bWasAiming;
	bWasAiming = bAiming;
	if (bAimStarted) { TryAimSnap(); }
	if (bHasPublishedLock && !LockedTarget.IsValid()) { SetTarget(nullptr, ESovLockLossReason::InvalidTarget); }
	if (!CanHoldFocus())
	{
		FramingSuspension = ESovFramingSuspension::None;
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
		FramingSuspension = bAiming ? ESovFramingSuspension::Aiming
			: OccludedFor > 0.f ? ESovFramingSuspension::Occluded : ESovFramingSuspension::None;
		if (FramingSuspension == ESovFramingSuspension::None)
		{ RotateCameraToward(LockedTarget->GetActorLocation() + FVector(0.f, 0.f, LockedTarget->GetSimpleCollisionHalfHeight() * .5f), SafeDelta, 1.f); }
		return;
	}
	// Assisted framing never competes with the weapon's own aim handling.
	FramingSuspension = bAiming ? ESovFramingSuspension::Aiming : ESovFramingSuspension::None;
	if (bAiming) { return; }
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
	else if (Tag == Tags.Input_Designate) { FString Reason; DesignateFocus(Reason); }
}

USovCompanionComponent* USovTargetingComponent::ResolveCompanionCommands() const
{
	const auto* PC = Cast<ASovPlayerController>(Controller.Get());
	const auto* Companions = PC ? PC->GetConvergenceCompanionState() : nullptr;
	auto* Companion = Companions ? Companions->GetActiveCompanion() : nullptr;
	return IsValid(Companion) ? Companion->GetCompanionComponent() : nullptr;
}

ANarrativeCharacter* USovTargetingComponent::FindAllyUnderReticle() const
{
	// A defend order names a person, so the protagonist must be looking at one: this is a deliberate
	// reticle selection, not the proximity sweep hostiles get. The companion itself is never the subject.
	if (!CanHoldFocus() || !FMath::IsFinite(MaximumLockDistance) || MaximumLockDistance < 100.f) { return nullptr; }
	const USovCompanionComponent* Commands = ResolveCompanionCommands();
	const AActor* CompanionActor = Commands ? Commands->GetOwner() : nullptr;
	TArray<FOverlapResult> Hits;
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovDefendCandidates), false, GetOwner());
	GetWorld()->OverlapMultiByObjectType(Hits, GetOwner()->GetActorLocation(), FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(MaximumLockDistance), Query);
	FVector View; FRotator Rotation; Controller->GetPlayerViewPoint(View, Rotation);
	const float Tightness = FMath::IsFinite(DefendReticleTightness) ? FMath::Clamp(DefendReticleTightness, .5f, 1.f) : .93f;
	ANarrativeCharacter* Best = nullptr; float BestScore = Tightness;
	for (const FOverlapResult& Hit : Hits)
	{
		ANarrativeCharacter* Candidate = Cast<ANarrativeCharacter>(Hit.GetActor());
		const auto* ASC = IsValid(Candidate) ? Candidate->GetNarrativeAbilitySystemComponent() : nullptr;
		if (!ASC || Candidate == GetOwner() || Candidate == CompanionActor || Candidate->IsHidden() || ASC->IsDead()
			|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f
			|| UArsenalStatics::GetAttitude(GetOwner(), Candidate) != ETeamAttitude::Friendly
			|| !HasLineOfSight(Candidate)) { continue; }
		const float Score = FVector::DotProduct((Candidate->GetActorLocation() - View).GetSafeNormal(), Rotation.Vector());
		if (Score > BestScore) { BestScore = Score; Best = Candidate; }
	}
	return Best;
}

ESovDesignationResult USovTargetingComponent::DesignateFocus(FString& Reason)
{
	Reason.Reset();
	// Designation writes a target-owned reward window, so it belongs to the authority that owns damage.
	if (!ResolveController() || !CanHoldFocus() || !GetOwner()->HasAuthority())
	{ Reason = TEXT("The protagonist cannot designate a target right now."); return ESovDesignationResult::Refused; }
	auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	USovCompanionComponent* Commands = ResolveCompanionCommands();
	// Designating without a focus acquires one first: one press is the whole gesture.
	if (!LockedTarget.IsValid()) { ToggleHardLock(); }
	if (ANarrativeCharacter* Threat = LockedTarget.Get())
	{
		auto* Status = Threat->FindComponentByClass<USovStatusComponent>();
		if (!Status) { Reason = TEXT("That threat carries no status owner, so it cannot hold a designation."); return ESovDesignationResult::Refused; }
		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		const auto* OwnerASC = Player ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
		// Tarrik designates a command target for the companion; Selene marks a priority target. Each
		// protagonist's own Echo component reads its window from the target at the killing hit.
		const bool bCommandTarget = OwnerASC && OwnerASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik);
		const FGameplayTag Request = bCommandTarget ? Tags.Status_Apply_CommandTarget : Tags.Status_Apply_Mark;
		const ESovStatusApplicationResult Applied = Status->ApplyStatusByTag(Request, GetOwner());
		if (Applied != ESovStatusApplicationResult::Applied && Applied != ESovStatusApplicationResult::Refreshed)
		{ Reason = TEXT("The threat refused the designation."); return ESovDesignationResult::Refused; }
		DesignatedTarget = Threat;
		// The window stands on its own. A companion that cannot take the order does not undo it.
		if (Commands && Player) { FString Unused; Commands->RequestCommand(Player, ESovCompanionCommand::FocusTarget, Threat, Unused); }
		else { Reason = TEXT("Designated, but no companion is available to take the order."); }
		return bCommandTarget ? ESovDesignationResult::CommandTarget : ESovDesignationResult::Marked;
	}
	if (!Commands || !Player) { Reason = TEXT("No companion is available to take a command."); return ESovDesignationResult::Refused; }
	ANarrativeCharacter* Person = FindAllyUnderReticle();
	if (!Commands->RequestCommand(Player, ESovCompanionCommand::DefendPerson, Person ? Cast<AActor>(Person) : Cast<AActor>(Player), Reason))
	{ return ESovDesignationResult::Refused; }
	DesignatedTarget = Person ? Cast<AActor>(Person) : Cast<AActor>(Player);
	return ESovDesignationResult::Defended;
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
	AActor* Target = nullptr; FVector Point; ESovLockLossReason FocusReason;
	if (LockedTarget.IsValid() && IsValidTarget(LockedTarget.Get(), true, false, FocusReason))
	{ Point = LockedTarget->GetActorLocation() + FVector(0.f, 0.f, LockedTarget->GetSimpleCollisionHalfHeight() * .5f); }
	else if (!SovAimAssist::FindVisibleTarget(GetOwner(), Origin, View.Vector(), 5000.f, 8.f, Target, Point)) { return; }
	const FRotator Desired = (Point - Origin).Rotation();
	const FQuat Current = FRotator(PC->GetControlRotation().Pitch, PC->GetControlRotation().Yaw, 0.f).Quaternion();
	const float Angle = FMath::RadiansToDegrees(Current.AngularDistance(Desired.Quaternion()));
	const float Fraction = Angle <= KINDA_SMALL_NUMBER ? 1.f : FMath::Min(1.f, 8.f / Angle);
	FRotator Result = FQuat::Slerp(Current, Desired.Quaternion(), Fraction).Rotator(); Result.Roll = 0.f;
	PC->SetControlRotation(Result);
}
