// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Exertion/SovExertionComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "NarrativeGameplayTags.h"

ASovExertionRuntimeTestCharacter::ASovExertionRuntimeTestCharacter(const FObjectInitializer& Initializer) : Super(Initializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("ExertionTestASC"));
	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("ExertionTestAttributes"));
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
}

void ASovExertionRuntimeTestCharacter::InitializeExertion(bool bSelene)
{
	bTestSelene = bSelene;
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
	AbilitySystemComponent->AddLooseGameplayTag(GetProtagonistIdentityTag());
	GetEchoComponent()->InitializeWithAbilitySystem(AbilitySystemComponent);
	GetExertionComponent()->InitializeWithAbilitySystem(AbilitySystemComponent);
	bCharacterReady = true;
	GetExertionComponent()->OnStaminaSpent.AddUniqueDynamic(this, &ThisClass::ObserveSpend);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

FGameplayTag ASovExertionRuntimeTestCharacter::GetProtagonistIdentityTag() const
{
	return bTestSelene ? FSovGameplayTags::Get().Character_Player_Selene : FSovGameplayTags::Get().Character_Player_Tarrik;
}

void ASovExertionRuntimeTestCharacter::ObserveSpend(float Amount, float Remaining)
{
	static_cast<void>(Amount); static_cast<void>(Remaining);
	if (bCancelOnSpend) { AbilitySystemComponent->CancelAllAbilities(); }
}

USovExertionRuntimeChargedAbility::USovExertionRuntimeChargedAbility()
{
	bRequiresAmmo = false;
	bRequiresChargedRelease = true;
	MinimumStaminaChargeTier = 2;
	ChargedReleaseStaminaCost = 20.f;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void USovExertionRuntimeChargedAbility::DispatchTestHit()
{
	FinalizeTargetData(FGameplayAbilityTargetDataHandle(), FGameplayTag());
}

void USovExertionRuntimeChargedAbility::HandleTargetData_Implementation(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Tag)
{
	static_cast<void>(Data); static_cast<void>(Tag); ++Dispatches;
}

USovInputReentryTestAbility::USovInputReentryTestAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Attack;
}
void USovInputReentryTestAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	Super::ActivateAbility(Handle, Info, Activation, Event);
	Info->AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, Handle, Activation.GetActivationPredictionKey())
		.AddUObject(this, &ThisClass::ObserveReplicatedRelease);
}
void USovInputReentryTestAbility::InputReleased(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation)
{
	Super::InputReleased(Handle, Info, Activation);
	if (!bReenterOnce) { return; }
	bReenterOnce = false;
	auto* ASC = Info->AbilitySystemComponent.Get();
	ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, Handle, Activation.GetActivationPredictionKey()).RemoveAll(this);
	EndAbility(Handle, Info, Activation, false, false);
	if (auto* Spec = ASC->FindAbilitySpecFromHandle(Handle)) { Spec->InputPressed = true; }
	ASC->TryActivateAbility(Handle, false);
}
