// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDroneContinuationTestFixtures.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "UObject/Class.h"

namespace
{
	void InvokeOnce(TFunction<void()>& Callback)
	{
		TFunction<void()> OwnedCallback = MoveTemp(Callback);
		if (OwnedCallback) { OwnedCallback(); }
	}
}

void FSovDroneContinuationHooks::Dispatch(UFunction* Function)
{
	if (!Function) { return; }
	const FName Name = Function->GetFName();
	if (Name == TEXT("ReceiveDroneWeaponStarted")) { ++Started; InvokeOnce(OnStarted); }
	else if (Name == TEXT("ReceiveDroneWeaponPayloadReleased")) { ++Releases; InvokeOnce(OnRelease); }
	else if (Name == TEXT("ReceiveDroneWeaponEnded")) { ++Ends; }
	else if (Name == TEXT("ReceiveDetonationWarningBegan")) { ++Warnings; InvokeOnce(OnWarning); }
}

void FSovDroneContinuationHooks::Spawned() { ++Spawns; InvokeOnce(OnSpawned); }
void FSovDroneContinuationHooks::Detonated() { ++Detonations; InvokeOnce(OnDetonated); }

FGameplayEffectSpecHandle USovDroneContinuationASC::MakeOutgoingSpec(TSubclassOf<UGameplayEffect> EffectClass,
	float Level, FGameplayEffectContextHandle Context) const
{
	FGameplayEffectSpecHandle Spec = Super::MakeOutgoingSpec(EffectClass, Level, Context);
	if (OnMakeSpec)
	{
		if (SpecsBeforeCallback > 0) { --SpecsBeforeCallback; }
		else { InvokeOnce(OnMakeSpec); }
	}
	return Spec;
}

ASovDroneContinuationCharacter::ASovDroneContinuationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USovDroneContinuationASC>(TEXT("TestASC")))
{
}

USovDroneContinuationGun::USovDroneContinuationGun()
{
	bAutoReleasePayload = false;
	CooldownDuration = 0.f;
	SpreadDegrees = 0.f;
	FallbackMuzzleOffset = FVector::ZeroVector;
	GunshotPresentationClass = ASovDroneContinuationGunPresentation::StaticClass();
}

void USovDroneContinuationGun::ProcessEvent(UFunction* Function, void* Parameters)
{
	Hooks.Dispatch(Function);
	Super::ProcessEvent(Function, Parameters);
}

void USovDroneContinuationGun::UnlockEnd()
{
	check(ScopeLockCount > 0);
	if (--ScopeLockCount == 0)
	{
		auto Pending = MoveTemp(WaitingToExecute);
		for (auto& Callback : Pending) { Callback.ExecuteIfBound(); }
	}
}

USovDroneContinuationRocket::USovDroneContinuationRocket()
{
	bAutoReleasePayload = false;
	CooldownDuration = 0.f;
	bEnableHoming = false;
	RocketClass = ASovDroneContinuationRocketProjectile::StaticClass();
}

void USovDroneContinuationRocket::ProcessEvent(UFunction* Function, void* Parameters)
{
	Hooks.Dispatch(Function);
	Super::ProcessEvent(Function, Parameters);
}

USovDroneContinuationExploder::USovDroneContinuationExploder()
{
	bAutoReleasePayload = false;
	CooldownDuration = 0.f;
	bOnlyAcquirePlayerControlledTargets = false;
	DetonationWarningDuration = .2f;
	bExplosionRequiresLineOfSight = false;
	PresentationClass = ASovDroneContinuationBlastPresentation::StaticClass();
}

void USovDroneContinuationExploder::ProcessEvent(UFunction* Function, void* Parameters)
{
	Hooks.Dispatch(Function);
	Super::ProcessEvent(Function, Parameters);
}

void ASovDroneContinuationGunPresentation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		if (auto* Spec = ASC->FindAbilitySpecFromClass(USovDroneContinuationGun::StaticClass()))
		{
			if (auto* Ability = Cast<USovDroneContinuationGun>(Spec->GetPrimaryInstance())) { Ability->Hooks.Spawned(); }
		}
	}
}

void ASovDroneContinuationRocketProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		if (auto* Spec = ASC->FindAbilitySpecFromClass(USovDroneContinuationRocket::StaticClass()))
		{
			if (auto* Ability = Cast<USovDroneContinuationRocket>(Spec->GetPrimaryInstance())) { Ability->Hooks.Spawned(); }
		}
	}
}

void ASovDroneContinuationBlastPresentation::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (Function && Function->GetFName() == TEXT("ReceiveDetonated"))
	{
		if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
		{
			if (auto* Spec = ASC->FindAbilitySpecFromClass(USovDroneContinuationExploder::StaticClass()))
			{
				if (auto* Ability = Cast<USovDroneContinuationExploder>(Spec->GetPrimaryInstance())) { Ability->Hooks.Detonated(); }
			}
		}
	}
	Super::ProcessEvent(Function, Parameters);
}
