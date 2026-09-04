// Copyright Narrative Tools. All Rights Reserved.
#include "GAS/NarrativeAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AI/NarrativeNPCController.h"
#include "Character/NarrativeCharacterVisual.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeCombatAbility.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace NarrativeBotCombat
{
	UNarrativeCombatAbility* ResolveCombatAbility(const FGameplayAbilitySpec& Spec)
	{
		return Cast<UNarrativeCombatAbility>(Spec.GetPrimaryInstance()
			? Spec.GetPrimaryInstance() : Spec.Ability.Get());
	}

	bool IsSelectable(const FGameplayAbilitySpec& Spec, const FGameplayTag InputFilter, const ANarrativeCharacter* Source)
	{
		const UNarrativeCombatAbility* Ability = ResolveCombatAbility(Spec);
		if (Spec.PendingRemove || !IsValid(Ability) || !Ability->bBotSelectionEnabled
			|| !Ability->InputTag.IsValid()) { return false; }
		if (InputFilter.IsValid() && Ability->InputTag != InputFilter
			&& !Spec.GetDynamicSpecSourceTags().HasTagExact(InputFilter)) { return false; }
		// Holstered item grants must not influence positioning or fire stale attacks.
		const UWeaponItem* Weapon = Cast<UWeaponItem>(Spec.SourceObject.Get());
		return !Weapon || (Weapon->IsWielded() && IsValid(Source)
			&& (Source->GetWeapon(true) == Weapon || Source->GetWeapon(false) == Weapon));
	}

	bool IsAlive(const UAbilitySystemComponent* ASC)
	{
		if (!IsValid(ASC)) { return false; }
		const UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
		return (!NarrativeASC || !NarrativeASC->IsDead())
			&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
			&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
				|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER);
	}

	bool HasLineOfSight(ANarrativeCharacter* Source, AActor* Target)
	{
		if (!IsValid(Source) || !IsValid(Target) || !Source->GetWorld()) { return false; }
		FVector Origin;
		FRotator Rotation;
		Source->GetActorEyesViewPoint(Origin, Rotation);
		if (Origin.ContainsNaN()) { return false; }
		FCollisionQueryParams Query(SCENE_QUERY_STAT(NarrativeBotAttackLOS), false, Source);
		TArray<AActor*> Attached;
		Source->GetAttachedActors(Attached, true, true);
		Query.AddIgnoredActors(Attached);
		if (ANarrativeCharacterVisual* Visual = Source->GetCharacterVisual()) { Query.AddIgnoredActor(Visual); }
		const auto Visible = [Source, Target, Origin, Query](const FVector& Point)
		{
			FHitResult Hit;
			if (!Source->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Point, ECC_Visibility, Query)) { return true; }
			AActor* HitActor = Hit.GetActor();
			const INarrativeCharacterOwner* Provider = Cast<INarrativeCharacterOwner>(HitActor);
			return HitActor == Target || (IsValid(HitActor) && HitActor->IsOwnedBy(Target))
				|| (Provider && Provider->GetNarrativeCharacter() == Target);
		};
		if (Visible(Target->GetActorLocation())) { return true; }
		if (const ACharacter* Character = Cast<ACharacter>(Target))
		{
			FVector Eyes;
			Character->GetActorEyesViewPoint(Eyes, Rotation);
			if (!Eyes.ContainsNaN() && FVector::DistSquared(Eyes, Target->GetActorLocation()) <= FMath::Square(400.f))
			{
				return Visible(Eyes);
			}
		}
		return false;
	}

	bool HasTokenOpportunity(ANarrativeNPCController* Controller, UNarrativeAbilitySystemComponent* Target)
	{
		if (!IsValid(Controller) || !IsValid(Target) || !Controller->CanAcquireAttackTokenFor(Target)) { return false; }
		if (Target->HasAttackTokenFor(Controller) || Target->GetAvailableAttackTokens() > 0) { return true; }
		// Match Narrative's existing steal rules without mutating the token budget.
		for (const FAttackToken& Token : Target->GrantedAttackTokens)
		{
			float StealScore = 0.f;
			if (Target->ShouldImmediatelyStealToken(Token) || Target->CanStealToken(Controller, Token, StealScore)) { return true; }
		}
		return false;
	}

	bool RankBefore(const FNarrativeBotAttackCandidate& A, const FNarrativeBotAttackCandidate& B)
	{
		if (A.Priority != B.Priority) { return A.Priority > B.Priority; }
		if (A.LastUsedSerial != B.LastUsedSerial) { return A.LastUsedSerial < B.LastUsedSerial; }
		const FString APath = GetPathNameSafe(A.AbilityClass.Get());
		const FString BPath = GetPathNameSafe(B.AbilityClass.Get());
		return APath != BPath ? APath < BPath : A.GrantOrder < B.GrantOrder;
	}
}

