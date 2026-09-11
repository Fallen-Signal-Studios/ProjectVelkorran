// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Widgets/NarrativeMenu.h"
#include "SovFatalPresentationRuntimeTestFixtures.generated.h"

/** Records only the external menu surface; native fatal/recovery routing remains production. */
UCLASS(Transient)
class USovFatalPresentationTestMenu : public UNarrativeMenu
{
    GENERATED_BODY()
protected:
    virtual void NativeOnActivated() override {}
    virtual void NativeOnDeactivated() override {}
};

UCLASS(Transient)
class USovFatalPresentationTestHUD : public UNarrativeGameplayHUD
{
    GENERATED_BODY()
public:
    int32 OpenCount = 0;
    TFunction<void()> OnOpen;
    UPROPERTY() TObjectPtr<USovFatalPresentationTestMenu> LastMenu;
    virtual UNarrativeMenu* OpenMenu(TSubclassOf<UNarrativeMenu> MenuClass, FGameplayTag LayerTag) override
    {
        ++OpenCount;
        LastMenu = NewObject<USovFatalPresentationTestMenu>(this, MenuClass);
        LastMenu->ActivateWidget();
        if (OnOpen) { OnOpen(); }
        return LastMenu;
    }
};

UCLASS(Transient)
class ASovFatalPresentationTestController : public ASovHandoffRuntimeTestController
{
    GENERATED_BODY()
public:
    ASovFatalPresentationTestController(const FObjectInitializer& Initializer) : Super(Initializer) {}
    int32 LegacyDeaths = 0;
    void ConfigureFailureMenu(bool bEnabled)
    { FatalRecoveryFailureMenuClass = bEnabled ? USovFatalPresentationTestMenu::StaticClass() : nullptr; }
    void SetTestHUD(USovFatalPresentationTestHUD* HUD) { GameplayHUD = HUD; }
protected:
    virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, bool bDead) override
    {
        if (bDead) { ++LegacyDeaths; }
        ANarrativePlayerController::HandleDeath_Implementation(KilledActor, KilledActorASC, bDead);
    }
};
