// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtagonistPartitionTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"

ASovPartitionTestPawn::ASovPartitionTestPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	// The incoming pawn spawns at the outgoing pawn's transform before the source is destroyed.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ASovPartitionTestPawn::StageContentReadiness(ASovPlayerState* State)
{
	if (!IsValid(State) || GetPlayerState() != State || State->GetPawn() != this || !GetPlayerDefinition()) { return false; }
	AbilitySystemComponent = Cast<UNarrativeAbilitySystemComponent>(State->GetAbilitySystemComponent());
	AttributeSetBase = State->GetAttributeSetBase();
	if (!AbilitySystemComponent || !AttributeSetBase) { return false; }
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(State, this);
	// Only the first pawn on a fresh PlayerState receives a baseline. Later pawns see whatever
	// the shared ASC still holds, so a production reset (or its absence) stays observable.
	if (AbilitySystemComponent->GetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute()) <= 0.f)
	{
#define SOV_BASELINE_RESOURCE(Name) AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute(), 100.f); AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), 50.f);
		SOV_BASELINE_RESOURCE(Health)
		SOV_BASELINE_RESOURCE(Shield)
		SOV_BASELINE_RESOURCE(Stamina)
		SOV_BASELINE_RESOURCE(Poise)
		SOV_BASELINE_RESOURCE(Echo)
#undef SOV_BASELINE_RESOURCE
	}
	// Production identity path: definition-owned tags carry the protagonist tag.
	OnDefinitionSet_Implementation(GetPlayerDefinition());
	InitializedAbilitySystem = AbilitySystemComponent;
	InitializedPlayerDefinition = GetPlayerDefinition();
	HandleAbilitySystemReady(AbilitySystemComponent);
	bProjectSystemsInitialized = AreAdditionalCharacterSystemsReady();
	bAuthoritativeGameplayInitialized = true;
	bAbilitySystemReadyPublished = true;
	bVisualReadyForGameplay = true;
	bInitialPlayerDataApplied = false;
	return IsCampaignDataReadyToApply();
}

FGameplayTag ASovPartitionTestTarrik::GetProtagonistIdentityTag() const { return FSovGameplayTags::Get().Character_Player_Tarrik; }
FGameplayTag ASovPartitionTestSelene::GetProtagonistIdentityTag() const { return FSovGameplayTags::Get().Character_Player_Selene; }