bool UNarrativeAbilitySystemComponent::IsBotCombatContextValid(AActor* Target, const bool bDuringOwnedAttack) const
{
	const ANarrativeCharacter* Source = Cast<ANarrativeCharacter>(GetAvatarActor());
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Source) || Source->IsPlayerControlled()
		|| Source->IsActorBeingDestroyed() || !IsValid(Target) || Target == Source
		|| Target->IsActorBeingDestroyed() || Target->GetWorld() != GetWorld()
		|| !AbilityActorInfo.IsValid() || AbilityActorInfo->AvatarActor.Get() != Source
		|| Source->GetAbilitySystemComponent() != this || !NarrativeBotCombat::IsAlive(this)) { return false; }
	const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	const INarrativeTeamAgentInterface* Team = Cast<INarrativeTeamAgentInterface>(Source);
	if (!NarrativeBotCombat::IsAlive(TargetASC) || !Team
		|| Team->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile) { return false; }
	const FNarrativeGameplayTags& N = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& S = FSovGameplayTags::Get();
	FGameplayTagContainer Blocked;
	if (!bDuringOwnedAttack)
	{
		Blocked.AddTag(N.State_Busy);
		Blocked.AddTag(N.State_Weapon_IsFiring);
	}
	Blocked.AddTag(N.State_Weapon_Equipping);
	Blocked.AddTag(N.State_Interacting);
	Blocked.AddTag(N.State_SequencerControlled);
	Blocked.AddTag(N.State_Movement_Ragdoll);
	Blocked.AddTag(S.State_Poise_Broken);
	Blocked.AddTag(S.State_Guard_Broken);
	Blocked.AddTag(S.State_Status_DeviceDisabled);
	Blocked.AddTag(S.State_Status_Frozen);
	return !HasAnyMatchingGameplayTags(Blocked);
}

