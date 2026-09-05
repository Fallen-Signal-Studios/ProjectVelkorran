// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "Settings/SovGameUserSettings.h"
#include "Components/Button.h"
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Save/SovSaveSubsystem.h"
#include "SovAccessibilitySettingsMenu.generated.h"

class UButton;
class UTextBlock;
class UScrollBox;
class UVerticalBox;
class USovAccessibilitySettingsMenu;

UCLASS()
class PROJECTVELKORRAN_API USovAccessibilityNativeButton : public UButton
{
	GENERATED_BODY()
public:
	void SetAccessibleLabel(const FText& Label);
};

/** A real focusable native control; labels and narration update after each committed transaction. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibilitySettingRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(USovAccessibilitySettingsMenu* Owner, FName Key, const FText& Label, float Min = 0.f, float Max = 1.f, float Step = 1.f);
	void Refresh();
	UWidget* GetFocusTarget() const;
	FText GetLabel() const;
	FName SettingKey;
	float Minimum = 0.f, Maximum = 1.f, Increment = 1.f;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
private:
	friend struct FSovAccessibilityFrontendTestAccess;
	UFUNCTION() UWidget* NavigateValue(EUINavigation Direction);
	UFUNCTION() void Clicked();
	UPROPERTY(Transient) TObjectPtr<USovAccessibilitySettingsMenu> Menu;
	UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> Button;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Text;
	FText DisplayLabel;
};

/** Asset-free CommonUI settings screen; mounted in the existing Narrative menu layer. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibilitySettingsMenu : public UNarrativeMenu
{
	GENERATED_BODY()
public:
	USovAccessibilitySettingsMenu();
	void SetFirstBoot(bool bValue);
	void Adjust(USovAccessibilitySettingRow* Row, int32 Direction);
	FText ValueText(const USovAccessibilitySettingRow* Row) const;
	bool IsRowEnabled(const USovAccessibilitySettingRow* Row) const;
	bool CanAdjustValue(const USovAccessibilitySettingRow* Row) const;
	void FocusRow(USovAccessibilitySettingRow* Row, const FText& AccessibleLabel);
	FSovUserSettingsSnapshot CurrentSettings() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
private:
	void AddRow(FName Key, const FText& Label, float Min = 0.f, float Max = 1.f, float Step = 1.f);
	friend struct FSovAccessibilityFrontendTestAccess;
	void RefreshRows();
	UFUNCTION() void SettingsChanged(const FSovUserSettingsSnapshot& Value);
	UFUNCTION() void CloudChanged(const FSovCloudReview& Review);
	UPROPERTY(Transient) TObjectPtr<UScrollBox> Scroll;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RowsBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TArray<TObjectPtr<USovAccessibilitySettingRow>> Rows;
	UPROPERTY(Transient) TObjectPtr<USovGameUserSettings> BoundSettings;
	UPROPERTY(Transient) TObjectPtr<USovPlatformServicesSubsystem> PlatformServices;
	FGuid CloudRequest;
	int32 CloudManualSlot = 0;
	int32 CloudRevisionCursor = 0;
	FGuid DeleteCloudRequest;
	FString DeleteCloudRevision;
	int32 ArchiveKind = 0, ArchiveSlot = 0, ArchiveCursor = 0;
	TArray<FString> ArchiveIds;
	FSovStorageOwnerToken ArchiveOwner;
	FString DeleteArchiveId;
	bool bFirstBoot = false;
	bool bHDREnabled = true;
	int32 HDRPeakNits = 1000;
	FSovHDRCalibration HDRDraft;
	FGuid HDRReceipt;
	uint64 MenuGeneration = 0;
};
