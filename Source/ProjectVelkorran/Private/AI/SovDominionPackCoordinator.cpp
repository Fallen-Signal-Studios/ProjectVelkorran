// Copyright Fallen Signal Studios. All Rights Reserved.

#include "AI/SovDominionPackCoordinator.h"

#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "Characters/SovDominionHandler.h"
#include "Components/SovCommandLinkComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Spawners/NPCSpawner.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovDominionPackCoordinator, Log, All);

namespace
{
	enum class ESpawnerNPCResolution : uint8
	{
		Ready,
		Waiting,
		Fatal
	};

	ESpawnerNPCResolution ResolveSingleLiveNPC(
		ANPCSpawner* Spawner,
		const UWorld* ExpectedWorld,
		const FString& RoleDescription,
		ANarrativeNPCCharacter*& OutNPC,
		FString& OutReason)
	{
		OutNPC = nullptr;
		OutReason.Reset();
		if (!IsValid(Spawner) || Spawner->GetWorld() != ExpectedWorld)
		{
			OutReason = FString::Printf(
				TEXT("%s references an invalid or cross-world NPC spawner"),
				*RoleDescription);
			return ESpawnerNPCResolution::Fatal;
		}

		TArray<ANarrativeNPCCharacter*> SpawnedNPCs;
		Spawner->GetSpawnedNPCs(SpawnedNPCs);
		TArray<ANarrativeNPCCharacter*, TInlineAllocator<2>> LiveNPCs;
		for (ANarrativeNPCCharacter* NPC : SpawnedNPCs)
		{
			if (IsValid(NPC) && NPC->GetWorld() != ExpectedWorld)
			{
				OutReason = FString::Printf(
					TEXT("%s spawner %s returned cross-world NPC %s"),
					*RoleDescription,
					*GetNameSafe(Spawner),
					*GetNameSafe(NPC));
				return ESpawnerNPCResolution::Fatal;
			}
			if (IsValid(NPC) && NPC->IsAlive())
			{
				LiveNPCs.Add(NPC);
			}
		}

		if (LiveNPCs.Num() == 1)
		{
			OutNPC = LiveNPCs[0];
			return ESpawnerNPCResolution::Ready;
		}
		if (LiveNPCs.IsEmpty())
		{
			OutReason = FString::Printf(
				TEXT("%s spawner %s has not produced one live NPC yet"),
				*RoleDescription,
				*GetNameSafe(Spawner));
			return ESpawnerNPCResolution::Waiting;
		}

		OutReason = FString::Printf(
			TEXT("%s spawner %s produced %d live NPCs; exactly one is required"),
			*RoleDescription,
			*GetNameSafe(Spawner),
			LiveNPCs.Num());
		return ESpawnerNPCResolution::Fatal;
	}
}

ASovDominionPackCoordinator::ASovDominionPackCoordinator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ASovDominionPackCoordinator::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AttemptPackInitialization();
}

void ASovDominionPackCoordinator::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitializationRetryTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool ASovDominionPackCoordinator::HasValidSpawnerConfiguration() const
{
	FString FailureReason;
	return ValidateSpawnerConfiguration(FailureReason);
}

bool ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
	const UNarrativeAbilitySystemComponent* AbilitySystem,
	bool& bOutSpecMissing)
{
	bOutSpecMissing = false;
	if (!IsValid(AbilitySystem))
	{
		bOutSpecMissing = true;
		return false;
	}

	const FGameplayTag HornChargeTag =
		FSovGameplayTags::Get().Ability_NPC_DominionHound_HornCharge;
	int32 MatchCount = 0;
	for (const FGameplayAbilitySpec& Spec :
		AbilitySystem->GetActivatableAbilities())
	{
		const bool bIsHornChargeClass = IsValid(
			Cast<USovGameplayAbility_DominionHoundHornCharge>(Spec.Ability));
		const bool bHasHornChargeIdentity = IsValid(Spec.Ability)
			&& Spec.Ability->GetAssetTags().HasTagExact(HornChargeTag);
		if (bIsHornChargeClass != bHasHornChargeIdentity)
		{
			return false;
		}
		if (bIsHornChargeClass)
		{
			++MatchCount;
			if (MatchCount > 1 || Spec.IsActive()
				|| Spec.PendingRemove || Spec.RemoveAfterActivation)
			{
				return false;
			}
		}
	}

	bOutSpecMissing = MatchCount == 0;
	return MatchCount == 1;
}