TArray<FNarrativeBotAttackCandidate> UNarrativeAbilitySystemComponent::GetBotAttackCandidates(
	AActor* Target, const FGameplayTag InputFilter)
{
	TArray<FNarrativeBotAttackCandidate> Candidates;
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities()) { Handles.Add(Spec.Handle); }
	}
	const TWeakObjectPtr<AActor> OriginalAvatar = GetAvatarActor();
	const TWeakObjectPtr<AActor> OriginalTarget = Target;
	ANarrativeCharacter* Source = Cast<ANarrativeCharacter>(OriginalAvatar.Get());
	const bool bContextValid = IsBotCombatContextValid(Target);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float Distance = IsValid(Source) && IsValid(Target)
		? FVector::Distance(Source->GetActorLocation(), Target->GetActorLocation()) : 0.f;
	const bool bLOS = bContextValid && NarrativeBotCombat::HasLineOfSight(Source, Target);
	ANarrativeNPCController* Controller = Source ? Cast<ANarrativeNPCController>(Source->GetController()) : nullptr;
	UNarrativeAbilitySystemComponent* TargetASC = Cast<UNarrativeAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
	for (int32 Index = 0; Index < Handles.Num(); ++Index)
	{
		FGameplayAbilitySpec* LiveSpec = FindAbilitySpecFromHandle(Handles[Index]);
		if (!LiveSpec || !NarrativeBotCombat::IsSelectable(*LiveSpec, InputFilter, Source)) { continue; }
		// Blueprint metadata and CanActivate callbacks may grant/remove specs. Retain
		// a value snapshot; never hold a pointer into ActivatableAbilities across them.
		const FGameplayAbilitySpec Spec = *LiveSpec;
		UNarrativeCombatAbility* Ability = NarrativeBotCombat::ResolveCombatAbility(Spec);
		FNarrativeBotAttackCandidate Candidate;
		Candidate.Handle = Spec.Handle;
		Candidate.AbilityClass = Ability->GetClass();
		Candidate.InputTag = Ability->InputTag;
		Candidate.MinimumRange = Ability->GetBotAttackMinimumRange();
		Candidate.MaximumRange = Ability->GetBotAttackMaximumRange();
		Candidate.PreferredRange = Ability->GetBotAttackRange();
		Candidate.Frequency = Ability->GetBotAttackFrequency();
		Candidate.Priority = Ability->BotSelectionPriority;
		Candidate.bRequiresAttackToken = Ability->RequiresBotAttackToken();
		Candidate.bManagesAttackToken = Ability->ManagesBotAttackToken();
		Candidate.LastUsedSerial = BotAttackLastUsed.FindRef(Spec.Handle);
		Candidate.GrantOrder = Index;
		if (!FMath::IsFinite(Candidate.MinimumRange) || !FMath::IsFinite(Candidate.MaximumRange)
			|| Candidate.MinimumRange < 0.f || Candidate.MaximumRange <= Candidate.MinimumRange
			|| !FMath::IsFinite(Candidate.PreferredRange) || !FMath::IsFinite(Candidate.Frequency)
			|| !FMath::IsFinite(Candidate.Priority) || Candidate.Frequency < 0.f) { continue; }
		Candidate.PreferredRange = FMath::Clamp(Candidate.PreferredRange, Candidate.MinimumRange, Candidate.MaximumRange);
		Candidate.bInRange = IsValid(Target) && FMath::IsFinite(Distance)
			&& Distance >= Candidate.MinimumRange && Distance <= Candidate.MaximumRange;
		Candidate.bHasLineOfSight = !Ability->bBotRequiresLineOfSight || bLOS;
		const bool bCadenceReady = Now + KINDA_SMALL_NUMBER >= BotAttackNextAllowedTimes.FindRef(Spec.Handle);
		// Movement descriptors survive temporary unavailability. Never query an
		// active spec as a new attack, or bypass its native cooldown/authorization.
		Candidate.bCanActivate = bContextValid && !Spec.IsActive() && bCadenceReady
			&& Ability->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get(), nullptr, nullptr, &Candidate.FailureTags);
		if (!OriginalAvatar.IsValid() || OriginalAvatar.Get() != GetAvatarActor()
			|| (Target && !OriginalTarget.IsValid())) { return {}; }
		LiveSpec = FindAbilitySpecFromHandle(Spec.Handle);
		if (!LiveSpec || LiveSpec->PendingRemove || !IsValid(Ability)) { continue; }
		const bool bTokenReady = !Candidate.bRequiresAttackToken
			|| NarrativeBotCombat::HasTokenOpportunity(Controller, TargetASC);
		if (!bCadenceReady) { Candidate.FailureTags.AddTag(FNarrativeGameplayTags::Get().Ability_ActivateFail_Cooldown); }
		Candidate.bAvailable = Candidate.bCanActivate && Candidate.bInRange && Candidate.bHasLineOfSight && bTokenReady;
		Candidates.Add(MoveTemp(Candidate));
	}
	Candidates.Sort(NarrativeBotCombat::RankBefore);
	return Candidates;
}

bool UNarrativeAbilitySystemComponent::SelectBotAttack(AActor* Target, const FGameplayTag InputFilter,
	FNarrativeBotAttackCandidate& OutCandidate)
{
	OutCandidate = FNarrativeBotAttackCandidate();
	for (const FNarrativeBotAttackCandidate& Candidate : GetBotAttackCandidates(Target, InputFilter))
	{
		if (Candidate.bAvailable) { OutCandidate = Candidate; return true; }
	}
	return false;
}

bool UNarrativeAbilitySystemComponent::TryActivateBestBotAttack(AActor* Target, const FGameplayTag InputFilter,
	FNarrativeBotAttackCandidate& OutCandidate)
{
	OutCandidate = FNarrativeBotAttackCandidate();
	if (bBotAttackActivationInProgress || !IsBotCombatContextValid(Target)) { return false; }
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActor()))
	{
		if (AAIController* Controller = Cast<AAIController>(Pawn->GetController())) { Controller->SetFocus(Target); }
	}
	for (const FNarrativeBotAttackCandidate& Candidate : GetBotAttackCandidates(Target, InputFilter))
	{
		if (Candidate.bAvailable && TryActivateBotAttack(Target, Candidate.Handle))
		{
			OutCandidate = Candidate;
			return true;
		}
		if (!IsBotCombatContextValid(Target)) { break; }
	}
	return false;
}

