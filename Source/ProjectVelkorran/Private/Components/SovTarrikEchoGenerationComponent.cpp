// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovTarrikEchoGenerationComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ArsenalStatics.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Combat/SovEchoAttackReceipt.h"
#include "Combat/SovProtectionInterceptReceipt.h"
#include "Combat/SovProtectionAwardPolicy.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "Items/RangedWeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePhysicalMaterial.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovTarrikEchoGeneration, Log, All);

USovTarrikEchoGenerationComponent::USovTarrikEchoGenerationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	PrecisionBoneNames.Add(FName(TEXT("head")));
}

void USovTarrikEchoGenerationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovTarrikEchoGenerationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovTarrikEchoGenerationComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		USovTarrikEchoGenerationComponent,
		CinderlineCadence,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

bool USovTarrikEchoGenerationComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(GetOwner()))
	{
		return false;
	}
	const USovTarrikEchoGenerationComponent* CanonicalGenerator =
		GetOwner()->FindComponentByClass<USovTarrikEchoGenerationComponent>();
	if (const ASovPlayerCharacterBase* PlayerOwner =
		Cast<ASovPlayerCharacterBase>(GetOwner()))
	{
		CanonicalGenerator = PlayerOwner->GetTarrikEchoGenerationComponent();
	}
	if (CanonicalGenerator != this)
	{
		UE_LOG(
			LogSovTarrikEchoGeneration,
			Error,
			TEXT("%s has more than one Tarrik Echo generator. Ignoring duplicate %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this));
		return false;
	}

	if (AbilitySystemComponent.Get() == InAbilitySystemComponent
		&& IsValid(EchoComponent.Get())
		&& bBindingsActive)
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>();
	if (!IsValid(EchoComponent.Get()))
	{
		UE_LOG(
			LogSovTarrikEchoGeneration,
			Error,
			TEXT("%s cannot initialize Tarrik Echo generation without USovEchoComponent."),
			*GetNameSafe(GetOwner()));
		AbilitySystemComponent = nullptr;
		return false;
	}

	if (!EchoComponent->IsInitialized()
		&& !EchoComponent->InitializeWithAbilitySystem(InAbilitySystemComponent))
	{
		UE_LOG(
			LogSovTarrikEchoGeneration,
			Error,
			TEXT("%s cannot initialize Tarrik Echo generation because Echo storage is not ready."),
			*GetNameSafe(GetOwner()));
		AbilitySystemComponent = nullptr;
		EchoComponent = nullptr;
		return false;
	}

	if (GetOwner()->HasAuthority())
	{
		AbilitySystemComponent->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::HandleDamageResolvedAsSource);
		AbilitySystemComponent->OnDealtDamage.AddUniqueDynamic(
			this,
			&ThisClass::HandleDealtDamage);
	}
	EchoComponent->OnEncounterScopeChanged.AddUniqueDynamic(this, &ThisClass::HandleEncounterScopeChanged);
	EchoComponent->OnEchoChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleEchoChanged);

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DeadTagChangedHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			NarrativeTags.State_IsDead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleBlockingTagChanged);
	FatalTagChangedHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			SovTags.State_Fatal,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleBlockingTagChanged);
	EchoAbilityTagChangedHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			SovTags.State_EchoAbility_Active,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleBlockingTagChanged);

	bBindingsActive = true;
	bWarnedMissingCinderlineIdentity = false;
	LastCadenceAwardWorldTime = -BIG_NUMBER;
	return true;
}

bool USovTarrikEchoGenerationComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get())
		&& IsValid(EchoComponent.Get())
		&& bBindingsActive;
}

float USovTarrikEchoGenerationComponent::GetCinderlineCadenceNormalized() const
{
	return static_cast<float>(CinderlineCadence)
		/ static_cast<float>(GetCinderlineCadenceThreshold());
}

void USovTarrikEchoGenerationComponent::ResetCinderlineCadence()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ResetCinderlineCadenceInternal();
	}
}

