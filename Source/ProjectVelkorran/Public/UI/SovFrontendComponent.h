// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"
#include "GAS/SovCombatTypes.h"
#include "SovFrontendComponent.generated.h"
class UTalesComponent;
class USovNarrativeCue;
class USovNarrativeCueComponent;
class UNarrativeAbilitySystemComponent;
class USovAccessibilityPresentation;
class USovAccessibilitySettingsMenu;

/** Binds native frontend consumers to existing Tales, cue and combat producers. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovFrontendComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USovFrontendComponent();
    /** Real local viewport only: headless automation/server worlds never acquire a first-boot UI prerequisite. */
    static bool IsInitialAccessibilitySetupPending(const class APlayerController* Player);
    void RefreshFrontend();
    UFUNCTION(BlueprintCallable, Category="Accessibility") bool OpenAccessibilitySettings();
    USovAccessibilityPresentation* GetPresentation() const { return Presentation; }
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FSovFrontendTestAccess;
    TWeakObjectPtr<AActor> PresentationAvatar;
    FName PresentationMission;
    void BindProducers(UTalesComponent* Tales, USovNarrativeCueComponent* Cues, UNarrativeAbilitySystemComponent* ASC);
    void RetirePreviousSpeech(UDialogue* NewDialogue);
    UFUNCTION() void OnNPCLine(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker);
    UFUNCTION() void OnPlayerLine(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line);
    UFUNCTION() void OnNPCLineFinished(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker);
    UFUNCTION() void OnPlayerLineFinished(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line);
    UFUNCTION() void OnDialogueEnded(UDialogue* Dialogue, bool bStartingNew, EExitDialogueReason Reason);
    UFUNCTION() void OnDialogueSuspended(UDialogue* Dialogue, bool bSuspended);
    UFUNCTION() void OnCueStarted(USovNarrativeCue* Cue, AActor* Speaker, const FText& Caption, float Seconds);
    UFUNCTION() void OnCueEnded(USovNarrativeCue* Cue, bool bInterrupted);
    UFUNCTION() void OnCueAudioReady(USovNarrativeCue* Cue, float Seconds);
    UFUNCTION() void OnDamage(const FSovDamageResult& Result);
    void Unbind();
    void ReleaseSetupPause();
    bool bEnding = false;
    bool bOwnSetupPause = false;
    TWeakObjectPtr<UWorld> AudioAppliedWorld;
    uint32 AudioAppliedDevice = MAX_uint32;
    TWeakObjectPtr<class ASovPlayerController> PausedController;
    TWeakObjectPtr<UTalesComponent> BoundTales;
    TWeakObjectPtr<USovNarrativeCueComponent> BoundCues;
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
    TWeakObjectPtr<UDialogue> SpeechDialogue;
    TWeakObjectPtr<UDialogueNode> SpeechNode;
    uint64 SpeechEpoch = 0;
    FGuid SpeechReceipt;
    FGuid CueReceipt;
    bool bRetireSceneOnNextLine = false;
    TWeakObjectPtr<USovNarrativeCue> SpeechCue;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilityPresentation> Presentation;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilitySettingsMenu> SetupMenu;
};
