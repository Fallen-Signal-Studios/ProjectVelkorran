// Copyright Narrative Tools 2024. 


#include "GAS/NarrativeCombatAbility.h"
#include "GAS/SovExertionProvider.h"
#include "Sovereign/SovGameplayTags.h"
#include <AbilitySystemComponent.h>
#include "Items/InventoryFunctionLibrary.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include <GameFramework/Controller.h>
#include <GameFramework/PlayerController.h>
#include <Engine/World.h>
#include <CollisionQueryParams.h>

#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "NarrativeGameplayTags.h"
#include "Items/RangedWeaponItem.h"
#include "NarrativeLogChannels.h"
#include "Engine/HitResult.h"
#include "Components/EquipmentComponent.h"
#include "Weapons/WeaponVisual.h"
#include "KismetTraceUtils.h"
#include "Character/NarrativeCharacterVisual.h"

#define LOCTEXT_NAMESPACE "NarrativeCombatAbility"

static TAutoConsoleVariable<bool> CVarTransformProviderDebug(
	TEXT("n.gas.TransformProvider.Debug"),
	false,
	TEXT("Debug Transform Providers 0=Off 1=On"),
	ECVF_Default);


static TAutoConsoleVariable<bool> CVarDrawTracesDebug(
	TEXT("n.gas.DrawTraces"),
	false,
	TEXT("Draw combat traces 0=Off 1=On"),
	ECVF_Default);

UNarrativeCombatAbility::UNarrativeCombatAbility()
{
	bRequiresAmmo = true;
	ActivationBlockedTags.AddTag(
		FNarrativeGameplayTags::Get().State_Weapon_Equipping);

	DefaultBotAttackFrequency = 1.f; 
	DefaultBotAttackRange = 10000.f; 
}

void UNarrativeCombatAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::CommitExecute(Handle, ActorInfo, ActivationInfo);
}

bool UNarrativeCombatAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /*= nullptr*/, const FGameplayTagContainer* TargetTags /*= nullptr*/, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/) const
{
	return ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		&& !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Evading)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UNarrativeCombatAbility::GetSovAttackIdentity(const AActor* ExpectedSource, FGuid& OutAttackId) const
{
	OutAttackId.Invalidate();
	if (!IsValid(ExpectedSource) || !ExpectedSource->HasAuthority() || !IsActive()
		|| HasAnyFlags(RF_ClassDefaultObject) || GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced
		|| !CurrentCombatAttackId.IsValid() || CombatAttackSpecHandle != CurrentSpecHandle
		|| !CurrentActorInfo || CurrentActorInfo->AvatarActor.Get() != ExpectedSource
		|| !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}
	OutAttackId = CurrentCombatAttackId;
	return true;
}

bool UNarrativeCombatAbility::CanDispatchNativeAttack() const
{
	return !bCombatEndPending && IsActive() && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		&& CurrentActorInfo->AbilitySystemComponent->GetAvatarActor() == CurrentActorInfo->AvatarActor.Get()
		&& !bDefensiveCancelCommitted && (!bRequiresChargedRelease || bChargedReleaseCommitted);
}

bool UNarrativeCombatAbility::BeginNextSovCombatAttack()
{
	FGuid Previous;
	if (bCombatEndPending || bExertionCommitPending || !GetSovAttackIdentity(GetAvatarActorFromActorInfo(), Previous)) { return false; }
	CurrentCombatAttackId = FGuid::NewGuid();
	bChargedReleaseCommitted = false;
	bDefensiveCancelCommitted = false;
	return true;
}

bool UNarrativeCombatAbility::TryPayAttackExertion(float Cost)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FGuid AttackId;
	if (!FMath::IsFinite(Cost) || Cost < 0.f || bExertionCommitPending || !GetSovAttackIdentity(Avatar, AttackId)) { return false; }
	TGuardValue<bool> Pending(bExertionCommitPending, true);
	TInlineComponentArray<UActorComponent*> Components(Avatar);
	for (UActorComponent* Component : Components)
	{
		if (ISovExertionProvider* Provider = Cast<ISovExertionProvider>(Component))
		{
			if (!Provider->CanSpendExertion(Cost) || !Provider->TrySpendExertion(Cost)) { return false; }
			FGuid CurrentId;
			return GetSovAttackIdentity(Avatar, CurrentId) && CurrentId == AttackId;
		}
	}
	return false;
}

