// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/SovCombatHUDQuiet.h"
#include "Settings/SovGameUserSettings.h"
#include "SovCombatVitalsWidget.generated.h"

class ASovPlayerController;
class APawn;
class UBorder;
class UTextBlock;
class UProgressBar;
class UVerticalBox;
class USovCombatReadinessWidget;
class USizeBox;
class UPlayerInteractionComponent;
class UNarrativeInteractionComponent;
class USovGameUserSettings;

struct FSovCombatVitalValue
{
    float Current = 0.f;
    float Maximum = 0.f;
};

/** Read-only projection; cleared whenever the current pawn is not admitted. */
struct FSovCombatVitalsSnapshot
{
    TWeakObjectPtr<APawn> Pawn;
    FGameplayTag Protagonist;
    FSovCombatVitalValue Values[5]; // Health, Shield, Stamina, Poise, Echo.
};

/** Optional noninteractive combat readout. The pawn/ASC remain the resource owners. */
UCLASS()
class PROJECTVELKORRAN_API USovCombatVitalsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USovCombatVitalsWidget(const FObjectInitializer& Initializer);
    static bool ReadCurrentVitals(const ASovPlayerController* Controller, FSovCombatVitalsSnapshot& Out);
    void RefreshVitals();
    /** Current displayed panels only; callers must check owner and rendered state. */
    const UBorder* GetSurvivalPanel() const { return Panel; }
    const UBorder* GetEchoPanel() const { return EchoPanel; }
protected:
    virtual void NativeDestruct() override;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    friend struct FSovCombatHUDQuietTestAccess;
    void BindQuietSources(ASovPlayerController* Controller);
    void RetireQuietSources();
    UFUNCTION() void HandleQuietInput(FGameplayTag InputTag, bool bPressed);
    UFUNCTION() void HandleQuietInteraction(UNarrativeInteractionComponent* Interaction);
    UFUNCTION() void HandleQuietSettings(const FSovUserSettingsSnapshot& Settings);
    void WakeQuietHUD();
    TWeakObjectPtr<ASovPlayerController> QuietController;
    TWeakObjectPtr<UPlayerInteractionComponent> QuietInteraction;
    TWeakObjectPtr<USovGameUserSettings> QuietSettings;
    SovCombatHUDQuiet::FState QuietState;
    double NextThreatRead = -1.;
    bool bCachedThreat = false;
    uint64 QuietActorInfoEpoch = 0;
    UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
    UPROPERTY(Transient) TObjectPtr<UBorder> EchoPanel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> IdentityLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EchoLabel;
    UPROPERTY(Transient) TArray<TObjectPtr<UVerticalBox>> VitalRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ValueLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> Bars;
    UPROPERTY(Transient) TObjectPtr<USovCombatReadinessWidget> AbilityReadiness;
    UPROPERTY(Transient) TObjectPtr<USizeBox> AbilityReadinessSize;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CompanionStatus;
    FSovCombatVitalsSnapshot Displayed;
    float DisplayedScale = -1.f;
    bool bDisplayedHighContrast = false;
};