void USovTarrikEchoGenerationComponent::HandleOwnerWieldStateChanged()
{
	if (GetOwner()
		&& GetOwner()->HasAuthority()
		&& CinderlineCadence > 0
		&& !IsSourceWeaponStillWielded(PendingCadenceWeapon.Get()))
	{
		ResetCinderlineCadenceInternal();
	}
}

void USovTarrikEchoGenerationComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	UNarrativeAbilitySystemComponent* OwnerAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
	if (IsValid(OwnerAbilitySystem)
		&& OwnerAbilitySystem != AbilitySystemComponent.Get())
	{
		InitializeWithAbilitySystem(OwnerAbilitySystem);
	}
}

void USovTarrikEchoGenerationComponent::UninitializeFromAbilitySystem()
{
	++ResourceScopeEpoch;
	ProtectionSourceAwardTimes.Reset();
	HeavyAttacks.Reset();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ResetCinderlineCadenceInternal();
	}
	else
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(CadenceTimeoutTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(CadenceAwardTimerHandle);
		}
		PendingCadenceWeapon.Reset();
		PendingCadenceWeightedMultiplierSum = 0.0f;
	}

	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleDamageResolvedAsSource);
		AbilitySystemComponent->OnDealtDamage.RemoveDynamic(
			this,
			&ThisClass::HandleDealtDamage);

		const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
		const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
		if (DeadTagChangedHandle.IsValid())
		{
			AbilitySystemComponent
				->RegisterGameplayTagEvent(
					NarrativeTags.State_IsDead,
					EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedHandle);
		}
		if (FatalTagChangedHandle.IsValid())
		{
			AbilitySystemComponent
				->RegisterGameplayTagEvent(
					SovTags.State_Fatal,
					EGameplayTagEventType::NewOrRemoved)
				.Remove(FatalTagChangedHandle);
		}
		if (EchoAbilityTagChangedHandle.IsValid())
		{
			AbilitySystemComponent
				->RegisterGameplayTagEvent(
					SovTags.State_EchoAbility_Active,
					EGameplayTagEventType::NewOrRemoved)
				.Remove(EchoAbilityTagChangedHandle);
		}
	}

	if (IsValid(EchoComponent.Get()))
	{
		EchoComponent->OnEncounterScopeChanged.RemoveDynamic(this, &ThisClass::HandleEncounterScopeChanged);
		EchoComponent->OnEchoChanged.RemoveDynamic(
			this,
			&ThisClass::HandleEchoChanged);
	}

	AbilitySystemComponent = nullptr;
	EchoComponent = nullptr;
	DeadTagChangedHandle.Reset();
	FatalTagChangedHandle.Reset();
	EchoAbilityTagChangedHandle.Reset();
	bBindingsActive = false;
	LastCadenceAwardWorldTime = -BIG_NUMBER;
}

bool USovTarrikEchoGenerationComponent::CanGenerateCinderlineEcho() const
{
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| EchoComponent->GetMaxEcho() <= KINDA_SMALL_NUMBER
		|| EchoComponent->GetEcho() + KINDA_SMALL_NUMBER >= EchoComponent->GetMaxEcho())
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (AbilitySystemComponent->HasMatchingGameplayTag(NarrativeTags.State_IsDead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Fatal)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_EchoAbility_Active))
	{
		return false;
	}

	// During content migration an entirely untagged player remains usable,
	// matching the shared Tarrik ability adapter. Once a player identity exists,
	// Tarrik must be the one and only concrete protagonist identity.
	if (!AbilitySystemComponent->HasMatchingGameplayTag(SovTags.Character_Player))
	{
		return true;
	}
	if (!AbilitySystemComponent->HasMatchingGameplayTag(
		SovTags.Character_Player_Tarrik))
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
	for (const FGameplayTag& OwnedTag : OwnedTags)
	{
		if (OwnedTag != SovTags.Character_Player
			&& OwnedTag != SovTags.Character_Player_Tarrik
			&& OwnedTag.MatchesTag(SovTags.Character_Player))
		{
			return false;
		}
	}
	return true;
}