bool UNarrativeAbilitySystemComponent::TryActivateBotAttack(AActor* Target, const FGameplayAbilitySpecHandle Handle)
{
	if (bBotAttackActivationInProgress || !IsBotCombatContextValid(Target)) { return false; }
	TGuardValue<bool> ActivationGuard(bBotAttackActivationInProgress, true);
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActor()))
	{
		if (AAIController* Controller = Cast<AAIController>(Pawn->GetController())) { Controller->SetFocus(Target); }
	}
	FNarrativeBotAttackCandidate Candidate;
	bool bFound = false;
	for (const FNarrativeBotAttackCandidate& Item : GetBotAttackCandidates(Target, FGameplayTag()))
	{
		if (Item.Handle == Handle && Item.bAvailable) { Candidate = Item; bFound = true; break; }
	}
	if (!bFound || !IsBotCombatContextValid(Target)) { return false; }
	if (!BotAttackEndedDelegate.IsValid())
	{
		BotAttackEndedDelegate = OnAbilityEnded.AddUObject(this, &ThisClass::HandleBotAttackEnded);
	}
	const TWeakObjectPtr<AActor> OriginalAvatar = GetAvatarActor();
	if (Candidate.bRequiresAttackToken && !Candidate.bManagesAttackToken)
	{
		ANarrativeCharacter* Source = Cast<ANarrativeCharacter>(GetAvatarActor());
		ANarrativeNPCController* Controller = Source ? Cast<ANarrativeNPCController>(Source->GetController()) : nullptr;
		UNarrativeAbilitySystemComponent* TargetASC = Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
		FNarrativeBotAttackLease Lease;
		if (!IsValid(Controller) || !IsValid(TargetASC)
			|| !Controller->TryAcquireAttackTokenFor(TargetASC, Lease.Serial, Lease.bNewlyAcquired)) { return false; }
		Lease.Controller = Controller;
		Lease.Target = TargetASC;
		BotAttackLeases.Add(Handle, Lease);
	}
	// Re-evaluate target geometry and live source-item state after token callbacks.
	// The reservation we just acquired intentionally makes bAvailable false, so
	// inspect the other gates individually rather than trying to acquire it twice.
	bool bStillEligible = false;
	for (const FNarrativeBotAttackCandidate& Rechecked : GetBotAttackCandidates(Target, FGameplayTag()))
	{
		if (Rechecked.Handle == Handle)
		{
			bStillEligible = Rechecked.bCanActivate && Rechecked.bInRange && Rechecked.bHasLineOfSight;
			break;
		}
	}
	// Claiming a token may synchronously fire controller callbacks. Recheck ownership
	// and the live spec before activation. GAS itself rechecks native costs/cooldown.
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!bStillEligible || OriginalAvatar.Get() != GetAvatarActor() || !IsBotCombatContextValid(Target)
		|| !Spec || Spec->PendingRemove || Spec->IsActive())
	{
		ReleaseBotAttackLease(Handle);
		return false;
	}
	Spec->InputPressed = true;
	const bool bActivated = TryActivateAbility(Handle, false);
	if (FGameplayAbilitySpec* CurrentSpec = FindAbilitySpecFromHandle(Handle))
	{
		CurrentSpec->InputPressed = false;
		if (CurrentSpec->IsActive())
		{
			// A selected attack is one input tap. Notify only this spec so legacy
			// WaitInputRelease abilities terminate without firing other input peers.
			const TArray<UGameplayAbility*> Instances = CurrentSpec->GetAbilityInstances();
			const FGameplayAbilityActivationInfo Info = Instances.IsEmpty()
				? CurrentSpec->ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
			AbilitySpecInputReleased(*CurrentSpec);
			if (FindAbilitySpecFromHandle(Handle))
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Handle, Info.GetActivationPredictionKey());
			}
		}
	}
	if (!bActivated)
	{
		ReleaseBotAttackLease(Handle);
		return false;
	}
	++BotAttackSelectionSerial;
	BotAttackLastUsed.Add(Handle, BotAttackSelectionSerial);
	BotAttackNextAllowedTimes.Add(Handle, (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0)
		+ FMath::Max(Candidate.Frequency, 0.05f));
	// Instant abilities may end inside TryActivateAbility; their end delegate has
	// already released the lease. Removal during activation is equally safe.
	const FGameplayAbilitySpec* ActiveSpec = FindAbilitySpecFromHandle(Handle);
	if (!ActiveSpec || !ActiveSpec->IsActive()) { ReleaseBotAttackLease(Handle); }
	return true;
}

void UNarrativeAbilitySystemComponent::HandleBotAttackEnded(const FAbilityEndedData& Data)
{
	ReleaseBotAttackLease(Data.AbilitySpecHandle);
}