bool ASovDominionPackCoordinator::ValidateSpawnerConfiguration(
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (CommandLinkId == NAME_None)
	{
		OutFailureReason = TEXT("CommandLinkId is None");
		return false;
	}
	if (!FMath::IsFinite(InitializationRetryInterval)
		|| InitializationRetryInterval < 0.01f)
	{
		OutFailureReason = TEXT("InitializationRetryInterval must be finite and at least 0.01 seconds");
		return false;
	}
	if (MaximumInitializationAttempts < 1)
	{
		OutFailureReason = TEXT("MaximumInitializationAttempts must be at least one");
		return false;
	}
	if (!IsValid(HandlerSpawner))
	{
		OutFailureReason = TEXT("HandlerSpawner is not assigned");
		return false;
	}
	if (HoundSpawners.IsEmpty())
	{
		OutFailureReason = TEXT("at least one HoundSpawner is required");
		return false;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		OutFailureReason = TEXT("coordinator has no valid world");
		return false;
	}
	if (HandlerSpawner->GetWorld() != World)
	{
		OutFailureReason = FString::Printf(
			TEXT("HandlerSpawner %s belongs to another world"),
			*GetNameSafe(HandlerSpawner));
		return false;
	}

	TSet<const ANPCSpawner*> SeenSpawners;
	SeenSpawners.Add(HandlerSpawner);
	for (int32 Index = 0; Index < HoundSpawners.Num(); ++Index)
	{
		ANPCSpawner* HoundSpawner = HoundSpawners[Index].Get();
		if (!IsValid(HoundSpawner))
		{
			OutFailureReason = FString::Printf(
				TEXT("HoundSpawners[%d] is not assigned"),
				Index);
			return false;
		}
		if (HoundSpawner->GetWorld() != World)
		{
			OutFailureReason = FString::Printf(
				TEXT("HoundSpawners[%d] (%s) belongs to another world"),
				Index,
				*GetNameSafe(HoundSpawner));
			return false;
		}
		if (SeenSpawners.Contains(HoundSpawner))
		{
			OutFailureReason = FString::Printf(
				TEXT("NPC spawner %s is referenced more than once"),
				*GetNameSafe(HoundSpawner));
			return false;
		}
		SeenSpawners.Add(HoundSpawner);
	}

	return true;
}

void ASovDominionPackCoordinator::AttemptPackInitialization()
{
	if (!HasAuthority() || bPackInitialized || bInitializationFailed)
	{
		return;
	}

	++InitializationAttemptCount;
	FString FailureReason;
	if (!ValidateSpawnerConfiguration(FailureReason))
	{
		FailInitialization(FailureReason);
		return;
	}

	ANarrativeNPCCharacter* HandlerNPC = nullptr;
	ESpawnerNPCResolution Resolution = ResolveSingleLiveNPC(
		HandlerSpawner,
		GetWorld(),
		TEXT("Handler"),
		HandlerNPC,
		FailureReason);
	if (Resolution == ESpawnerNPCResolution::Waiting)
	{
		RetryOrFail(FailureReason);
		return;
	}
	if (Resolution == ESpawnerNPCResolution::Fatal)
	{
		FailInitialization(FailureReason);
		return;
	}

	ASovDominionHandler* Handler = Cast<ASovDominionHandler>(HandlerNPC);
	if (!IsValid(Handler))
	{
		FailInitialization(FString::Printf(
			TEXT("HandlerSpawner %s produced %s (%s), not an ASovDominionHandler"),
			*GetNameSafe(HandlerSpawner),
			*GetNameSafe(HandlerNPC),
			*GetNameSafe(HandlerNPC ? HandlerNPC->GetClass() : nullptr)));
		return;
	}

	TArray<ANarrativeNPCCharacter*> Hounds;
	Hounds.Reserve(HoundSpawners.Num());
	TSet<const ANarrativeNPCCharacter*> ResolvedNPCs;
	ResolvedNPCs.Add(Handler);
	for (int32 Index = 0; Index < HoundSpawners.Num(); ++Index)
	{
		ANarrativeNPCCharacter* Hound = nullptr;
		const FString RoleDescription = FString::Printf(
			TEXT("Hound[%d]"),
			Index);
		Resolution = ResolveSingleLiveNPC(
			HoundSpawners[Index],
			GetWorld(),
			RoleDescription,
			Hound,
			FailureReason);
		if (Resolution == ESpawnerNPCResolution::Waiting)
		{
			RetryOrFail(FailureReason);
			return;
		}
		if (Resolution == ESpawnerNPCResolution::Fatal)
		{
			FailInitialization(FailureReason);
			return;
		}
		if (IsValid(Cast<ASovDominionHandler>(Hound)))
		{
			FailInitialization(FString::Printf(
				TEXT("HoundSpawners[%d] (%s) produced Handler %s"),
				Index,
				*GetNameSafe(HoundSpawners[Index]),
				*GetNameSafe(Hound)));
			return;
		}
		if (ResolvedNPCs.Contains(Hound))
		{
			FailInitialization(FString::Printf(
				TEXT("NPC %s was resolved from more than one configured spawner"),
				*GetNameSafe(Hound)));
			return;
		}

		UNarrativeAbilitySystemComponent* HoundAbilitySystem =
			Hound->GetNarrativeAbilitySystemComponent();
		if (!IsValid(HoundAbilitySystem)
			|| HoundAbilitySystem->GetAvatarActor() != Hound)
		{
			RetryOrFail(FString::Printf(
				TEXT("HoundSpawners[%d] (%s) produced %s, whose Narrative ASC is not initialized yet"),
				Index,
				*GetNameSafe(HoundSpawners[Index]),
				*GetNameSafe(Hound)));
			return;
		}
		bool bHornChargeSpecMissing = false;
		const bool bHornChargeSpecReady =
			HasExactlyOneReadyHornChargeSpec(
				HoundAbilitySystem,
				bHornChargeSpecMissing);
		if (!bHornChargeSpecReady && bHornChargeSpecMissing)
		{
			RetryOrFail(FString::Printf(
				TEXT("HoundSpawners[%d] (%s) produced %s, whose initialized ASC has not received Horn Charge yet"),
				Index,
				*GetNameSafe(HoundSpawners[Index]),
				*GetNameSafe(Hound)));
			return;
		}
		if (!bHornChargeSpecReady)
		{
			FailInitialization(FString::Printf(
				TEXT("HoundSpawners[%d] (%s) produced %s with malformed, duplicate, active, or removal-pending Horn Charge specs"),
				Index,
				*GetNameSafe(HoundSpawners[Index]),
				*GetNameSafe(Hound)));
			return;
		}

		ResolvedNPCs.Add(Hound);
		Hounds.Add(Hound);
	}

	if (!ConfigureResolvedPack(Handler, Hounds, FailureReason))
	{
		FailInitialization(FailureReason);
		return;
	}

	bPackInitialized = true;
	GetWorldTimerManager().ClearTimer(InitializationRetryTimerHandle);
	UE_LOG(
		LogSovDominionPackCoordinator,
		Log,
		TEXT("%s initialized Dominion pack '%s' with Handler %s and %d Hound(s) on attempt %d."),
		*GetNameSafe(this),
		*CommandLinkId.ToString(),
		*GetNameSafe(Handler),
		Hounds.Num(),
		InitializationAttemptCount);
}