bool USovTarrikEchoGenerationComponent::IsQualifyingPrimaryFire(
	const FGameplayEffectSpec& EffectSpec,
	URangedWeaponItem*& OutSourceWeapon) const
{
	OutSourceWeapon = nullptr;
	if (!EffectSpec.Def
		|| EffectSpec.Def->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return false;
	}

	FGameplayTagContainer EffectTags;
	EffectSpec.GetAllAssetTags(EffectTags);
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (EffectTags.HasTag(SovTags.Ability_Echo)
		|| EffectTags.HasTagExact(SovTags.Damage_Result_Guarded))
	{
		return false;
	}

	const FGameplayEffectContextHandle& Context = EffectSpec.GetContext();
	const FHitResult* HitResult = Context.GetHitResult();
	if (!HitResult || !HitResult->bBlockingHit)
	{
		return false;
	}

	URangedWeaponItem* SourceWeapon = Cast<URangedWeaponItem>(Context.GetSourceObject());
	if (!IsValid(SourceWeapon) || !IsSourceWeaponStillWielded(SourceWeapon))
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	const UNarrativeGameplayAbility* SourceAbility =
		Cast<UNarrativeGameplayAbility>(Context.GetAbility());
	if (!IsValid(SourceAbility))
	{
		return false;
	}
	AbilityTags.AppendTags(SourceAbility->GetAssetTags());
	if (AbilityTags.HasTag(SovTags.Ability_Echo))
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const bool bExplicitCinderlinePrimary =
		AbilityTags.HasTagExact(SovTags.Ability_Weapon_Cinderline_PrimaryFire)
		|| EffectTags.HasTagExact(SovTags.Ability_Weapon_Cinderline_PrimaryFire);
	const bool bGenericNarrativePrimary =
		SourceAbility->InputTag == NarrativeTags.Narrative_Input_Attack
		&& AbilityTags.HasTagExact(NarrativeTags.Ability_WeaponFire)
		&& AbilityTags.HasTagExact(NarrativeTags.Ability_DamageType_Ranged);
	if (!bExplicitCinderlinePrimary && !bGenericNarrativePrimary)
	{
		return false;
	}

	const bool bKnownCinderlineWeapon = bExplicitCinderlinePrimary
		|| IsConfiguredCinderlineWeapon(SourceWeapon)
		|| WeaponGrantsCinderlineEchoAbility(SourceWeapon);
	if (!bKnownCinderlineWeapon)
	{
		if (bGenericNarrativePrimary && !bWarnedMissingCinderlineIdentity)
		{
			UE_LOG(
				LogSovTarrikEchoGeneration,
				Warning,
				TEXT("%s ignored ranged primary-fire Echo generation from %s. Add "
					"Sov.Ability.Weapon.Cinderline.PrimaryFire to Cinderline's fire GA, "
					"or add the item class to Allowed Cinderline Weapon Classes."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(SourceWeapon));
			bWarnedMissingCinderlineIdentity = true;
		}
		return false;
	}

	OutSourceWeapon = SourceWeapon;
	return true;
}

bool USovTarrikEchoGenerationComponent::IsConfiguredCinderlineWeapon(
	const URangedWeaponItem* SourceWeapon) const
{
	if (!IsValid(SourceWeapon))
	{
		return false;
	}

	for (const TSubclassOf<URangedWeaponItem>& AllowedClass : AllowedCinderlineWeaponClasses)
	{
		if (AllowedClass.Get() && SourceWeapon->IsA(AllowedClass.Get()))
		{
			return true;
		}
	}
	return false;
}

bool USovTarrikEchoGenerationComponent::WeaponGrantsCinderlineEchoAbility(
	const URangedWeaponItem* SourceWeapon) const
{
	if (!IsValid(SourceWeapon) || !IsValid(AbilitySystemComponent.Get()))
	{
		return false;
	}

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	for (const FGameplayAbilitySpec& AbilitySpec :
		AbilitySystemComponent->GetActivatableAbilities())
	{
		if (AbilitySpec.SourceObject.Get() != SourceWeapon || !AbilitySpec.Ability)
		{
			continue;
		}

		const FGameplayTagContainer& AbilityTags = AbilitySpec.Ability->GetAssetTags();
		if (AbilityTags.HasTagExact(Tags.Ability_Echo_Tarrik_CinderJudgement)
			|| AbilityTags.HasTagExact(Tags.Ability_Echo_Tarrik_CinderlineRequiem))
		{
			return true;
		}
	}
	return false;
}

bool USovTarrikEchoGenerationComponent::IsSourceWeaponStillWielded(
	const URangedWeaponItem* SourceWeapon) const
{
	const ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner());
	if (!IsValid(NarrativeOwner)
		|| !IsValid(SourceWeapon)
		|| !SourceWeapon->IsWielded())
	{
		return false;
	}

	for (const UWeaponItem* WieldedWeapon : NarrativeOwner->GetWieldedWeapons())
	{
		if (WieldedWeapon == SourceWeapon)
		{
			return true;
		}
	}
	return false;
}

