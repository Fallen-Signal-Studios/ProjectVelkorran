// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Progression/SovTechniqueComponent.h"
#include "Sovereign/SovGameplayTags.h"
ASovHandoffRuntimeTestPawn::ASovHandoffRuntimeTestPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
FGameplayTag ASovHandoffRuntimeTestPawn::GetProtagonistIdentityTag() const
{
	return FSovGameplayTags::Get().Character_Player_Tarrik;
}
bool ASovHandoffRuntimeTestPawn::StageTestReadiness(ASovPlayerState* State, bool bVisualReady)
{
	if (!IsValid(State) || GetPlayerState() != State || State->GetPawn() != this) { return false; }
	AbilitySystemComponent = Cast<UNarrativeAbilitySystemComponent>(State->GetAbilitySystemComponent());
	AttributeSetBase = State->GetAttributeSetBase();
	if (!AbilitySystemComponent || !AttributeSetBase) { return false; }
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(State, this);
#define SOV_INIT_RESOURCE(Name) AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute(), 100.f); AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), 50.f);
	SOV_INIT_RESOURCE(Health)
	SOV_INIT_RESOURCE(Shield)
	SOV_INIT_RESOURCE(Stamina)
	SOV_INIT_RESOURCE(Poise)
	SOV_INIT_RESOURCE(Echo)
#undef SOV_INIT_RESOURCE
	AbilitySystemComponent->AddLooseGameplayTag(GetProtagonistIdentityTag());
	InitializedAbilitySystem = AbilitySystemComponent;
	InitializedPlayerDefinition = PlayerDefinition;
	HandleAbilitySystemReady(AbilitySystemComponent);
	bProjectSystemsInitialized = AreAdditionalCharacterSystemsReady();
	bAuthoritativeGameplayInitialized = true;
	bAbilitySystemReadyPublished = true;
	bVisualReadyForGameplay = bVisualReady;
	bInitialPlayerDataApplied = false;
	if (auto* Techniques = Cast<USovTechniqueComponent>(State->GetSkillTreeComponent()))
	{
		// An empty first-entry profile is valid without authored Technique branches.
		// Use the production initializer so its saved identity matches this ASC.
		return Techniques->InitializeNewProtagonist(GetProtagonistIdentityTag()) && Techniques->IsTechniqueStateValid();
	}
	return false;
}
void ASovHandoffRuntimeTestController::SetTestPlayerState(ASovPlayerState* State)
{
	PlayerState = State;
	if (State) { State->SetOwner(this); }
}
