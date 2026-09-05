// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tales/Dialogue.h"
#include "Settings/SovGameUserSettings.h"
#include "SovDialoguePresentationComponent.generated.h"

class UTalesComponent;
class USovDialogueChoiceWidget;
class USovAccessibleNarrationSubsystem;
struct FSovDialoguePresentationState;

/** Controller-local presentation of the existing Tales reply revision. Never owns graph progression. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovDialoguePresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovDialoguePresentationComponent();
	virtual ~USovDialoguePresentationComponent() override;
	/** Restore choices after an intentionally removed presentation, using current Tales state only. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Dialogue") void RefreshChoices();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	friend struct FSovDialogueTestAccess;
	UFUNCTION() void OnReplies(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Replies);
	UFUNCTION() void OnDialogueBegan(UDialogue* Dialogue);
	UFUNCTION() void OnDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue, EExitDialogueReason Reason);
	UFUNCTION() void OnOptionSelected(UDialogue* Dialogue, UDialogueNode_Player* Option);
	UFUNCTION() void OnSuspensionChanged(UDialogue* Dialogue, bool bSuspended);
	UFUNCTION() void OnSettingsChanged(const FSovUserSettingsSnapshot& Settings);
	void CancelPresentation();
	void OnWidgetRemoved();
	void Choose(int32 Index);
	void OnFocusedChoice(int32 Index);
	void AnnounceChoices();
	void FinishAnnouncement(FGuid Request, bool bCompleted, uint64 ExpectedGeneration, uint64 ExpectedSpeechGeneration);
	void SuspendPresentation();
	bool IsCurrent() const;
	bool IsPresentationPaused() const;
	FText TimerDescription() const;
	USovAccessibleNarrationSubsystem* Narrator() const;
	UPROPERTY(Transient) TObjectPtr<UTalesComponent> Tales;
	UPROPERTY(Transient) TObjectPtr<USovDialogueChoiceWidget> ChoiceWidget;
	UPROPERTY(Transient) TWeakObjectPtr<UDialogue> PresentedDialogue;
	UPROPERTY(Transient) TArray<TObjectPtr<UDialogueNode_Player>> PresentedReplies;
	UPROPERTY(Transient) TObjectPtr<UDialogueNode_Player> SilenceReply;
	TUniquePtr<FSovDialoguePresentationState> State;
	uint64 Generation = 0;
	int64 SeenRevision = INDEX_NONE;
	TWeakObjectPtr<UDialogue> SeenDialogue;
	bool bEndingPlay = false;
};