bool USovTarrikEchoGenerationComponent::IsPrecisionHit(
	const FGameplayEffectSpec& EffectSpec) const
{
	const FHitResult* HitResult = EffectSpec.GetContext().GetHitResult();
	if (!HitResult)
	{
		return false;
	}

	FName TestBone = HitResult->BoneName;
	if (!TestBone.IsNone() && PrecisionBoneNames.Contains(TestBone))
	{
		return true;
	}

	if (!TestBone.IsNone())
	{
		const USkinnedMeshComponent* HitMesh = Cast<USkinnedMeshComponent>(
			HitResult->GetComponent());
		if (IsValid(HitMesh))
		{
			for (int32 ParentDepth = 0; ParentDepth < 64; ++ParentDepth)
			{
				const FName ParentBone = HitMesh->GetParentBone(TestBone);
				if (ParentBone.IsNone() || ParentBone == TestBone)
				{
					break;
				}
				if (PrecisionBoneNames.Contains(ParentBone))
				{
					return true;
				}
				TestBone = ParentBone;
			}
		}
	}

	if (bUsePhysicalMaterialWeakPoints)
	{
		if (const UNarrativePhysicalMaterial* PhysicalMaterial =
			Cast<UNarrativePhysicalMaterial>(HitResult->PhysMaterial.Get()))
		{
			return PhysicalMaterial->DamageMultiplier + KINDA_SMALL_NUMBER
				>= FMath::Max(PrecisionPhysicalMaterialMultiplierThreshold, 1.0f);
		}
	}
	return false;
}

bool USovTarrikEchoGenerationComponent::IsBossTarget(
	const UNarrativeAbilitySystemComponent* DamagedAbilitySystem) const
{
	return IsValid(DamagedAbilitySystem)
		&& DamagedAbilitySystem->HasMatchingGameplayTag(
			FSovGameplayTags::Get().Character_Enemy_Boss);
}

void USovTarrikEchoGenerationComponent::AddCinderlineCadence(
	const int32 Points,
	URangedWeaponItem* SourceWeapon,
	const bool bBossReduced,
	const bool bPrecisionHit,
	const FName HitBone)
{
	if (Points <= 0 || !IsValid(SourceWeapon) || !GetWorld())
	{
		return;
	}

	if (CinderlineCadence > 0 && PendingCadenceWeapon.Get() != SourceWeapon)
	{
		ResetCinderlineCadenceInternal();
	}
	PendingCadenceWeapon = SourceWeapon;

	const int32 EffectiveThreshold = GetCinderlineCadenceThreshold();
	const int32 AcceptedPoints = FMath::Min(
		FMath::Max(Points, 0),
		FMath::Max(EffectiveThreshold - CinderlineCadence, 0));
	if (AcceptedPoints > 0)
	{
		const float TargetMultiplier = bBossReduced
			? FMath::Clamp(BossEchoMultiplier, 0.0f, 1.0f)
			: 1.0f;
		PendingCadenceWeightedMultiplierSum +=
			static_cast<float>(AcceptedPoints) * TargetMultiplier;
		SetCinderlineCadence(CinderlineCadence + AcceptedPoints);
		ClientNotifyCinderlineHitConfirmed(
			CinderlineCadence,
			bPrecisionHit,
			HitBone,
			bBossReduced);
	}

	GetWorld()->GetTimerManager().SetTimer(
		CadenceTimeoutTimerHandle,
		this,
		&ThisClass::HandleCadenceTimeout,
		FMath::Max(CadenceTimeout, 0.05f),
		false);

	if (CinderlineCadence >= EffectiveThreshold)
	{
		TryAwardCompletedCadence();
	}
}

