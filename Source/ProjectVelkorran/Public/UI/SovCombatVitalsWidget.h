// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SovCombatVitalsWidget.generated.h"

class ASovPlayerController;
class APawn;
class UBorder;
class UTextBlock;
class UProgressBar;

struct FSovCombatVitalValue
{
    float Current = 0.f;
    float Maximum = 0.f;
};

/** Read-only projection; cleared whenever the current pawn is not admitted. */
struct FSovCombatVitalsSnapshot
{
    TWeakObjectPtr<APawn> Pawn;
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
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ValueLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> Bars;
    FSovCombatVitalsSnapshot Displayed;
    float DisplayedScale = -1.f;
    bool bDisplayedHighContrast = false;
};