bool ASovDominionPackCoordinator::ConfigureResolvedPack(
	ASovDominionHandler* Handler,
	const TArray<ANarrativeNPCCharacter*>& Hounds,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!IsValid(Handler) || !Handler->IsAlive()
		|| Handler->GetWorld() != GetWorld()
		|| !Handler->HasAuthority())
	{
		OutFailureReason = TEXT("resolved Handler became invalid or died before link setup");
		return false;
	}

	USovCommandLinkComponent* CommandLink = Handler->GetCommandLinkComponent();
	if (!IsValid(CommandLink))
	{
		OutFailureReason = FString::Printf(
			TEXT("Handler %s has no native command-link component"),
			*GetNameSafe(Handler));
		return false;
	}
	if (!CommandLink->IncludesOwnerAsParticipant())
	{
		OutFailureReason = FString::Printf(
			TEXT("Handler %s command link excludes its owner"),
			*GetNameSafe(Handler));
		return false;
	}
	if (CommandLink->GetCommandLinkState() != ESovCommandLinkState::Inactive
		|| CommandLink->GetLinkInstanceId().IsValid())
	{
		OutFailureReason = FString::Printf(
			TEXT("Handler %s command link already has live or terminal instance state"),
			*GetNameSafe(Handler));
		return false;
	}

	TSet<const AActor*> ExpectedHounds;
	for (ANarrativeNPCCharacter* Hound : Hounds)
	{
		if (!IsValid(Hound) || !Hound->IsAlive()
			|| Hound->GetWorld() != GetWorld() || Hound == Handler)
		{
			OutFailureReason = TEXT("a resolved Hound became invalid before link setup");
			return false;
		}
		const UNarrativeAbilitySystemComponent* HoundAbilitySystem =
			Hound->GetNarrativeAbilitySystemComponent();
		bool bHornChargeSpecMissing = false;
		if (!IsValid(HoundAbilitySystem)
			|| HoundAbilitySystem->GetAvatarActor() != Hound
			|| !HasExactlyOneReadyHornChargeSpec(
				HoundAbilitySystem,
				bHornChargeSpecMissing))
		{
			OutFailureReason = FString::Printf(
				TEXT("resolved Hound %s lost its ready exact Horn Charge spec before link setup"),
				*GetNameSafe(Hound));
			return false;
		}
		ExpectedHounds.Add(Hound);
	}
	if (ExpectedHounds.Num() != Hounds.Num())
	{
		OutFailureReason = TEXT("resolved Hound actors are not unique");
		return false;
	}

	for (AActor* ExistingActor : CommandLink->GetLinkedActors())
	{
		if (!ExpectedHounds.Contains(ExistingActor))
		{
			OutFailureReason = FString::Printf(
				TEXT("Handler command link already contains unexpected actor %s"),
				*GetNameSafe(ExistingActor));
			return false;
		}
	}
	if (!CommandLink->ConfigureLinkId(CommandLinkId))
	{
		OutFailureReason = FString::Printf(
			TEXT("Handler %s rejected inactive LinkId configuration '%s'"),
			*GetNameSafe(Handler),
			*CommandLinkId.ToString());
		return false;
	}

	TArray<AActor*> NewlyRegisteredHounds;
	const auto RollBackNewRegistrations = [&CommandLink, &NewlyRegisteredHounds]()
	{
		if (!IsValid(CommandLink)
			|| CommandLink->GetCommandLinkState()
				!= ESovCommandLinkState::Inactive)
		{
			return;
		}
		for (int32 Index = NewlyRegisteredHounds.Num() - 1; Index >= 0; --Index)
		{
			CommandLink->UnregisterLinkedActor(NewlyRegisteredHounds[Index]);
		}
	};

	for (ANarrativeNPCCharacter* Hound : Hounds)
	{
		if (CommandLink->ContainsLinkedActor(Hound))
		{
			continue;
		}
		if (!CommandLink->RegisterLinkedActor(Hound))
		{
			OutFailureReason = FString::Printf(
				TEXT("Handler command link rejected Hound %s"),
				*GetNameSafe(Hound));
			RollBackNewRegistrations();
			return false;
		}
		NewlyRegisteredHounds.Add(Hound);
	}

	const TArray<AActor*> RegisteredActors = CommandLink->GetLinkedActors();
	if (RegisteredActors.Num() != Hounds.Num())
	{
		OutFailureReason = TEXT("Handler command-link membership is not the exact resolved Hound set");
		RollBackNewRegistrations();
		return false;
	}
	for (AActor* RegisteredActor : RegisteredActors)
	{
		if (!ExpectedHounds.Contains(RegisteredActor))
		{
			OutFailureReason = FString::Printf(
				TEXT("Handler command link gained unexpected actor %s during setup"),
				*GetNameSafe(RegisteredActor));
			RollBackNewRegistrations();
			return false;
		}
	}

	if (!CommandLink->ActivateCommandLink(Handler))
	{
		OutFailureReason = FString::Printf(
			TEXT("Handler %s command link rejected activation for '%s'"),
			*GetNameSafe(Handler),
			*CommandLinkId.ToString());
		RollBackNewRegistrations();
		return false;
	}
	if (!CommandLink->IsCommandLinkActive()
		|| CommandLink->GetCommandSource() != Handler
		|| !CommandLink->GetLinkInstanceId().IsValid()
		|| CommandLink->GetLinkId() != CommandLinkId)
	{
		OutFailureReason = TEXT("command link activation returned inconsistent state");
		return false;
	}

	return true;
}