void USovTarrikEchoGenerationComponent::TryAwardCompletedCadence()
{
	if (CinderlineCadence < GetCinderlineCadenceThreshold())
	{
		return;
	}

	URangedWeaponItem* SourceWeapon = PendingCadenceWeapon.Get();
	if (!CanGenerateCinderlineEcho() || !IsSourceWeaponStillWielded(SourceWeapon))
	{
		ResetCinderlineCadenceInternal();
		return;
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	const float NextAwardTime = LastCadenceAwardWorldTime
		+ FMath::Max(CadenceAwardCooldown, 0.0f);
	if (CurrentWorldTime + KINDA_SMALL_NUMBER < NextAwardTime)
	{
		if (GetWorld())
		{
			// A completed sequence is earned. Do not let the unfinished-sequence
			// timeout delete it while the anti-spam payout cooldown finishes.
			GetWorld()->GetTimerManager().ClearTimer(CadenceTimeoutTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(
				CadenceAwardTimerHandle,
				this,
				&ThisClass::TryAwardCompletedCadence,
				FMath::Max(NextAwardTime - CurrentWorldTime, KINDA_SMALL_NUMBER),
				false);
		}
		return;
	}

	const int32 EffectiveThreshold = GetCinderlineCadenceThreshold();
	const float WeightedMultiplier = PendingCadenceWeightedMultiplierSum > 0.0f
		? FMath::Clamp(
			PendingCadenceWeightedMultiplierSum
				/ static_cast<float>(EffectiveThreshold),
			0.0f,
			1.0f)
		: 1.0f;
	const bool bBossReduced = WeightedMultiplier < 1.0f - KINDA_SMALL_NUMBER;
	const float RequestedEcho = FMath::Max(CadenceEchoReward, 0.0f)
		* WeightedMultiplier;

	LastCadenceAwardWorldTime = CurrentWorldTime;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CadenceAwardTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(CadenceTimeoutTimerHandle);
	}
	PendingCadenceWeapon.Reset();
	PendingCadenceWeightedMultiplierSum = 0.0f;
	SetCinderlineCadence(0);

	AwardEcho(
		RequestedEcho,
		FSovGameplayTags::Get().Echo_Source_CinderlineCadence,
		ESovCinderlineEchoAwardType::Cadence,
		bBossReduced);
}

void USovTarrikEchoGenerationComponent::AwardEcho(
	const float RequestedEcho,
	const FGameplayTag& SourceTag,
	const ESovCinderlineEchoAwardType AwardType,
	const bool bBossReduced)
{
	if (!IsValid(EchoComponent.Get()) || RequestedEcho <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	USovEchoComponent* OriginalEcho = EchoComponent.Get();
	const uint32 Epoch = ResourceScopeEpoch;
	const float BeforeEcho = OriginalEcho->GetEcho();
	const float AppliedEcho = OriginalEcho->AddEcho(RequestedEcho, SourceTag);
	if (ResourceScopeEpoch != Epoch || EchoComponent.Get() != OriginalEcho || !IsValid(OriginalEcho)) return;
	if (AppliedEcho > KINDA_SMALL_NUMBER)
	{
		ClientNotifyCinderlineEchoAwarded(
			AppliedEcho,
			BeforeEcho + AppliedEcho,
			AwardType,
			bBossReduced);
	}
}

void USovTarrikEchoGenerationComponent::HandleCadenceTimeout()
{
	ResetCinderlineCadenceInternal();
}

void USovTarrikEchoGenerationComponent::ResetCinderlineCadenceInternal()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CadenceTimeoutTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(CadenceAwardTimerHandle);
	}
	PendingCadenceWeapon.Reset();
	PendingCadenceWeightedMultiplierSum = 0.0f;
	SetCinderlineCadence(0);
}

