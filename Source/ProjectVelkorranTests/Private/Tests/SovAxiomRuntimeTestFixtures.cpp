// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"

#include "Components/CapsuleComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "Tests/SovBotAttackTestFixtures.h"

ASovAxiomRuntimeTestCharacter::ASovAxiomRuntimeTestCharacter(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("TestASC"));
	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("TestAttributes"));
	TestEcho = CreateDefaultSubobject<USovEchoComponent>(TEXT("TestEcho"));
	TestDeflection = CreateDefaultSubobject<USovDeflectionComponent>(TEXT("TestDeflection"));
	TestEchoGeneration = CreateDefaultSubobject<USovSeleneEchoGenerationComponent>(TEXT("TestSeleneGeneration"));
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASovAxiomRuntimeTestCharacter::InitializeTestCombat(const int32 InTeam)
{
	TestTeam = InTeam;
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxEchoAttribute(), 100.f);
	TestEcho->InitializeWithAbilitySystem(AbilitySystemComponent);
	TestDeflection->InitializeWithAbilitySystem(AbilitySystemComponent);
	TestEchoGeneration->InitializeWithAbilitySystem(AbilitySystemComponent);
	TestEcho->RestoreEchoFromCheckpoint(100.f);
	AbilitySystemComponent->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::RecordDamage);
	if (InTeam == 0)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
	}
}

UWeaponItem* ASovAxiomRuntimeTestCharacter::SetTestWeapon()
{
	// Exercise Narrative's existing fallback without requiring inventory/visual assets.
	// The runtime ability still verifies its actual source item is currently wielded.
	EquipmentComp = nullptr;
	EquippedWeapon = NewObject<USovAxiomRuntimeTestWeapon>(this);
	return EquippedWeapon;
}

void ASovAxiomRuntimeTestCharacter::RemoveTestWeapon()
{
	EquippedWeapon = nullptr;
}

ETeamAttitude::Type ASovAxiomRuntimeTestCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	const ASovAxiomRuntimeTestCharacter* OtherCharacter = Cast<ASovAxiomRuntimeTestCharacter>(&Other);
	if (OtherCharacter && OtherCharacter->bTestNeutral) { return ETeamAttitude::Neutral; }
	return &Other == this || (OtherCharacter && OtherCharacter->TestTeam == TestTeam)
		? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

FGameplayTagContainer ASovAxiomRuntimeTestCharacter::GetFactions() const
{
	return FGameplayTagContainer();
}

void ASovAxiomRuntimeTestCharacter::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
	Location = GetActorLocation() + TestEyeOffset;
	Rotation = GetActorRotation();
}

USovAxiomRuntimeTestAbility::USovAxiomRuntimeTestAbility()
{
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
}

USovAxiomRuntimeTestCommandLink::USovAxiomRuntimeTestCommandLink()
{
	LinkId = TEXT("AxiomRuntimeTestLink");
	bStartsActive = false;
}

USovAxiomRuntimeTestWeapon::USovAxiomRuntimeTestWeapon()
{
	WieldedSlot = FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand;
}

USovSavedKitRuntimeTestWeapon::USovSavedKitRuntimeTestWeapon()
{
	SetTestHolsteredKit({ USovBotTestAttackAlpha::StaticClass() });
}

void ASovAxiomRuntimeTestCharacter::RecordDamage(const FSovDamageResult& Result)
{
	++ResolvedHitCount;
	LastDamageResult = Result;
	if (ReentrantPulse.IsValid())
	{
		if (bCancelPulseOnDamage) { ReentrantPulse->FinishEchoAbility(true); }
		else { bReentrantReleaseAccepted = ReentrantPulse->ReleaseAxiomNullPulseFromAim(); }
	}
}