void ASovDominionPackCoordinator::RetryOrFail(
	const FString& WaitingReason)
{
	if (InitializationAttemptCount >= MaximumInitializationAttempts)
	{
		FailInitialization(FString::Printf(
			TEXT("timed out after %d readiness attempts: %s"),
			InitializationAttemptCount,
			*WaitingReason));
		return;
	}

	if (InitializationAttemptCount == 1)
	{
		UE_LOG(
			LogSovDominionPackCoordinator,
			Log,
			TEXT("%s is waiting to initialize Dominion pack '%s': %s. Retrying up to %d times."),
			*GetNameSafe(this),
			*CommandLinkId.ToString(),
			*WaitingReason,
			MaximumInitializationAttempts);
	}

	GetWorldTimerManager().SetTimer(
		InitializationRetryTimerHandle,
		this,
		&ThisClass::AttemptPackInitialization,
		InitializationRetryInterval,
		false);
}

void ASovDominionPackCoordinator::FailInitialization(
	const FString& FailureReason)
{
	bInitializationFailed = true;
	GetWorldTimerManager().ClearTimer(InitializationRetryTimerHandle);
	UE_LOG(
		LogSovDominionPackCoordinator,
		Error,
		TEXT("%s failed to initialize Dominion pack '%s' on attempt %d: %s. No further initialization attempts will be made."),
		*GetNameSafe(this),
		*CommandLinkId.ToString(),
		InitializationAttemptCount,
		*FailureReason);
}