void USovTarrikEchoGenerationComponent::SetCinderlineCadence(
	const int32 NewCadence)
{
	const int32 ClampedCadence = FMath::Clamp(
		NewCadence,
		0,
		GetCinderlineCadenceThreshold());
	if (ClampedCadence == CinderlineCadence)
	{
		return;
	}

	const int32 OldCadence = CinderlineCadence;
	CinderlineCadence = ClampedCadence;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (OwnerPawn->IsLocallyControlled()
			&& OwnerPawn->GetNetMode() != NM_DedicatedServer)
		{
			OnCinderlineCadenceChanged.Broadcast(
				OldCadence,
				CinderlineCadence,
				GetCinderlineCadenceThreshold());
		}
	}
}

float USovTarrikEchoGenerationComponent::GetWorldTimeSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void USovTarrikEchoGenerationComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovTarrikEchoGenerationComponent::HandleDealtDamage(
	UNarrativeAbilitySystemComponent* DamagedAbilitySystem,
	const float Damage,
	const FGameplayEffectSpec& EffectSpec)
{
	if (!CanGenerateCinderlineEcho()
		|| !IsValid(DamagedAbilitySystem)
		|| Damage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (PendingCadenceWeapon.IsValid())
	{
		if (const URangedWeaponItem* DamageSourceWeapon =
			Cast<URangedWeaponItem>(EffectSpec.GetContext().GetSourceObject()))
		{
			if (DamageSourceWeapon != PendingCadenceWeapon.Get())
			{
				ResetCinderlineCadenceInternal();
			}
		}
	}

	URangedWeaponItem* SourceWeapon = nullptr;
	if (!IsQualifyingPrimaryFire(EffectSpec, SourceWeapon))
	{
		return;
	}

	AActor* TargetActor = DamagedAbilitySystem->GetAvatarActor();
	if (!IsValid(TargetActor)
		|| TargetActor == GetOwner()
		|| UArsenalStatics::GetAttitude(GetOwner(), TargetActor) != ETeamAttitude::Hostile)
	{
		return;
	}

	const bool bPrecisionHit = IsPrecisionHit(EffectSpec);
	const bool bBossTarget = IsBossTarget(DamagedAbilitySystem);
	const FHitResult* HitResult = EffectSpec.GetContext().GetHitResult();
	const int32 Contribution = SovCinderlineCadencePolicy::Contribution(bPrecisionHit, PrecisionHitCadence, BodyHitCadence);
	// A body hit worth no cadence must not keep an open cadence alive either, or volume would still
	// carry a run of precision hits that the player never finished.
	if (Contribution > 0)
	{
		AddCinderlineCadence(
			Contribution,
			SourceWeapon,
			bBossTarget,
			bPrecisionHit,
			HitResult ? HitResult->BoneName : NAME_None);
	}

	if (bPrecisionHit && DamagedAbilitySystem->IsDead())
	{
		const float TargetMultiplier = bBossTarget
			? FMath::Clamp(BossEchoMultiplier, 0.0f, 1.0f)
			: 1.0f;
		AwardEcho(
			FMath::Max(PrecisionKillEchoReward, 0.0f) * TargetMultiplier,
			FSovGameplayTags::Get().Echo_Source_CinderlinePrecisionKill,
			ESovCinderlineEchoAwardType::PrecisionKill,
			bBossTarget && TargetMultiplier < 1.0f - KINDA_SMALL_NUMBER);
	}
}

void USovTarrikEchoGenerationComponent::HandleEchoChanged(
	const float OldEcho,
	const float NewEcho,
	const float MaxEcho)
{
	static_cast<void>(OldEcho);
	if (GetOwner()
		&& GetOwner()->HasAuthority()
		&& MaxEcho > KINDA_SMALL_NUMBER
		&& NewEcho + KINDA_SMALL_NUMBER >= MaxEcho)
	{
		ResetCinderlineCadenceInternal();
	}
}

void USovTarrikEchoGenerationComponent::HandleBlockingTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	if (NewCount > 0 && GetOwner() && GetOwner()->HasAuthority())
	{
		ResetCinderlineCadenceInternal();
	}
}

