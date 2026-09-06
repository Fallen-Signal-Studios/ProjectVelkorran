// Copyright Fallen Signal Studios. All Rights Reserved.

#include "AI/BTTask_SovDominionHandlerCommandHound.h"

#include "AIController.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/SovGameplayAbility_DominionHandler.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/SovDominionHandler.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovDominionHandlerBT, Log, All);

namespace
{
	bool IsNativeCommandCooldownFailure(
		const FGameplayTagContainer& FailureTags)
	{
		const FGameplayTag CooldownFailureTag =
			FNarrativeGameplayTags::Get().Ability_ActivateFail_Cooldown;
		return CooldownFailureTag.IsValid()
			&& FailureTags.HasTagExact(CooldownFailureTag);
	}
}

UBTTask_SovDominionHandlerCommandHound::
	UBTTask_SovDominionHandlerCommandHound(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Command Linked Dominion Hound");
	bCreateNodeInstance = true;
	bIgnoreRestartSelf = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

bool UBTTask_SovDominionHandlerCommandHound::
	IsExactCommandAbilityDefinition(
		const UGameplayAbility* AbilityDefinition)
{
	if (!IsValid(AbilityDefinition)
		|| !AbilityDefinition->IsA<
			USovGameplayAbility_DominionHandlerCommandHound>())
	{
		return false;
	}

	const FGameplayTag CommandIdentity =
		FSovGameplayTags::Get().Ability_NPC_DominionHandler_CommandHound;
	return CommandIdentity.IsValid()
		&& AbilityDefinition->GetAssetTags().HasTagExact(CommandIdentity);
}

ESovDominionHandlerCommandSpecSelection
	UBTTask_SovDominionHandlerCommandHound::SelectExactCommandAbilitySpec(
		const TConstArrayView<FGameplayAbilitySpec> AbilitySpecs,
		FGameplayAbilitySpecHandle& OutHandle,
		UGameplayAbility*& OutAbilitySource)
{
	OutHandle = FGameplayAbilitySpecHandle();
	OutAbilitySource = nullptr;

	const FGameplayTag CommandIdentity =
		FSovGameplayTags::Get().Ability_NPC_DominionHandler_CommandHound;
	if (!CommandIdentity.IsValid())
	{
		return ESovDominionHandlerCommandSpecSelection::IdentityCollision;
	}

	int32 ExactMatchCount = 0;
	bool bExactMatchIsUnavailable = false;
	bool bFoundIdentityCollision = false;
	for (const FGameplayAbilitySpec& Spec : AbilitySpecs)
	{
		const UGameplayAbility* AbilityDefinition = Spec.Ability;
		if (!IsValid(AbilityDefinition))
		{
			continue;
		}

		const bool bMatchesCommandClass = AbilityDefinition->IsA<
			USovGameplayAbility_DominionHandlerCommandHound>();
		const bool bMatchesCommandIdentity =
			AbilityDefinition->GetAssetTags().HasTagExact(CommandIdentity);
		if (bMatchesCommandClass != bMatchesCommandIdentity)
		{
			bFoundIdentityCollision = true;
			continue;
		}
		if (!bMatchesCommandClass)
		{
			continue;
		}

		++ExactMatchCount;
		bExactMatchIsUnavailable = Spec.IsActive()
			|| Spec.PendingRemove
			|| Spec.RemoveAfterActivation;
		OutHandle = Spec.Handle;
		OutAbilitySource = Spec.GetPrimaryInstance()
			? Spec.GetPrimaryInstance()
			: Spec.Ability.Get();
	}

	if (bFoundIdentityCollision)
	{
		OutHandle = FGameplayAbilitySpecHandle();
		OutAbilitySource = nullptr;
		return ESovDominionHandlerCommandSpecSelection::IdentityCollision;
	}
	if (ExactMatchCount == 0)
	{
		return ESovDominionHandlerCommandSpecSelection::Missing;
	}
	if (ExactMatchCount != 1)
	{
		OutHandle = FGameplayAbilitySpecHandle();
		OutAbilitySource = nullptr;
		return ESovDominionHandlerCommandSpecSelection::Ambiguous;
	}
	if (bExactMatchIsUnavailable)
	{
		OutHandle = FGameplayAbilitySpecHandle();
		OutAbilitySource = nullptr;
		return ESovDominionHandlerCommandSpecSelection::Unavailable;
	}
	if (!OutHandle.IsValid() || !IsValid(OutAbilitySource))
	{
		OutHandle = FGameplayAbilitySpecHandle();
		OutAbilitySource = nullptr;
		return ESovDominionHandlerCommandSpecSelection::Missing;
	}

	return ESovDominionHandlerCommandSpecSelection::UniqueInactive;
}

ESovDominionHandlerCommandSpecSelection
	UBTTask_SovDominionHandlerCommandHound::ResolveExactCommandAbility(
	UAbilitySystemComponent& AbilitySystem,
	FGameplayAbilitySpecHandle& OutHandle,
	UGameplayAbility*& OutAbilitySource) const
{
	FScopedAbilityListLock AbilityListLock(AbilitySystem);
	return SelectExactCommandAbilitySpec(
		AbilitySystem.GetActivatableAbilities(),
		OutHandle,
		OutAbilitySource);
}

bool UBTTask_SovDominionHandlerCommandHound::IsFailureBackoffActive(
	const UBehaviorTreeComponent& OwnerComp) const
{
	const UWorld* World = OwnerComp.GetWorld();
	return IsValid(World)
		&& World->GetTimeSeconds() + KINDA_SMALL_NUMBER
			< FailureRetryNotBefore;
}

void UBTTask_SovDominionHandlerCommandHound::ArmFailureBackoff(
	UBehaviorTreeComponent& OwnerComp)
{
	const UWorld* World = OwnerComp.GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float SafeBackoff = FMath::IsFinite(FailureBackoffSeconds)
		? FMath::Max(FailureBackoffSeconds, 0.0f)
		: 0.75f;
	FailureRetryNotBefore = World->GetTimeSeconds() + SafeBackoff;
}

void UBTTask_SovDominionHandlerCommandHound::BeginObservingAbility(
	UBehaviorTreeComponent& OwnerComp,
	UAbilitySystemComponent& AbilitySystem,
	const FGameplayAbilitySpecHandle AbilityHandle)
{
	StopObservingAbility();
	ObservedOwnerComp = &OwnerComp;
	ObservedAbilitySystem = &AbilitySystem;
	ObservedAbilityHandle = AbilityHandle;
	bStartedObservedAbility = false;
	bInsideTryActivate = false;
	bEndedSynchronously = false;
	bSynchronousEndWasCancelled = false;
	AbilityEndedDelegateHandle = AbilitySystem.OnAbilityEnded.AddUObject(
		this,
		&ThisClass::HandleAbilityEnded);
}

void UBTTask_SovDominionHandlerCommandHound::StopObservingAbility()
{
	if (AbilityEndedDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem =
			ObservedAbilitySystem.Get())
		{
			AbilitySystem->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		}
		AbilityEndedDelegateHandle.Reset();
	}
}