bool UNarrativeCombatAbility::TryCommitChargedRelease(int32 ReleasedChargeTier)
{
	if (!bRequiresChargedRelease || bChargedReleaseCommitted || bDefensiveCancelCommitted
		|| ReleasedChargeTier < 0 || MinimumStaminaChargeTier < 0
		|| !FMath::IsFinite(ChargedReleaseStaminaCost) || ChargedReleaseStaminaCost < 0.f) { return false; }
	const float Cost = ReleasedChargeTier >= MinimumStaminaChargeTier ? ChargedReleaseStaminaCost : 0.f;
	if (!TryPayAttackExertion(Cost)) { return false; }
	bChargedReleaseCommitted = true;
	return true;
}

bool UNarrativeCombatAbility::TryCommitDefensiveCancel(float StaminaCost)
{
	if (bDefensiveCancelCommitted || !TryPayAttackExertion(StaminaCost)) { return false; }
	bDefensiveCancelCommitted = true;
	return true;
}

void UNarrativeCombatAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	CurrentCombatAttackId.Invalidate();
	bChargedReleaseCommitted = false;
	bDefensiveCancelCommitted = false;
	bExertionCommitPending = false;
	bCombatEndPending = false;
	CombatAttackSpecHandle = Handle;
	if (ActorInfo && ActorInfo->IsNetAuthority() && !HasAnyFlags(RF_ClassDefaultObject)
		&& GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		CurrentCombatAttackId = FGuid::NewGuid();
	}
	// Bind target data callback
	UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	check(MyAbilityComponent);

	//We use this method from lyra to avoid using targeting actors and just call the target datas ourselves 
	OnTargetDataReadyCallbackDelegateHandle = MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::FinalizeTargetData);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UNarrativeCombatAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		bCombatEndPending = true;
		if (ScopeLockCount > 0)
		{
			CurrentCombatAttackId.Invalidate();
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}

		CurrentCombatAttackId.Invalidate();
		bChargedReleaseCommitted = false;
		bDefensiveCancelCommitted = false;
		CombatAttackSpecHandle = FGameplayAbilitySpecHandle();
		if (CurrentActorInfo)
		{
			if (UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get())
			{
				// When ability ends, consume target data and remove delegate
				MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).Remove(OnTargetDataReadyCallbackDelegateHandle);
				MyAbilityComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
			}
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}


bool UNarrativeCombatAbility::HasAmmo() const
{
	if (!bRequiresAmmo)
	{
		return true;
	}

	if (UWeaponItem* Weapon = GetAbilityWeapon())
	{
		return Weapon->HasAmmo();
	}

	return false;
}

void UNarrativeCombatAbility::GenerateTargetDataUsingTrace(const FCombatTraceData& TraceData, const FTransform& TraceStart, FGameplayTag ApplicationTag /*= FGameplayTag()*/)
{
	FinalizeTargetData(GetTargetDataUsingTrace(TraceData, TraceStart), ApplicationTag);
}

FGameplayAbilityTargetDataHandle UNarrativeCombatAbility::GetTargetDataUsingTrace(const FCombatTraceData& TraceData, const FTransform& TraceStart)
{
	if (AController* OwnerController = GetOwningController())
	{
		if (OwnerController->IsLocalController())
		{
			const FTransform TargetViewPoint = TraceStart;
			const FVector StartTrace = TargetViewPoint.GetTranslation();
			const float TraceLen = TraceData.TraceDistance + FVector::Dist(GetAvatarActorFromActorInfo()->GetActorLocation(), TargetViewPoint.GetLocation());

			const FVector EndTrace = (TargetViewPoint.GetRotation().Vector() * TraceLen) + StartTrace;

			TArray<FHitResult> Hits = PerformTraceMulti(StartTrace, EndTrace, TraceData.TraceRadius);

			if (!TraceData.bTraceMulti)
			{
				Hits.SetNum(1);

#if ENABLE_DRAW_DEBUG

				if (CVarDrawTracesDebug.GetValueOnGameThread() && Hits.IsValidIndex(0) && IsValid(Hits[0].GetComponent()))
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, *GetNameSafe(Hits[0].GetComponent()));
				}
#endif 
			}

			FGameplayAbilityTargetDataHandle TargetData;

			for (auto& Hit : Hits)
			{
				FGameplayAbilityTargetData_SingleTargetHit* SingleTargetHit = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
				TargetData.Add(SingleTargetHit);
			}

			return TargetData; 
			//FinalizeTargetData(TargetData, ApplicationTag);
		}
	}

	return FGameplayAbilityTargetDataHandle();
}

