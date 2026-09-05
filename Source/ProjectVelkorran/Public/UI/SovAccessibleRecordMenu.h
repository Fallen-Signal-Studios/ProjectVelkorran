// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "Settings/SovGameUserSettings.h"
#include "Campaign/SovNarrativeTypes.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "SovAccessibleRecordMenu.generated.h"
class UTextBlock;
class USovAccessibilityNativeButton;
class USovEvidenceDefinition;
class USovAccessibilityPresentation;
class UScrollBox;

/** Acquired evidence/current-scene review, using the existing campaign journal, never a second evidence store. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibleRecordMenu : public UNarrativeMenu
{
	GENERATED_BODY()
public:
	USovAccessibleRecordMenu();
	void SetSceneHistoryMode(bool bValue);
	static FText DescribeEvidence(const USovEvidenceDefinition* Definition,ESovEvidenceStage Stage);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	friend struct FSovAccessibilityFrontendTestAccess;
	void RebuildRecords();
	void ShowRecord(bool bAnnounce);
	UFUNCTION() void Previous();
	UFUNCTION() void Next();
	UFUNCTION() void Read();
	UFUNCTION() void Close();
	UFUNCTION() void SettingsChanged(const FSovUserSettingsSnapshot& Value);
	UFUNCTION() void HistoryChanged();
	UFUNCTION() void EvidenceChanged(const FSovEvidenceAcquisition& Value);
	UFUNCTION() void CampaignRestored(bool bValid);
	UFUNCTION() void MissionChanged(FName Mission,bool bSucceeded);
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Body;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ScrollHint;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> RecordScroll;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> PreviousButton;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> NextButton;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> ReadButton;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> CloseButton;
	UPROPERTY(Transient) TObjectPtr<USovGameUserSettings> BoundSettings;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityPresentation> BoundPresentation;
	UPROPERTY(Transient) TObjectPtr<USovCampaignStateComponent> BoundCampaign;
	TArray<FText> Records;
	bool bSceneHistory = false;
	int32 Selection = 0;
	uint64 ViewGeneration = 0;
	bool bRetiring = false;
};
