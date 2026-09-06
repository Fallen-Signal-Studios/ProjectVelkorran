// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovBotAttackTestFixtures.h"
#include "Components/CapsuleComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"

ASovBotTestCharacter::ASovBotTestCharacter(const FObjectInitializer& Initializer)
	: Super(Initializer.SetDefaultSubobjectClass<USovBotTestASC>(TEXT("AbilitySystemComponent")))
{
	TestActorGuid = FGuid::NewGuid();
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASovBotTestCharacter::InitializeTestCombat(const int32 Team)
{
	TestTeam = Team;
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
}

ETeamAttitude::Type ASovBotTestCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	const ASovBotTestCharacter* Character = Cast<ASovBotTestCharacter>(&Other);
	return &Other == this || (Character && Character->TestTeam == TestTeam) ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void ASovBotTestCharacter::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
	Location = GetActorLocation();
	Rotation = GetActorRotation();
}

USovBotTestAttackAlpha::USovBotTestAttackAlpha()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRequiresAmmo = false;
	bBotRequiresAttackToken = false;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Attack;
	DefaultBotAttackRange = 1000.f;
	DefaultBotAttackFrequency = 1.f;
	ActivationOwnedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
}

bool USovBotTestAttackAlpha::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* FailureTags) const
{
	return !bRejectActivation && Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, FailureTags);
}

void USovBotTestAttackAlpha::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
	if (!CommitAbility(Handle, Info, ActivationInfo)) { EndAbility(Handle, Info, ActivationInfo, false, true); return; }
	++ActivationCount;
	Super::ActivateAbility(Handle, Info, ActivationInfo, Event);
}

void USovBotTestAttackAlpha::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	++ReleaseCount;
	Super::InputReleased(Handle, Info, ActivationInfo);
}

void USovBotTestAttackAlpha::FinishTestAttack()
{
	if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false); }
}

USovBotTestAttackBeta::USovBotTestAttackBeta()
{
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	DefaultBotAttackRange = 1600.f;
	MinimumTestRange = 400.f;
}