void UNarrativeCombatAbility::FinalizeTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{
	if (!CanDispatchNativeAttack()) { return; }
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();

	if (const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		FScopedPredictionWindow	ScopedPrediction(ASC);

		//We use this method from lyra to avoid using targeting actors and just call the target datas ourselves 
		FGameplayAbilityTargetDataHandle LocalTargetDataHandle(TargetData);

		//We call this function manually instead of using the target data node, which is inefficient and spawns a target data generating actor. We're basically just overriding that to just do a nice lightweight trace instead! 
		if (CurrentActorInfo->IsLocallyControlled() && !CurrentActorInfo->IsNetAuthority())
		{
			ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), LocalTargetDataHandle, ApplicationTag, ASC->ScopedPredictionKey);
		}

		//Client calls this once, that also makes sense. 
		// then server calls this again from the bound target data coming through from client.  

		const FGameplayAbilitySpecHandle DispatchedHandle = CurrentSpecHandle;
		const FPredictionKey DispatchedKey = CurrentActivationInfo.GetActivationPredictionKey();
		HandleTargetData(LocalTargetDataHandle, ApplicationTag);

		// Target callbacks may end this activation and begin another one.
		ASC->ConsumeClientReplicatedTargetData(DispatchedHandle, DispatchedKey);
	}
}

void UNarrativeCombatAbility::HandleTargetData_Implementation(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{

}

FHitResult UNarrativeCombatAbility::PerformTrace(const FVector& Start, const FVector& End, const float SweepRadius)
{
	auto Settings = UArsenalStatics::GetNarrativeProSettings();
	
	FHitResult Hit;
	ECollisionChannel TraceChannel = Settings->WeaponTraceChannel;

	FCollisionQueryParams CQP;
	CQP.bTraceComplex = true;
	CQP.bReturnPhysicalMaterial = true; 
	CQP.AddIgnoredActor(GetAvatarActorFromActorInfo());
	CQP.TraceTag = FName("CombatTrace");

#if ENABLE_DRAW_DEBUG
	CQP.bDebugQuery = CVarDrawTracesDebug.GetValueOnGameThread();
	GetWorld()->DebugDrawTraceTag = CVarDrawTracesDebug.GetValueOnGameThread() ? CQP.TraceTag : NAME_None;
#endif 
	
	if (FMath::IsNearlyZero(SweepRadius))
	{
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, CQP);
	}
	else
	{
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(SweepRadius);
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, TraceChannel, Sphere, CQP);
	}

#if ENABLE_DRAW_DEBUG
	GetWorld()->DebugDrawTraceTag = NAME_None;

	if (CVarDrawTracesDebug.GetValueOnGameThread() && IsValid(Hit.GetComponent()))
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, *GetNameSafe(Hit.GetComponent()));
	}
#endif 

	return Hit;
}

TArray<FHitResult> UNarrativeCombatAbility::PerformTraceMulti(const FVector& Start, const FVector& End, const float SweepRadius)
{
	if (!CharacterOwner)
	{
		return {}; 
	}

	auto Settings = UArsenalStatics::GetNarrativeProSettings();
	
	TArray<FHitResult> Hits;
	ECollisionChannel TraceChannel = Settings->WeaponTraceChannel;

	FCollisionQueryParams CQP = CharacterOwner->GetIgnoreCharacterParams();
	CQP.bTraceComplex = true;
	CQP.bReturnPhysicalMaterial = true; 
	CQP.TraceTag = FName("CombatTrace");

#if ENABLE_DRAW_DEBUG
	CQP.bDebugQuery = CVarDrawTracesDebug.GetValueOnGameThread();
	GetWorld()->DebugDrawTraceTag = CQP.bDebugQuery ? CQP.TraceTag : NAME_None;
#endif

	if (FMath::IsNearlyZero(SweepRadius))
	{
		GetWorld()->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, CQP);
	}
	else
	{
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(SweepRadius);
		GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, TraceChannel, Sphere, CQP);
	}

#if ENABLE_DRAW_DEBUG
	GetWorld()->DebugDrawTraceTag = NAME_None;
#endif 

	return Hits;
}

