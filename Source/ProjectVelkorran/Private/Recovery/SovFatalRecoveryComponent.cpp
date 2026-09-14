// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Recovery/SovFatalRecoveryComponent.h"
#include "Recovery/SovRecoveryPolicy.h"
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Save/SovSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

USovGameplayEffect_RecoveryProtection::USovGameplayEffect_RecoveryProtection()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
}
USovFatalRecoveryComponent::USovFatalRecoveryComponent() { PrimaryComponentTick.bCanEverTick = false; }
ASovPlayerCharacterBase* USovFatalRecoveryComponent::Player() const { return Cast<ASovPlayerCharacterBase>(GetOwner()); }
bool USovFatalRecoveryComponent::IsCurrentContext(uint64 ExpectedEpoch, const UNarrativeAbilitySystemComponent* ASC,
	const ASovPlayerCharacterBase* P, const APlayerController* PC) const
{
	const auto* CampaignController = Cast<ASovPlayerController>(PC);
	return !bEndingPlay && Epoch == ExpectedEpoch && IsValid(ASC) && BoundASC == ASC && IsValid(P)
		&& GetOwner() == P && ASC->GetAvatarActor() == P && (!PC || (IsValid(PC) && PC->GetPawn() == P && P->GetController() == PC))
		&& (!CampaignController || CampaignController->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle);
}
void USovFatalRecoveryComponent::InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* ASC)
{
	if (bEndingPlay || (ASC == BoundASC && IsValid(ASC) && ASC->GetAvatarActor() == GetOwner())) { return; }
	const uint64 ExpectedEpoch = Epoch + 1;
	Unbind();
	// Removing an owned effect can call user code and establish a newer binding.
	if (Epoch != ExpectedEpoch || bEndingPlay || !IsValid(ASC) || !Player() || !Player()->HasAuthority() || ASC->GetAvatarActor() != GetOwner()) { return; }
	BoundASC = ASC;
	ASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
	ASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamage);
}
void USovFatalRecoveryComponent::Unbind()
{
	++Epoch;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DecisionTimer); GetWorld()->GetTimerManager().ClearTimer(RetryTimer); }
	UNarrativeAbilitySystemComponent* OldASC = BoundASC;
	const FActiveGameplayEffectHandle OldProtection = ProtectionHandle;
	const bool bReleaseBusy = bOwnFailureBusy;
	TWeakObjectPtr<APlayerController> OldController = LockedController;
	const bool bReleaseInput = bOwnInputLock;
	// Retire all ownership before any delegate from a removal can reenter initialization.
	ProtectionHandle.Invalidate(); BoundASC = nullptr; PendingEncounter = nullptr;
	LockedController.Reset(); bOwnInputLock = false; bOwnFailureBusy = false;
	State = ESovRecoveryState::Ready; bCanonicalCompanionFailure = false; bResolvingRevive = false;
	FatalTransaction.Invalidate(); bExcludedFatal = false;
	if (IsValid(OldASC))
	{
		OldASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath);
		OldASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamage);
		if (OldProtection.IsValid()) { OldASC->RemoveActiveGameplayEffect(OldProtection); }
		if (bReleaseBusy && IsValid(OldASC)) { OldASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1); }
	}
	if (bReleaseInput && OldController.IsValid())
	{ OldController->SetIgnoreMoveInput(false); OldController->SetIgnoreLookInput(false); }
}
void USovFatalRecoveryComponent::EndPlay(const EEndPlayReason::Type Reason) { bEndingPlay = true; Unbind(); Super::EndPlay(Reason); }
bool USovFatalRecoveryComponent::OwnsFatalRecovery() const
{
	const auto* P = Player();
	return !bEndingPlay && P && P->HasAuthority() && P->GetNetMode() == NM_Standalone && P->IsCharacterReady()
		&& Cast<ASovPlayerController>(P->GetController()) && IsValid(BoundASC)
		&& BoundASC->GetAvatarActor() == P && BoundASC->IsDead();
}
ASovEncounterDirector* USovFatalRecoveryComponent::FindActiveEncounter() const
{
	ASovEncounterDirector* Found = nullptr;
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (!It->IsEncounterCombatant(GetOwner()) || It->GetEncounterState() != ESovEncounterState::Active) { continue; }
		if (Found) { return nullptr; }
		Found = *It;
	}
	return Found;
}
bool USovFatalRecoveryComponent::RequestCompanionFailure(USovCompanionComponent* Source)
{
	ASovPlayerCharacterBase* P = Player();
	ASovPlayerController* PC = P ? Cast<ASovPlayerController>(P->GetController()) : nullptr;
	USovCampaignStateComponent* Campaign = PC ? PC->GetCampaignState() : nullptr;
	USovCampaignDefinition* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
	UNarrativeAbilitySystemComponent* ASC = BoundASC;
	AActor* Ally = IsValid(Source) ? Source->GetOwner() : nullptr;
	const auto* AllyASC = Ally ? Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ally)) : nullptr;
	if (!P || !P->HasAuthority() || P->GetNetMode() != NM_Standalone || !P->IsCharacterReady() || !P->IsAlive()
		|| !PC || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle || !IsValid(ASC)
		|| ASC->GetAvatarActor() != P || bCanonicalCompanionFailure || !IsValid(Ally) || Ally->GetWorld() != GetWorld()
		|| !AllyASC || !AllyASC->IsDead() || !Mission || !Mission->AllowedCompanionIds.Contains(Source->CompanionId)
		|| (!Cast<ASovProtagonistCompanionCharacter>(Ally) && !IsValid(Source->RequiredEncounter))) { return false; }
	const uint64 ExpectedEpoch = ++Epoch;
	bCanonicalCompanionFailure = true; PendingEncounter = FindActiveEncounter();
	if (!bOwnFailureBusy)
	{
		bOwnFailureBusy = true; ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1);
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	}
	PC->ReleaseHeldAbilityInputs();
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	ASC->CancelAllAbilities();
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	if (!bOwnInputLock) { bOwnInputLock = true; LockedController = PC; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); }
	if (IsValid(PendingEncounter)) { PendingEncounter->FailEncounter(); }
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	SetState(ESovRecoveryState::Retrying, TEXT("A required companion was defeated."));
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying) { return false; }
	GetWorld()->GetTimerManager().SetTimer(RetryTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedEpoch]() { Retry(ExpectedEpoch); }),
		static_cast<float>(SovRecoveryPolicy::RetryDelaySeconds), false);
	return true;
}
void USovFatalRecoveryComponent::HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead)
{
	if (bEndingPlay || Actor != GetOwner() || ASC != BoundASC || !IsValid(ASC) || ASC->GetAvatarActor() != Actor) { return; }
	ASovPlayerCharacterBase* P = Player();
	ASovPlayerController* PC = P ? Cast<ASovPlayerController>(P->GetController()) : nullptr;
	if (!bDead)
	{
		if (bResolvingRevive) { return; }
		const uint64 ExpectedEpoch = ++Epoch;
		GetWorld()->GetTimerManager().ClearTimer(DecisionTimer); GetWorld()->GetTimerManager().ClearTimer(RetryTimer);
		PendingEncounter = nullptr; FatalTransaction.Invalidate(); bCanonicalCompanionFailure = false;
		ReleaseInputLock();
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
		if (bOwnFailureBusy)
		{
			bOwnFailureBusy = false; ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1);
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
		}
		SetState(ESovRecoveryState::Ready); return;
	}
	if (!OwnsFatalRecovery()) { return; }
	bResolvingRevive = false; // A new fatal transaction cannot inherit an older restore's callback suppression.
	const uint64 ExpectedEpoch = ++Epoch;
	FatalTransaction.Invalidate(); bExcludedFatal = false; PendingEncounter = FindActiveEncounter();
	PC->ReleaseHeldAbilityInputs();
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || !OwnsFatalRecovery()) { return; }
	if (!bOwnInputLock) { LockedController = PC; bOwnInputLock = true; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); }
	SetState(ESovRecoveryState::ResolvingFatal);
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::ResolvingFatal || !OwnsFatalRecovery()) { return; }
	GetWorld()->GetTimerManager().SetTimer(DecisionTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedEpoch]() { ResolveFatal(ExpectedEpoch); }),
		static_cast<float>(SovRecoveryPolicy::DecisionDelaySeconds), false);
}
void USovFatalRecoveryComponent::HandleDamage(const FSovDamageResult& Result)
{
	if (State != ESovRecoveryState::ResolvingFatal || Result.TargetActor != GetOwner()
		|| !Result.bFatal || !Result.TransactionId.IsValid() || !OwnsFatalRecovery()) { return; }
	FatalTransaction = Result.TransactionId;
	// A canonical lethal hit and an environmental death must never be converted into rescue.
	bExcludedFatal |= Result.bCanonicalFatal || Result.DamageChannels.HasTagExact(FSovGameplayTags::Get().Damage_Channel_Environmental);
}
bool USovFatalRecoveryComponent::IsSafeRecoveryPosition(const ASovPlayerCharacterBase* P, const FVector& Position)
{
	if (!IsValid(P) || !P->GetWorld() || Position.ContainsNaN() || !P->GetCapsuleComponent() || !P->GetCharacterMovement()) { return false; }
	const UCapsuleComponent* Capsule = P->GetCapsuleComponent();
	if (ASovRecoveryExclusionVolume::ExcludesCapsule(P->GetWorld(), Position,
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight())) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SovRecoveryClearance), false, P);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	if (P->GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn, Shape, Params)) { return false; }
	FHitResult Floor;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Feet = Position - FVector(0, 0, HalfHeight - 4.f);
	if (!P->GetWorld()->LineTraceSingleByChannel(Floor, Feet, Feet - FVector(0, 0, 80.f), ECC_WorldStatic, Params)
		|| !P->GetCharacterMovement()->IsWalkable(Floor)) { return false; }
	return true;
}
bool USovFatalRecoveryComponent::ApplyProtection(float Seconds, bool bRescue)
{
	UNarrativeAbilitySystemComponent* ASC = BoundASC;
	ASovPlayerCharacterBase* P = Player();
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	const uint64 ExpectedEpoch = Epoch;
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || !FMath::IsFinite(Seconds) || Seconds <= 0.f) { return false; }
	const FActiveGameplayEffectHandle OldProtection = ProtectionHandle;
	ProtectionHandle.Invalidate();
	if (OldProtection.IsValid()) { ASC->RemoveActiveGameplayEffect(OldProtection); }
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_RecoveryProtection::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) { return false; }
	Spec.Data->SetDuration(Seconds, true);
	Spec.Data->DynamicGrantedTags.AddTag(FNarrativeGameplayTags::Get().State_Invulnerable);
	Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Invulnerable_Respawn);
	if (bRescue) { Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Recovery_Rescue); }
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC))
	{ if (Handle.IsValid() && IsValid(ASC)) { ASC->RemoveActiveGameplayEffect(Handle); } return false; }
	ProtectionHandle = Handle;
	return Handle.IsValid();
}
bool USovFatalRecoveryComponent::ProtectRestoredCheckpoint()
{
	ASovPlayerCharacterBase* P = Player();
	UNarrativeAbilitySystemComponent* ASC = BoundASC;
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	uint64 ExpectedEpoch = Epoch;
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || !P->IsAlive()
		|| !ApplyProtection(static_cast<float>(SovRecoveryPolicy::CheckpointProtectionSeconds), false)
		|| !IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
	if (State == ESovRecoveryState::Retrying || bCanonicalCompanionFailure)
	{
		ExpectedEpoch = ++Epoch; bCanonicalCompanionFailure = false; PendingEncounter = nullptr;
		if (bOwnFailureBusy)
		{
			bOwnFailureBusy = false; ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1);
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
		}
		ReleaseInputLock();
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return false; }
		SetState(ESovRecoveryState::Ready);
	}
	return IsCurrentContext(ExpectedEpoch, ASC, P, PC);
}
void USovFatalRecoveryComponent::ResolveFatal(uint64 ExpectedEpoch)
{
	ASovPlayerCharacterBase* P = Player();
	UNarrativeAbilitySystemComponent* ASC = BoundASC;
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::ResolvingFatal || !OwnsFatalRecovery()) { return; }
	ASovEncounterDirector* Encounter = PendingEncounter;
	const bool bActive = IsValid(Encounter) && Encounter->IsEncounterCombatant(P)
		&& Encounter->GetEncounterState() == ESovEncounterState::Active && Encounter->GetAttemptId().IsValid();
	const auto* Settings = UNarrativeGameUserSettings::GetSovSettings();
	if (SovRecoveryPolicy::CanRescue(true, bActive, bActive && Encounter->bAllowCompanionRescue,
		!Settings || Settings->IsCompanionRescueAllowed(), bActive && UsedRescueAttempt == Encounter->GetAttemptId(),
		FatalTransaction.IsValid(), bExcludedFatal, true, IsSafeRecoveryPosition(P, P->GetActorLocation())))
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			USovCompanionComponent* Companion = It->FindComponentByClass<USovCompanionComponent>();
			FString Reason;
			if (!Companion || !Companion->CanProvideRescue(P, Reason)) { continue; }
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
			FSovCombatResourceSnapshot Resources;
			if (!USovEncounterSnapshotLibrary::CaptureResources(ASC, Resources)) { break; }
			Resources.Health = static_cast<float>(SovRecoveryPolicy::RescueHealth(Resources.MaxHealth));
			Resources.Shield = 0.f; Resources.Stamina = Resources.MaxStamina * .5f; Resources.Poise = Resources.MaxPoise;
			if (!USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(ASC, Resources)) { break; }
			UsedRescueAttempt = Encounter->GetAttemptId();
			const FName CompanionId = Companion->CompanionId;
			const FName EncounterId = Encounter->EncounterId;
			if (!ApplyProtection(static_cast<float>(SovRecoveryPolicy::RescueProtectionSeconds), true))
			{ if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; } break; }
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
			// Revive callbacks are suppressed only for this owned restore. Do not let an old scope reset a new binding's state.
			bResolvingRevive = true;
			const bool bRestored = USovEncounterSnapshotLibrary::RestoreResources(ASC, Resources);
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
			bResolvingRevive = false;
			if (!bRestored || !P->IsAlive()) { break; }
			P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
			ReleaseInputLock(); PendingEncounter = nullptr;
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
			USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::CompanionRescue,
				CompanionId, EncounterId, Resources.Health, 0.f, true);
			SetState(ESovRecoveryState::Rescued); return;
		}
	}
	if (bActive && IsValid(Encounter)) { Encounter->FailEncounter(); }
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC)) { return; }
	// A restore that failed after Revive also requires retry, even though the pawn is now technically alive.
	SetState(ESovRecoveryState::Retrying);
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying) { return; }
	GetWorld()->GetTimerManager().SetTimer(RetryTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedEpoch]() { Retry(ExpectedEpoch); }),
		static_cast<float>(SovRecoveryPolicy::RetryDelaySeconds), false);
}
void USovFatalRecoveryComponent::Retry(uint64 ExpectedEpoch)
{
	ASovPlayerCharacterBase* P = Player();
	UNarrativeAbilitySystemComponent* ASC = BoundASC;
	ASovPlayerController* PC = P ? Cast<ASovPlayerController>(P->GetController()) : nullptr;
	if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying || !PC
		|| PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return; }
	FString Reason;
	if (IsValid(PendingEncounter) && PendingEncounter->GetEncounterState() == ESovEncounterState::Failed)
	{
		const bool bRetryStarted = PendingEncounter->RetryEncounter(Reason);
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying || bRetryStarted
			|| PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return; }
	}
	USovSaveSubsystem* Slots = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
	if (Slots)
	{
		const ESovSaveResult CheckpointResult = Slots->LoadSlot(ESovSaveSlotKind::Checkpoint, 0, Reason);
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying || CheckpointResult == ESovSaveResult::LoadStarted
			|| PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return; }
		FSovSaveSlotHeader Recovery;
		const bool bFound = Slots->FindRecoveryAutosave(Recovery);
		if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying) { return; }
		if (bFound)
		{
			const ESovSaveResult AutoResult = Slots->LoadSlot(ESovSaveSlotKind::Auto, Recovery.SlotIndex, Reason);
			if (!IsCurrentContext(ExpectedEpoch, ASC, P, PC) || State != ESovRecoveryState::Retrying || AutoResult == ESovSaveResult::LoadStarted
				|| PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return; }
		}
	}
	SetState(ESovRecoveryState::Failed, Reason.IsEmpty() ? TEXT("No valid checkpoint is available for recovery.") : Reason);
}
void USovFatalRecoveryComponent::ReleaseInputLock()
{
	const TWeakObjectPtr<APlayerController> PC = LockedController;
	const bool bRelease = bOwnInputLock;
	bOwnInputLock = false; LockedController.Reset();
	if (bRelease && PC.IsValid()) { PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); }
}
void USovFatalRecoveryComponent::SetState(ESovRecoveryState Value, const FString& Reason)
{ State = Value; OnRecoveryChanged.Broadcast(Value, Reason); }
