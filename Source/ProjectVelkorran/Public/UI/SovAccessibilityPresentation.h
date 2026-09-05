// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SovGameUserSettings.h"
#include "SovAccessibilityPresentation.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UPlayerInteractionComponent;
class UNarrativeInteractableComponent;

USTRUCT(BlueprintType)
struct FSovSceneSubtitleEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Speaker;
	UPROPERTY(BlueprintReadOnly) FText Text;
	UPROPERTY(BlueprintReadOnly) bool bCaption = false;
	FVector Location = FVector::ZeroVector;
	float Duration = 5.f;
	bool bCinematic = false;
	bool bFinished = false;
	FGuid Receipt;
	bool bHasDirection = true;
	FName CueKey;
	int32 Priority = 0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovSceneHistoryChanged);

/** Native, split-screen-relative presentation. No material/widget asset is required. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibilityPresentation : public UUserWidget
{
	GENERATED_BODY()
public:
	USovAccessibilityPresentation(const FObjectInitializer& Initializer);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void PresentSpeech(const FText& Speaker, const FText& Text, float Duration, const FVector& SpeakerLocation, bool bCinematic);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void PresentCaption(const FText& Text, float Duration, const FVector& SourceLocation);
	FGuid PresentOwnedSpeech(const FText& Speaker, const FText& Text, float Duration, const FVector& Location, bool bCinematic, bool bHasDirection);
	void FinishOwnedSpeech(FGuid Receipt);
	void RestartOwnedSpeech(FGuid Receipt, float Duration);
	void SuspendDialogueSpeech();
	void ResumeDialogueSpeech();
	bool HasUnreadSpeech(FGuid Receipt) const;
	void PresentPrioritizedCaption(const FText& Text, float Duration, const FVector& SourceLocation, FName CueKey, int32 Priority, bool bHasDirection);
	/** Line-end does not erase a page before its readable interval. Scene-end force-clears below. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void ClearSpeech();
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void ClearSceneHistory();
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") TArray<FSovSceneSubtitleEntry> GetSceneHistory() const { return History; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") FText GetCurrentCaptionText() const { return CaptionRemaining > 0.f ? ActiveCaption.Text : FText::GetEmpty(); }
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") FText GetCurrentSpeechText() const { return ActiveSpeech.Text; }
	UPROPERTY(BlueprintAssignable,Category="Sovereign|Accessibility") FSovSceneHistoryChanged OnSceneHistoryChanged;
	static TArray<FString> PaginateText(const FString& Text, int32 CharactersPerLine, int32 MaximumLines);
	static FLinearColor TeamTint(const FSovUserSettingsSnapshot& Settings);
	static FLinearColor ThreatTint(const FSovUserSettingsSnapshot& Settings);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	friend struct FSovFrontendTestAccess;
	friend struct FSovAccessibilityFrontendTestAccess;
	void BeginEntry(const FSovSceneSubtitleEntry& Entry);
	void BeginCaption(const FSovSceneSubtitleEntry& Entry);
	void AdvancePresentation(float DeltaSeconds);
	void RefreshText();
	float GetSafeTextWidth() const;
	FText DirectionText(const FVector& Location, bool bHasDirection = true) const;
	UFUNCTION() void SettingsChanged(const FSovUserSettingsSnapshot& Value);
	UFUNCTION() void FoundInteractable(UNarrativeInteractableComponent* Value);
	UFUNCTION() void LostInteractable(UNarrativeInteractableComponent* Value);
	UPROPERTY(Transient) TObjectPtr<UBorder> SubtitleBackground;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SafeTextCanvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient) TObjectPtr<UBorder> CaptionBackground;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CaptionText;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> SubtitleSlot;
	UPROPERTY(Transient) TObjectPtr<USovGameUserSettings> BoundSettings;
	UPROPERTY(Transient) TObjectPtr<UPlayerInteractionComponent> Interaction;
	UPROPERTY(Transient) TArray<FSovSceneSubtitleEntry> History;
	UPROPERTY(Transient) TArray<FSovSceneSubtitleEntry> PendingSpeech;
	TWeakObjectPtr<UNarrativeInteractableComponent> FocusedInteractable;
	FSovUserSettingsSnapshot Settings;
	FSovSceneSubtitleEntry ActiveSpeech;
	FSovSceneSubtitleEntry ActiveCaption;
	TArray<FSovSceneSubtitleEntry> PendingCaptions;
	FSovSceneSubtitleEntry SuspendedSpeech;
	TArray<FSovSceneSubtitleEntry> SuspendedPendingSpeech;
	TArray<FString> SuspendedSpeechPages;
	int32 SuspendedPageIndex = 0;
	float SuspendedPageRemaining = 0.f;
	bool bDialogueSpeechSuspended = false;
	bool bSuspendedSpeechLayoutDirty = false;
	TArray<FString> SpeechPages;
	TArray<FString> CaptionPages;
	int32 CaptionPageIndex = 0;
	float CaptionPageDuration = 3.f;
	int32 PageIndex = 0;
	float PageRemaining = 0.f;
	float CaptionRemaining = 0.f;
	float MarkerRefreshRemaining = 0.f;
	float LastLayoutWidth = 0.f;
	float TextRefreshRemaining = 0.f;
	struct FMarker { FVector Location; FText Text; bool bThreat = false; bool bNavigation = false; };
	TArray<FMarker> Markers;
};