//Useful rand cone from lyra! 
FVector VRandConeNormalDistribution(const FVector& Dir, const float ConeHalfAngleRad, const float Exponent)
{
	if (ConeHalfAngleRad > 0.f)
	{
		const float ConeHalfAngleDegrees = FMath::RadiansToDegrees(ConeHalfAngleRad);

		// consider the cone a concatenation of two rotations. one "away" from the center line, and another "around" the circle
		// apply the exponent to the away-from-center rotation. a larger exponent will cluster points more tightly around the center
		const float FromCenter = FMath::Pow(FMath::FRand(), Exponent);
		const float AngleFromCenter = FromCenter * ConeHalfAngleDegrees;
		const float AngleAround = FMath::FRand() * 360.0f;

		FRotator Rot = Dir.Rotation();
		FQuat DirQuat(Rot);
		FQuat FromCenterQuat(FRotator(0.0f, AngleFromCenter, 0.0f));
		FQuat AroundQuat(FRotator(0.0f, 0.0, AngleAround));
		FQuat FinalDirectionQuat = DirQuat * AroundQuat * FromCenterQuat;
		FinalDirectionQuat.Normalize();

		return FinalDirectionQuat.RotateVector(FVector::ForwardVector);
	}
	else
	{
		return Dir.GetSafeNormal();
	}
}

float UNarrativeCombatAbility::GetBotAttackFrequency_Implementation() const
{
	return DefaultBotAttackFrequency;
}

float UNarrativeCombatAbility::GetBotAttackRange_Implementation() const
{
	return DefaultBotAttackRange;
}

bool UNarrativeCombatAbility::IsMainhand_Implementation() const
{
	

	return true; 
}

class UWeaponItem* UNarrativeCombatAbility::GetAbilityWeapon() const
{
	if (UWeaponItem* Weap = Cast<UWeaponItem>(GetCurrentSourceObject()))
	{
		return Weap;
	}

	return GetOwnerEquippedWeapon(IsMainhand());
}

class UNarrativeItem* UNarrativeCombatAbility::GetAbilityWeaponAmmo() const
{
	if (UWeaponItem* Weapon = GetAbilityWeapon())
	{
		return Weapon->GetAmmoSource();
	}

	return nullptr;
}

class AWeaponVisual* UNarrativeCombatAbility::GetAbilityWeaponVisual() const
{
	if (UWeaponItem* Weapon = GetAbilityWeapon())
	{
		if (ANarrativeCharacterVisual* CharVis = GetOwningNarrativeCharacterVisual())
		{
			return CharVis->GetWeaponVisual(Weapon->CurrentSlot);
		}
	}

	return nullptr; 
}

float UNarrativeCombatAbility::GetAttackDamage_Implementation() const
{
	return DefaultAttackDamage;
}

FTransform UNarrativeCombatAbility::ApplyWeaponSpread(const FTransform& InTargetingTransform) const
{
	//Always apply spread to mainhand weapon, we dont want to juggle 2 spreads for dual wield. 
	if (UWeaponItem* Weapon = GetOwnerEquippedWeapon())
	{
		return ApplySpread(InTargetingTransform, Weapon->GetWeaponSpread());
	}

	return InTargetingTransform;
}

FTransform UNarrativeCombatAbility::ApplySpread(const FTransform& ViewPoint, const float Spread) const
{
	//Always apply spread to mainhand weapon, we dont want to juggle 2 spreads for dual wield. 
	if (UWeaponItem* Weapon = GetOwnerEquippedWeapon())
	{
		if (Spread > KINDA_SMALL_NUMBER)
		{
			int32 RandSeed = 1234;

			if (CharacterOwner)
			{
				RandSeed = CharacterOwner->GetCharacterRandomSeed();
			}

			//TODO make seeded server-client random stream - one per player 
			FRandomStream Rand(RandSeed);

			FTransform TargetingTransform = ViewPoint;

			const float WeaponSpreadHalfDeg = Spread * 0.5f;
			const FVector AdjustedAimDir = VRandConeNormalDistribution(TargetingTransform.GetRotation().Vector(), FMath::DegreesToRadians(WeaponSpreadHalfDeg), 1.f); //Rand.VRandCone(TargetingTransform.GetRotation().Vector(), FMath::DegreesToRadians(WeaponSpreadHalfDeg), FMath::DegreesToRadians(WeaponSpreadHalfDeg));

			TargetingTransform.SetRotation(AdjustedAimDir.Rotation().Quaternion());

			return TargetingTransform;
		}
	}

	return ViewPoint;
}

UTargetingTransformProvider::UTargetingTransformProvider()
{

}

FTransform UTargetingTransformProvider::ProvideTargetingTransform_Implementation(const AController* Controller) const
{
	return FTransform();
}

UTargetingTransformProvider_CameraTowardsFocus::UTargetingTransformProvider_CameraTowardsFocus()
{

}