void USovTarrikEchoGenerationComponent::OnRep_CinderlineCadence(
	const int32 OldCadence)
{
	OnCinderlineCadenceChanged.Broadcast(
		OldCadence,
		CinderlineCadence,
		GetCinderlineCadenceThreshold());
}

void USovTarrikEchoGenerationComponent::ClientNotifyCinderlineEchoAwarded_Implementation(
	const float AwardedEcho,
	const float NewEcho,
	const ESovCinderlineEchoAwardType AwardType,
	const bool bBossReduced)
{
	OnCinderlineEchoAwarded.Broadcast(
		AwardedEcho,
		NewEcho,
		AwardType,
		bBossReduced);
}

void USovTarrikEchoGenerationComponent::ClientNotifyCinderlineHitConfirmed_Implementation(
	const int32 NewCadence,
	const bool bPrecisionHit,
	const FName HitBone,
	const bool bBossReduced)
{
	OnCinderlineHitConfirmed.Broadcast(
		NewCadence,
		GetCinderlineCadenceThreshold(),
		bPrecisionHit,
		HitBone,
		bBossReduced);
}

void USovTarrikEchoGenerationComponent::HandleEncounterScopeChanged(bool bStarted)
{
	++ResourceScopeEpoch;
	ProtectionSourceAwardTimes.Reset();
	HeavyAttacks.Reset();
	ResetCinderlineCadence();
	LastCadenceAwardWorldTime = -BIG_NUMBER;
}

bool USovTarrikEchoGenerationComponent::CanGenerateTarrikEcho() const
{
	if (!IsInitialized() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| !GetOwner()->HasAuthority() || AbilitySystemComponent->GetAvatarActor() != GetOwner()) return false;
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	return AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Fatal)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_EchoAbility_Active);
}

void USovTarrikEchoGenerationComponent::AwardTarrikEcho(float Amount, FGameplayTag Tag,
	ESovTarrikEchoAwardType Type, AActor* Target)
{
	if (!CanGenerateTarrikEcho()) return;
	USovEchoComponent* OriginalEcho = EchoComponent.Get();
	UNarrativeAbilitySystemComponent* OriginalASC = AbilitySystemComponent.Get();
	const uint32 Epoch = ResourceScopeEpoch;
	const float BeforeEcho = OriginalEcho->GetEcho();
	const float Granted = OriginalEcho->AddEcho(Amount, Tag);
	if (ResourceScopeEpoch != Epoch || EchoComponent.Get() != OriginalEcho
		|| AbilitySystemComponent.Get() != OriginalASC || !IsValid(OriginalEcho) || !IsValid(GetOwner())) return;
	if (Granted > KINDA_SMALL_NUMBER) ClientNotifyTarrikEchoAwarded(Granted, BeforeEcho + Granted, Type, Target);
}

