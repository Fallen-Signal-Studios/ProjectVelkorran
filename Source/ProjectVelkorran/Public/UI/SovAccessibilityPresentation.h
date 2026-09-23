// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SovGameUserSettings.h"
#include "UI/SovObjectivePresentationTypes.h"
#include "UI/SovObjectiveWaypoint.h"
#include "SovAccessibilityPresentation.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class USizeBox;
class UVerticalBox;
class UPlayerInteractionComponent;
class UNarrativeInteractableComponent;
class ANarrativeCharacter;
class UCommonActivatableWidget;

UENUM(BlueprintType)
enum class ESovCaptionPriority : uint8 { Routine, Important, Critical };

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
	ESovCaptionPriority CaptionPriority = ESovCaptionPriority::Routine;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovSceneHistoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovObjectiveViewChanged);

/** Native, split-screen-relative presentation. No material/widget asset is required. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibilityPresentation : public UUserWidget
{
	GENERATED_BODY()
public:
	USovAccessibilityPresentation(const FObjectInitializer& Initializer);
    /** Geometry readers for other native HUD layers; no objective or caption state changes. */
    const UBorder* GetObjectivePanel() const { return ObjectiveBackground; }
    const UBorder* GetSubtitlePanel() const { return SubtitleBackground; }
    const UBorder* GetCaptionPanel() const { return CaptionBackground; }
    /** The title-safe text area in absolute space; false before the canvas has been arranged. */
    bool GetSafeAreaAbsoluteRect(FSlateRect& Out) const;
    /** While the holographic HUD is up, text keeps clear of the areas it draws in (see SovHolographicHUDLayout). */
    void SetHolographicHUDClearance(bool bHUDShown, bool bAmmoShown);
    /** The HUD's occupied areas in absolute space, for other overlays that must avoid them. Empty while it is down. */
    void GetHolographicHUDRegions(TArray<FSlateRect>& OutAbsolute) const;
    /** Live owning-player menu geometry. Weak references never keep a closed menu alive. */
    void SetWeaponWheelSurface(UWidget* Surface, UCommonActivatableWidget* Menu);
    bool GetWeaponWheelAbsoluteRect(FSlateRect& Out) const;
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void PresentSpeech(const FText& Speaker, const FText& Text, float Duration, const FVector& SpeakerLocation, bool bCinematic);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void PresentCaption(const FText& Text, float Duration, const FVector& SourceLocation, ESovCaptionPriority CaptionPriority = ESovCaptionPriority::Important);
	/** Line-end and normal dialogue completion preserve the remaining readable pages. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void ClearSpeech();
	/** A replacement scene may take the speech surface immediately; its predecessor stays in recent history. */
	void RetireSpeechPresentation();
	UFUNCTION(BlueprintCallable, Category="Sovereign|Accessibility") void ClearSceneHistory();
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") TArray<FSovSceneSubtitleEntry> GetSceneHistory() const { return History; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") FText GetCurrentCaptionText() const { return CaptionRemaining > 0.f ? ActiveCaption.Text : FText::GetEmpty(); }
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") FText GetCurrentSpeechText() const { return ActiveSpeech.Text; }
	/** Frontend supplies only current, actionable goals; this surface never owns quest state. */
	void PresentObjectives(const TArray<FSovObjectivePresentationEntry>& Entries, int32 AdditionalCount = 0);
	void ClearObjectives();
	UFUNCTION(BlueprintPure, Category="Sovereign|Objectives") TArray<FSovObjectivePresentationEntry> GetPresentedObjectives() const { return Objectives; }
	/** Same authorized cache as the HUD, including goals deferred by row/height limits. */
	UFUNCTION(BlueprintPure, Category="Sovereign|Objectives") TArray<FSovObjectivePresentationEntry> GetObjectiveReviewEntries() const { return ObjectiveReviewEntries; }
	UPROPERTY(BlueprintAssignable, Category="Sovereign|Objectives") FSovObjectiveViewChanged OnObjectiveViewChanged;
	UFUNCTION(BlueprintPure, Category="Sovereign|Objectives") int32 GetAdditionalObjectiveCount() const { return AdditionalObjectiveCount + Objectives.Num() - VisibleObjectiveRows; }
	uint64 GetObjectiveViewGeneration() const { return ObjectiveViewGeneration; }
	static constexpr int32 MaximumObjectiveRows = 3;
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
    TWeakObjectPtr<UWidget> WeaponWheelSurface;
    TWeakObjectPtr<UCommonActivatableWidget> WeaponWheelMenu;
	friend struct FSovFrontendTestAccess;
	friend struct FSovAccessibilityFrontendTestAccess;
	friend struct FSovObjectivePresentationTestAccess;
	void BeginEntry(const FSovSceneSubtitleEntry& Entry);
	void BeginCaption(const FSovSceneSubtitleEntry& Entry);
	void QueueCaption(const FSovSceneSubtitleEntry& Entry);
	void RefreshWeakPointMarkers(APlayerController* Player);
    void RefreshObjectiveWaypoint();
    void RegisterWaypointSource(AActor* Actor);
    void ResetWaypointRegistry();
	void RegisterMarkerCharacter(AActor* Actor);
	void ResetMarkerRegistry();
	void RefreshText();
	void RefreshObjectiveText();
	void LayoutObjectives(float SafeWidth, float SafeHeight);
	float GetSafeTextWidth() const;
	/** The subtitle column's width: the safe width's centre band, narrowed only where the HUD requires it. */
	float GetSubtitleTextWidth() const;
	FVector2D GetSafeCanvasSize() const;
	bool bHUDClearance = false;
	bool bHUDAmmo = false;
	FText DirectionText(const FVector& Location) const;
	UFUNCTION() void SettingsChanged(const FSovUserSettingsSnapshot& Value);
	UFUNCTION() void FoundInteractable(UNarrativeInteractableComponent* Value);
	UFUNCTION() void LostInteractable(UNarrativeInteractableComponent* Value);
	UPROPERTY(Transient) TObjectPtr<UBorder> SubtitleBackground;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SafeTextCanvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient) TObjectPtr<UBorder> CaptionBackground;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CaptionText;
	UPROPERTY(Transient) TObjectPtr<UBorder> ObjectiveBackground;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> ObjectiveSize;
	UPROPERTY(Transient) TArray<TObjectPtr<UVerticalBox>> ObjectiveRowPanels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveHeaders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveRows;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveOverflow;
	UPROPERTY(Transient) TArray<FSovObjectivePresentationEntry> Objectives;
	UPROPERTY(Transient) TArray<FSovObjectivePresentationEntry> ObjectiveReviewEntries;
	int32 AdditionalObjectiveCount = 0;
	int32 VisibleObjectiveRows = 0;
	uint64 ObjectiveViewGeneration = 0;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> SubtitleSlot;
	UPROPERTY(Transient) TObjectPtr<USovGameUserSettings> BoundSettings;
	UPROPERTY(Transient) TObjectPtr<UPlayerInteractionComponent> Interaction;
	UPROPERTY(Transient) TArray<FSovSceneSubtitleEntry> History;
	UPROPERTY(Transient) TArray<FSovSceneSubtitleEntry> PendingSpeech;
	UPROPERTY(Transient) TArray<FSovSceneSubtitleEntry> PendingCaptions;
	TWeakObjectPtr<UNarrativeInteractableComponent> FocusedInteractable;
	FSovUserSettingsSnapshot Settings;
	FSovSceneSubtitleEntry ActiveSpeech;
	FSovSceneSubtitleEntry ActiveCaption;
	TArray<FString> SpeechPages;
	TArray<FString> CaptionPages;
	int32 CaptionPageIndex = 0;
	float CaptionPageDuration = 3.f;
	int32 PageIndex = 0;
	float PageRemaining = 0.f;
	float CaptionRemaining = 0.f;
	float MarkerRefreshRemaining = 0.f;
	float LastLayoutWidth = 0.f;
	float LastLayoutHeight = 0.f;
	struct FMarker { FVector Location; FText Text; bool bThreat = false; bool bNavigation = false; };
	TArray<FMarker> Markers;
    FSovObjectiveWaypoint ObjectiveWaypoint;
    TArray<TWeakObjectPtr<AActor>> WaypointSources;
    TWeakObjectPtr<UWorld> WaypointWorld;
    FDelegateHandle WaypointSpawnedHandle;
	TWeakObjectPtr<UWorld> MarkerWorld;
	FDelegateHandle ActorSpawnedHandle;
	TArray<TWeakObjectPtr<ANarrativeCharacter>> MarkerCharacters;
	TArray<TWeakObjectPtr<ANarrativeCharacter>> RetainedMarkerCharacters;
	size_t MarkerCursor = 0;
};
