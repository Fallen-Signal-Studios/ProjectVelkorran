// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDefenseLifecycleTestFixtures.h"

#include "Components/SovDeflectionComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Class.h"

ASovDefenseLifecycleTestCharacter::ASovDefenseLifecycleTestCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TestShield = CreateDefaultSubobject<USovShieldComponent>(TEXT("TestShield"));
	TestPoise = CreateDefaultSubobject<USovPoiseComponent>(TEXT("TestPoise"));
}

void ASovDefenseLifecycleTestCharacter::InitializeDefenseCombat()
{
	InitializeTestCombat(0);
	TestShield->InitializeWithAbilitySystem(GetNarrativeAbilitySystemComponent());
	TestPoise->InitializeWithAbilitySystem(GetNarrativeAbilitySystemComponent());
	TestDeflection->OnDeflectionStarted.AddUniqueDynamic(this, &ThisClass::ObserveDeflectionStarted);
	TestDeflection->OnDeflectionWindowClosed.AddUniqueDynamic(this, &ThisClass::ObserveDeflectionClosed);
}

void ASovDefenseLifecycleTestCharacter::ObserveDeflectionStarted()
{
	++DeflectionStartCount;
	if (bCancelDeflectionOnStart)
	{
		bCancelDeflectionOnStart = false;
		GetNarrativeAbilitySystemComponent()->CancelAbilityHandle(DeflectionHandle);
	}
}
void ASovDefenseLifecycleTestCharacter::ObserveDeflectionClosed() { ++DeflectionCloseCount; }

void ASovDefenseLifecycleTestCharacter::InterruptBlastSource(const FSovDamageResult& Result)
{
	if (BlastSourceToInterrupt.IsValid())
	{
		auto* ASC = BlastSourceToInterrupt->GetNarrativeAbilitySystemComponent();
		BlastSourceToInterrupt.Reset();
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Fatal);
	}
}

void USovDefenseLifecycleDeflection::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (Function && Function->GetFName() == TEXT("ReceiveDeflectionAbilityStarted")) { ++StartedHookCount; }
	Super::ProcessEvent(Function, Parameters);
}

USovDefenseLifecycleRocket::USovDefenseLifecycleRocket()
{
	bAutoReleasePayload = false;
	CooldownDuration = 0.f;
}

void USovDefenseLifecycleRocket::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (Function && Function->GetFName() == TEXT("ReceiveDroneWeaponPayloadReleased"))
	{
		++ReleasedHookCount;
		if (bInterruptOnRelease)
		{
			bInterruptOnRelease = false;
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				const FGameplayTag Tag = FSovGameplayTags::Get().State_Status_DeviceDisabled;
				ASC->AddLooseGameplayTag(Tag);
				ASC->RemoveLooseGameplayTag(Tag);
			}
		}
	}
	Super::ProcessEvent(Function, Parameters);
}
