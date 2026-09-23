// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovMeleeValidationLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Melee/SovGameplayAbility_Melee.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "EnhancedInputSubsystems.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/SovPlayerController.h"
#include "UnrealFramework/NarrativePlayerController.h"

TArray<FString> USovMeleeValidationLibrary::GrantedAbilityClassesForInput(UAbilitySystemComponent* AbilitySystem, FGameplayTag InputTag)
{
    TArray<FString> Classes;
    if (!IsValid(AbilitySystem)) { return Classes; }
    for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
    {
        // Narrative routes input by the spec's dynamic source tags, which hold the ability's input tag.
        if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) { Classes.Add(Spec.Ability->GetClass()->GetName()); }
    }
    return Classes;
}

USovGameplayAbility_EchoBase* USovMeleeValidationLibrary::ActiveEchoAbilityForInput(UAbilitySystemComponent* AbilitySystem, FGameplayTag InputTag)
{
    if (!IsValid(AbilitySystem) || !InputTag.IsValid()) { return nullptr; }
    for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
    {
        if (!Spec.IsActive() || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) { continue; }
        if (auto* Ability = Cast<USovGameplayAbility_EchoBase>(Spec.GetPrimaryInstance())) { return Ability; }
    }
    return nullptr;
}

void USovMeleeValidationLibrary::DescribeDamageSource(const FSovDamageResult& Result, FString& SourceObjectClass, FString& SourceAbilityClass)
{
    SourceObjectClass.Reset(); SourceAbilityClass.Reset();
    const UObject* Source = Result.EffectContext.GetSourceObject();
    if (!Source)
    {
        if (const UGameplayAbility* Ability = Result.EffectContext.GetAbility()) { SourceAbilityClass = Ability->GetClass()->GetName(); }
        return;
    }
    SourceObjectClass = Source->GetClass()->GetName();
    if (const UGameplayAbility* Owner = Source->GetTypedOuter<UGameplayAbility>()) { SourceAbilityClass = Owner->GetClass()->GetName(); }
    else if (const UGameplayAbility* Ability = Result.EffectContext.GetAbility()) { SourceAbilityClass = Ability->GetClass()->GetName(); }
}

bool USovMeleeValidationLibrary::PressAndReleaseSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag)
{
    if (!IsValid(PlayerController) || !InputTag.IsValid()) { return false; }
    PlayerController->AbilityInputPressed(InputTag);
    PlayerController->AbilityInputReleased(InputTag);
    return true;
}

bool USovMeleeValidationLibrary::PressSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag)
{
    if (!IsValid(PlayerController) || !InputTag.IsValid()) { return false; }
    PlayerController->AbilityInputPressed(InputTag);
    return true;
}

bool USovMeleeValidationLibrary::ReleaseSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag)
{
    if (!IsValid(PlayerController) || !InputTag.IsValid()) { return false; }
    PlayerController->AbilityInputReleased(InputTag);
    return true;
}

TArray<FString> USovMeleeValidationLibrary::DescribeInputRouting(ANarrativePlayerController* PlayerController)
{
    TArray<FString> Lines;
    const auto* Campaign = Cast<ASovPlayerController>(PlayerController);
    const UNarrativeAbilityInputMapping* Schema = Campaign ? Campaign->GetAbilityHUDInputMappings() : nullptr;
    if (!Schema) { return Lines; }
    const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
    const auto* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    for (const FAbilityInputMappingData& Mapping : Schema->InputAbilities)
    {
        TArray<FString> Keys;
        if (Input && Mapping.InputAction)
        {
            for (const FKey& Key : Input->QueryKeysMappedToAction(Mapping.InputAction)) { Keys.Add(Key.ToString()); }
        }
        Lines.Add(FString::Printf(TEXT("%s tag=%s keys=%s"), Mapping.InputAction ? *Mapping.InputAction->GetName() : TEXT("none"),
            *Mapping.InputTag.ToString(), Keys.Num() ? *FString::Join(Keys, TEXT("+")) : TEXT("unbound")));
    }
    return Lines;
}

ANarrativeNPCCharacter* USovMeleeValidationLibrary::SpawnValidationNPC(UObject* WorldContext, UNPCDefinition* Definition, FTransform Transform)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    // Play worlds only: an editor world would keep a spawned NPC in the level being edited.
    if (!World || !World->IsGameWorld() || !Definition) { return nullptr; }
    UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
    return Characters ? Characters->SpawnNPC(Definition, Transform) : nullptr;
}

TArray<FString> USovMeleeValidationLibrary::DescribeNativeMelee(UAbilitySystemComponent* AbilitySystem)
{
    TArray<FString> Lines;
    if (!IsValid(AbilitySystem)) { return Lines; }
    const UAnimInstance* Anim = AbilitySystem->AbilityActorInfo.IsValid() ? AbilitySystem->AbilityActorInfo->GetAnimInstance() : nullptr;
    const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
    const AActor* Owner = AbilitySystem->GetOwner();
    Lines.Add(FString::Printf(TEXT("ASC owner=%s active=%d tickEnabled=%d tickRegistered=%d interval=%.3f ownerTick=%d ownerBegunPlay=%d"),
        Owner ? *Owner->GetClass()->GetName() : TEXT("none"), AbilitySystem->IsActive() ? 1 : 0, AbilitySystem->IsComponentTickEnabled() ? 1 : 0,
        AbilitySystem->PrimaryComponentTick.IsTickFunctionRegistered() ? 1 : 0, AbilitySystem->GetComponentTickInterval(),
        Owner && Owner->IsActorTickEnabled() ? 1 : 0, Owner && Owner->HasActorBegunPlay() ? 1 : 0));
    for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
    {
        if (!Spec.Ability || !Spec.Ability->IsA<USovGameplayAbility_Melee>()) { continue; }
        const auto* Instance = Cast<USovGameplayAbility_Melee>(Spec.GetPrimaryInstance());
        Lines.Add(FString::Printf(TEXT("%s active=%d node=%d montage=%s"), *Spec.Ability->GetClass()->GetName(), Spec.IsActive() ? 1 : 0,
            Instance ? Instance->GetCurrentNodeIndex() : INDEX_NONE, Montage ? *Montage->GetName() : TEXT("none")));
    }
    return Lines;
}