void USovTarrikEchoGenerationComponent::HandleDamageResolvedAsSource(const FSovDamageResult& Result)
{
	AActor* Target = Result.TargetActor.Get();
	if (!GetOwner() || !GetOwner()->HasAuthority() || Result.SourceActor.Get() != GetOwner()
		|| !IsValid(Target) || !Result.IsCurrentTargetLife() || !Result.TransactionId.IsValid()
		|| ConsumedCombatTransactions.Contains(Result.TransactionId)) return;
	ConsumedCombatTransactions.Add(Result.TransactionId);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	const UGameplayAbility* SourceAbility = Result.EffectContext.GetAbility();
	if (Result.bFromEchoAbility || (SourceAbility && SourceAbility->GetAssetTags().HasTag(Tags.Ability_Echo))
		|| UArsenalStatics::GetAttitude(GetOwner(), Target) != ETeamAttitude::Hostile
		|| !Result.IsCurrentTargetLife()) return;

	const uint32 ExpectedScope = ResourceScopeEpoch;
	bool bHeavyReward = false;
	const USovEchoAttackReceipt* Receipt = Cast<USovEchoAttackReceipt>(Result.EffectContext.GetSourceObject());
	if (!Result.bPeriodicDamage && Result.AttackId.IsValid() && !ConsumedHeavyAttacks.Contains(Result.AttackId) && Receipt && Receipt->MatchesCommittedHeavyAttack(GetOwner(), Result.AttackId)
		&& Result.AttackClassifications.HasTag(Tags.Damage_Heavy)
		&& Result.AppliedHealthDamage + Result.AppliedShieldDamage + Result.AppliedPoiseDamage > 0.0f)
	{
		FHeavyAttackProgress& Progress = HeavyAttacks.FindOrAdd(Result.AttackId);
		Progress.Targets.Add(Target);
		bHeavyReward = !Progress.bConsumed && Progress.Targets.Num() >= 3;
		if (bHeavyReward)
		{
			Progress.bConsumed = true;
			ConsumedHeavyAttacks.Add(Result.AttackId);
		}
	}
	// All reward eligibility is captured before delegates can cause reentrant damage.
	if (bHeavyReward) AwardTarrikEcho(8.0f, Tags.Echo_Source_HeavyMultiHit, ESovTarrikEchoAwardType::HeavyMultiHit, Target);
	if (ResourceScopeEpoch != ExpectedScope || !Result.IsCurrentTargetLife()) return;
	if (Result.bPoiseBroken) AwardTarrikEcho(15.0f, Tags.Echo_Source_PoiseBreak, ESovTarrikEchoAwardType::PoiseBreak, Target);
	if (ResourceScopeEpoch != ExpectedScope || !Result.IsCurrentTargetLife()) return;
	if (Result.bFatal && Result.AppliedHealthDamage > 0.0f
		&& Result.TargetTagsBeforeDamage.HasTag(Tags.State_CommandTarget_Window))
		AwardTarrikEcho(8.0f, Tags.Echo_Source_CommandTargetKill, ESovTarrikEchoAwardType::CommandTargetKill, Target);
}

void USovTarrikEchoGenerationComponent::ClientNotifyTarrikEchoAwarded_Implementation(float Amount, float NewEcho,
	ESovTarrikEchoAwardType Type, AActor* Target)
{
	OnTarrikEchoAwarded.Broadcast(Amount, NewEcho, Type, Target);
}

void USovTarrikEchoGenerationComponent::ConsumeProtectionIntercept(USovProtectionInterceptReceipt* Receipt,
	const FSovDamageResult& Result)
{
	AActor* Threat = nullptr;
	AActor* Protected = nullptr;
	if (!IsValid(Receipt) || !GetOwner() || !GetOwner()->HasAuthority()
		|| !Receipt->ConsumeForProtector(GetOwner(), Result, Threat, Protected)
		|| !Result.TransactionId.IsValid() || ConsumedProtectionTransactions.Contains(Result.TransactionId)) return;
	ConsumedProtectionTransactions.Add(Result.TransactionId);
	if (!CanGenerateTarrikEcho() || !IsValid(Threat) || !IsValid(Protected)
		|| !FMath::IsFinite(ProtectionInterceptSourceCooldown)) return;
	const FGameplayTag Tag = FSovGameplayTags::Get().Echo_Source_ProtectionIntercept;
	EchoComponent->RecordCombatActivity(Tag);
	const float Now = GetWorldTimeSeconds();
	if (const float* Previous = ProtectionSourceAwardTimes.Find(Threat))
		if (!SovProtectionAwardPolicy::HasCooldownElapsed(Now, *Previous, ProtectionInterceptSourceCooldown)) return;
	for (auto It = ProtectionSourceAwardTimes.CreateIterator(); It; ++It)
		if (!It.Key().IsValid()) It.RemoveCurrent();
	ProtectionSourceAwardTimes.Add(Threat, Now);
	AwardTarrikEcho(15.f, Tag, ESovTarrikEchoAwardType::ProtectionIntercept, Protected);
}