void UBTTask_SovDominionHandlerCommandHound::ClearObservedRequest()
{
	StopObservingAbility();
	ObservedOwnerComp.Reset();
	ObservedAbilitySystem.Reset();
	ObservedAbilityHandle = FGameplayAbilitySpecHandle();
	bStartedObservedAbility = false;
	bInsideTryActivate = false;
	bEndedSynchronously = false;
	bSynchronousEndWasCancelled = false;
}

void UBTTask_SovDominionHandlerCommandHound::HandleAbilityEnded(
	const FAbilityEndedData& EndedData)
{
	if (!ObservedAbilityHandle.IsValid()
		|| EndedData.AbilitySpecHandle != ObservedAbilityHandle)
	{
		return;
	}

	const bool bIdentityMismatch =
		!IsExactCommandAbilityDefinition(EndedData.AbilityThatEnded);
	const bool bCommandFailed = EndedData.bWasCancelled || bIdentityMismatch;
	StopObservingAbility();
	if (bInsideTryActivate)
	{
		bEndedSynchronously = true;
		bSynchronousEndWasCancelled = bCommandFailed;
		return;
	}

	UBehaviorTreeComponent* OwnerComp = ObservedOwnerComp.Get();
	ClearObservedRequest();
	if (!IsValid(OwnerComp)
		|| OwnerComp->GetTaskStatus(this) != EBTTaskStatus::Active)
	{
		return;
	}

	if (bCommandFailed)
	{
		ArmFailureBackoff(*OwnerComp);
	}
	else
	{
		// A normal end follows an accepted order. The gameplay ability already
		// owns its success-only cooldown, so the BT must add no second delay.
		FailureRetryNotBefore = 0.0;
	}
	FinishLatentTask(
		*OwnerComp,
		bCommandFailed
			? EBTNodeResult::Failed
			: EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_SovDominionHandlerCommandHound::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ClearObservedRequest();

	AAIController* Controller = OwnerComp.GetAIOwner();
	ASovDominionHandler* Handler = Controller
		? Cast<ASovDominionHandler>(Controller->GetPawn())
		: nullptr;
	UAbilitySystemComponent* AbilitySystem = IsValid(Handler)
		? Handler->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(Handler) || !Handler->HasAuthority()
		|| !IsValid(AbilitySystem)
		|| AbilitySystem->GetAvatarActor() != Handler)
	{
		return EBTNodeResult::Failed;
	}
	if (IsFailureBackoffActive(OwnerComp))
	{
		return EBTNodeResult::Failed;
	}

	FGameplayAbilitySpecHandle CommandHandle;
	UGameplayAbility* CommandAbility = nullptr;
	const ESovDominionHandlerCommandSpecSelection SpecSelection =
		ResolveExactCommandAbility(
			*AbilitySystem,
			CommandHandle,
			CommandAbility);
	if (SpecSelection !=
		ESovDominionHandlerCommandSpecSelection::UniqueInactive)
	{
		if (SpecSelection ==
			ESovDominionHandlerCommandSpecSelection::Unavailable)
		{
			// Never adopt or cancel a command activation this task did not start.
			return EBTNodeResult::Failed;
		}
		UE_LOG(
			LogSovDominionHandlerBT,
			Error,
			TEXT("%s cannot run the Handler command task: exact spec selection failed (%d)."),
			*GetNameSafe(Handler),
			static_cast<int32>(SpecSelection));
		ArmFailureBackoff(OwnerComp);
		return EBTNodeResult::Failed;
	}

	// This is the authoritative structural-opportunity query. The ability calls
	// it again during CanActivateAbility, closing changes between BT selection
	// and GAS activation.
	if (!IsValid(Handler->FindBestCommandableHound()))
	{
		// No structural opportunity is not an attempted command failure. Let the
		// tree's support/wait fallback control polling so a Hound still executing
		// the preceding successful order cannot manufacture a BT cooldown.
		return EBTNodeResult::Failed;
	}

	FGameplayTagContainer FailureTags;
	const FGameplayAbilityActorInfo* ActorInfo =
		AbilitySystem->AbilityActorInfo.Get();
	if (!ActorInfo
		|| !CommandAbility->CanActivateAbility(
			CommandHandle,
			ActorInfo,
			nullptr,
			nullptr,
			&FailureTags))
	{
		// Do not turn a successful command's native cooldown into a BT failure
		// cooldown. The fallback branch may run until GAS becomes eligible.
		if (!IsNativeCommandCooldownFailure(FailureTags))
		{
			ArmFailureBackoff(OwnerComp);
		}
		return EBTNodeResult::Failed;
	}

	BeginObservingAbility(OwnerComp, *AbilitySystem, CommandHandle);
	bStartedObservedAbility = true;
	bInsideTryActivate = true;
	const bool bActivated =
		AbilitySystem->TryActivateAbility(CommandHandle, false);
	bInsideTryActivate = false;

	if (bEndedSynchronously)
	{
		const bool bCommandFailed = bSynchronousEndWasCancelled;
		ClearObservedRequest();
		if (bCommandFailed)
		{
			ArmFailureBackoff(OwnerComp);
		}
		else
		{
			FailureRetryNotBefore = 0.0;
		}
		return bCommandFailed
			? EBTNodeResult::Failed
			: EBTNodeResult::Succeeded;
	}

	if (!bActivated)
	{
		ClearObservedRequest();
		// The immediately preceding local preflight already ruled out the
		// ability's native success cooldown. A rejection here is a genuine
		// state/configuration race and receives the failure-only backoff.
		ArmFailureBackoff(OwnerComp);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_SovDominionHandlerCommandHound::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	const FGameplayAbilitySpecHandle AbilityHandle = ObservedAbilityHandle;
	const bool bOwnedActivation = bStartedObservedAbility;
	ClearObservedRequest();

	// Remove our delegate before cancelling: GAS ends synchronously, and an
	// external BT abort is not a failed command attempt for retry accounting.
	if (bOwnedActivation
		&& IsValid(AbilitySystem) && AbilityHandle.IsValid())
	{
		const FGameplayAbilitySpec* Spec =
			AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec && Spec->IsActive()
			&& IsExactCommandAbilityDefinition(Spec->Ability))
		{
			AbilitySystem->CancelAbilityHandle(AbilityHandle);
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_SovDominionHandlerCommandHound::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	const EBTNodeResult::Type TaskResult)
{
	ClearObservedRequest();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_SovDominionHandlerCommandHound::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s\nExact Handler command; %.2fs failure-only backoff"),
		*Super::GetStaticDescription(),
		FailureBackoffSeconds);
}
