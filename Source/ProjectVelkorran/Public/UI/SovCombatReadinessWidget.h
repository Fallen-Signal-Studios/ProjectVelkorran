// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "SovCombatReadinessWidget.generated.h"

class ASovPlayerController;
class APawn;
class UNarrativeAbilityInputMapping;
class UEnhancedInputLocalPlayerSubsystem;

enum class ESovAbilityHUDState : uint8 { EchoReady, NeedsEcho, Cooldown, Active, InputLocked, Unbound, WeaponRequired, Unavailable };
enum class ESovAbilityHUDIcon : uint8
{
    Unknown, CinderGrenade, Hunger, Judgement, Slam, Requiem,
    Stillpoint, Wake, Staccato, NullPulse, Dispatch
};

/** Granted ability facts only. EchoReady never promises target-specific activation. */
struct FSovAbilityHUDEntry
{
    FGameplayAbilitySpecHandle Handle;
    FGameplayTag InputTag;
    FText Name, Binding, Status;
    int32 SemanticSlot = 0;
    ESovAbilityHUDState State = ESovAbilityHUDState::Unavailable;
    ESovAbilityHUDIcon Icon = ESovAbilityHUDIcon::Unknown;
    float EchoCost = 0.f, EchoRequired = 0.f, EchoCurrent = 0.f;
    float CooldownRemaining = 0.f, CooldownDuration = 0.f;
    bool bCostSatisfied = false;
};

struct FSovCombatReadinessSnapshot
{
    TWeakObjectPtr<APawn> Pawn;
    FGameplayTag Protagonist;
    TArray<FSovAbilityHUDEntry> Abilities;
    FText CompanionText;
};

namespace SovCombatReadiness
{
    /** Reads current native grants, resources and bindings. No activation, input, resource or companion writes. */
    PROJECTVELKORRAN_API bool Read(const ASovPlayerController* Controller, FSovCombatReadinessSnapshot& Out);
    PROJECTVELKORRAN_API FText BindingForInput(const UNarrativeAbilityInputMapping* Schema, FGameplayTag Input,
        const UEnhancedInputLocalPlayerSubsystem* Subsystem, bool bGamepad);
    /** Image identity follows the actual granted native class, including its authored Blueprint children. */
    PROJECTVELKORRAN_API ESovAbilityHUDIcon IconForAbilityClass(const UClass* AbilityClass);
}

/** Compact lower-right child; its parent owns refresh cadence, HUD-hide and safe-zone placement. */
UCLASS()
class PROJECTVELKORRAN_API USovCombatReadinessWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USovCombatReadinessWidget(const FObjectInitializer& Initializer);
    void Present(const FSovCombatReadinessSnapshot& Snapshot);
    float GetPresentationHeight(float Width) const;
    FText GetAccessibleReadinessText() const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    FSovCombatReadinessSnapshot Displayed;
};