void UNarrativeAbilitySystemComponent::ReleaseBotAttackLease(const FGameplayAbilitySpecHandle Handle)
{
	FNarrativeBotAttackLease Lease;
	if (!BotAttackLeases.RemoveAndCopyValue(Handle, Lease)) { return; }
	// Remove ownership before callbacks. A stale/double end cannot return a new lease.
	if (Lease.Controller.IsValid() && Lease.Target.IsValid()
		&& Lease.Controller->IsAttackTokenLeaseCurrent(Lease.Serial, Lease.Target.Get()))
	{
		Lease.Controller->ReleaseAttackTokenLease(Lease.Serial, Lease.bNewlyAcquired);
	}
}

void UNarrativeAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	BotAttackLeases.GetKeys(Handles);
	for (const FGameplayAbilitySpecHandle Handle : Handles) { ReleaseBotAttackLease(Handle); }
	OnAbilityEnded.Remove(BotAttackEndedDelegate);
	BotAttackEndedDelegate.Reset();
	BotAttackNextAllowedTimes.Reset();
	BotAttackLastUsed.Reset();
	Super::EndPlay(EndPlayReason);
}

float UNarrativeAbilitySystemComponent::GetBotCombatMovementRange(AActor* Target, const FGameplayTag InputFilter)
{
	const TArray<FNarrativeBotAttackCandidate> Candidates = GetBotAttackCandidates(Target, InputFilter);
	for (const FNarrativeBotAttackCandidate& Candidate : Candidates)
	{
		if (Candidate.bAvailable) { return Candidate.PreferredRange; }
	}
	const AActor* Source = GetAvatarActor();
	const float Distance = IsValid(Source) && IsValid(Target)
		? FVector::Distance(Source->GetActorLocation(), Target->GetActorLocation()) : 0.f;
	const FNarrativeBotAttackCandidate* Closest = nullptr;
	float BestGap = TNumericLimits<float>::Max();
	for (const FNarrativeBotAttackCandidate& Candidate : Candidates)
	{
		const float Gap = FMath::Max(Candidate.MinimumRange - Distance, FMath::Max(Distance - Candidate.MaximumRange, 0.f));
		if (Gap < BestGap) { BestGap = Gap; Closest = &Candidate; }
	}
	return Closest ? Closest->PreferredRange : 250.f;
}

float UNarrativeAbilitySystemComponent::GetBotAttackRange(const FGameplayTag InputTag)
{
	// Legacy movement callers retain their input filter but not the old 1 cm
	// failure mode whenever a cooldown, command order or state temporarily blocks.
	float Range = 0.f;
	for (const FNarrativeBotAttackCandidate& Candidate : GetBotAttackCandidates(nullptr, InputTag))
	{
		Range = FMath::Max(Range, Candidate.PreferredRange);
	}
	return Range > KINDA_SMALL_NUMBER ? Range : 250.f;
}

float UNarrativeAbilitySystemComponent::GetBotAttackFrequency(const FGameplayTag InputTag)
{
	const APawn* Source = Cast<APawn>(GetAvatarActor());
	const AAIController* Controller = Source ? Cast<AAIController>(Source->GetController()) : nullptr;
	const TArray<FNarrativeBotAttackCandidate> Candidates = GetBotAttackCandidates(
		Controller ? Controller->GetFocusActor() : nullptr, InputTag);
	for (const FNarrativeBotAttackCandidate& Candidate : Candidates)
	{
		if (Candidate.bAvailable) { return FMath::Max(Candidate.Frequency, 0.05f); }
	}
	return Candidates.IsEmpty() ? 1.f : FMath::Max(Candidates[0].Frequency, 0.05f);
}

bool UNarrativeAbilitySystemComponent::IsBotAttackExecutionValid(AActor* Target, const FGameplayAbilitySpecHandle Handle) const
{
	if (!IsBotCombatContextValid(Target, true)) { return false; }
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->IsActive() || !NarrativeBotCombat::IsSelectable(*Spec, FGameplayTag(), Cast<ANarrativeCharacter>(GetAvatarActor()))) { return false; }
	if (const FNarrativeBotAttackLease* Lease = BotAttackLeases.Find(Handle))
	{
		return Lease->Controller.IsValid() && Lease->Target.IsValid()
			&& Lease->Controller->GetPawn() == GetAvatarActor()
			&& Lease->Target->GetAvatarActor() == Target
			&& Lease->Controller->IsAttackTokenLeaseCurrent(Lease->Serial, Lease->Target.Get());
	}
	return true;
}