FTransform UTargetingTransformProvider_CameraTowardsFocus::ProvideTargetingTransform_Implementation(const AController* Controller) const
{
	if (Controller)
	{
		if (const APawn* Pawn = Controller->GetPawn())
		{
			FVector EyesLoc;
			FRotator EyesRot;

			FVector PawnLoc = Pawn->GetActorLocation();

			Controller->GetPlayerViewPoint(EyesLoc, EyesRot);

			const FVector AimDir = EyesRot.Vector();
			const FVector FocalLoc = EyesLoc + (AimDir * 1024.f);

			const FVector StartPoint = FocalLoc + (((PawnLoc - FocalLoc) | AimDir) * AimDir);

			return FTransform(EyesRot, StartPoint);
		}
	}

	return FTransform();
}


UTargetingTransformProvider_WeaponTowardsFocus::UTargetingTransformProvider_WeaponTowardsFocus()
{
	bUseWeaponVisualMesh = true;
	//We longer push forward as we instead solve owner collisions by making projectiles ignore owners for a short time. 
	ForwardPushDist = 0.f; 
}

FTransform UTargetingTransformProvider_WeaponTowardsFocus::ProvideTargetingTransform_Implementation(const AController* Controller) const
{
	if (Controller)
	{
		if (const INarrativeCharacterOwner* CharOwner = Cast<INarrativeCharacterOwner>(Controller))
		{
			if (ANarrativeCharacter* NChar = CharOwner->GetNarrativeCharacter())
			{
				//Find where we're looking at, because we're gunna spawn near our weapon and shoot off in that direction. 
				FVector EyesLoc;
				FRotator EyesRot;
				Controller->GetPlayerViewPoint(EyesLoc, EyesRot);

				const FVector Start = EyesLoc;
				const FVector End = (EyesRot.Vector() * 10000.f) + EyesLoc;

				FVector Focal = End;

				FCollisionQueryParams CQP = NChar->GetIgnoreCharacterParams();

				UArsenalSettings* Settings = UArsenalStatics::GetNarrativeProSettings();
				ECollisionChannel TraceChannel = Settings->WeaponTraceChannel;

				FHitResult Hit;
				const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, CQP);

				if (bHit)
				{
					Focal = Hit.ImpactPoint;
				}

				#if WITH_EDITOR

				if (CVarTransformProviderDebug.GetValueOnAnyThread())
				{
				#if ENABLE_DRAW_DEBUG
					DrawDebugLineTraceSingle(GetWorld(), Start, End, EDrawDebugTrace::Type::ForDuration, bHit, Hit, FLinearColor::Green, FLinearColor::Red, 5.f);
				#endif
				}

				#endif 

				USkeletalMeshComponent* MeshToUse = NChar->GetMesh();

				if(bUseWeaponVisualMesh)
				{
					if (UNarrativeCombatAbility* AbilityOuter = Cast<UNarrativeCombatAbility>(GetOuter()))
					{
						if (AWeaponVisual* WeapVis = NChar->GetWieldedWeaponVisual(AbilityOuter->IsMainhand()))
						{
							MeshToUse = WeapVis->GetRelevantWeaponMesh();
						}
					}
					else
					{	//If ability isnt using this just grab mainhand weap
						if (AWeaponVisual* WeapVis = NChar->GetWieldedWeaponVisual())
						{
							MeshToUse = WeapVis->GetRelevantWeaponMesh();
						}
					}
				}

				if (MeshToUse)
				{

					const FVector SocketLoc = MeshToUse->GetSocketTransform(BoneName, RTS_World).GetLocation();
					const FRotator AimRot = (Focal - SocketLoc).GetSafeNormal().Rotation();

					//Move aimloc 200 forward a bit from the weapon - we dont want to spawn projectiles etc inside our weapon as they will hit it. 
					const FVector AimLoc = SocketLoc + (AimRot.Vector() * ForwardPushDist);

					return FTransform(AimRot.Quaternion(), AimLoc, FVector(1.f));
				}

			}
		}


	}

	return FTransform();
}


#undef LOCTEXT_NAMESPACE 

float UNarrativeCombatAbility::GetBotAttackMinimumRange_Implementation() const { return 0.f; }
float UNarrativeCombatAbility::GetBotAttackMaximumRange_Implementation() const { return GetBotAttackRange(); }
bool UNarrativeCombatAbility::RequiresBotAttackToken_Implementation() const { return bBotRequiresAttackToken; }
bool UNarrativeCombatAbility::ManagesBotAttackToken_Implementation() const { return false; }
