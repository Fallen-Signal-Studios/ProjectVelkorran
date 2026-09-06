// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Settings/SovGameUserSettings.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Targeting/SovTargetingComponent.h"
#include "SovSettingsTestFixtures.generated.h"
UCLASS()
class USovSettingsTestSettings : public USovGameUserSettings
{
	GENERATED_BODY()
public:
	virtual void SaveSettings() override { ++Saves; }
	int32 Saves = 0;
};
UCLASS()
class USovSettingsReentryProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<USovSettingsTestSettings> Settings;
	bool bReentryAccepted = false;
	int32 Calls = 0;
	UFUNCTION() void OnChanged(const FSovUserSettingsSnapshot& Value)
	{
		++Calls;
		FString Error;
		bReentryAccepted = Settings->ApplySettingsSnapshot(Value, Error);
	}
};

UCLASS()
class ASovInputRoutingTestController : public ANarrativePlayerController
{
	GENERATED_BODY()
public:
	FGuid TestActorGuid = FGuid::NewGuid();
	virtual FGuid GetActorGUID_Implementation() const override { return TestActorGuid; }
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> TestASC;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return TestASC ? TestASC.Get() : Super::GetAbilitySystemComponent(); }
	TSet<FGameplayTag> TestToggleTags;
	virtual bool WantsToggleForInput(FGameplayTag Tag) const override { return TestToggleTags.Contains(Tag); }
	void SetTestMappings(UNarrativeAbilityInputMapping* Mappings) { AbilityInputMappings = Mappings; }
	void SuppressTestInputs() { ReleaseHeldAbilityInputs(); }
};
UCLASS()
class USovInputRoutingProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<ASovInputRoutingTestController> RepressController;
	FGameplayTag RepressTag;
	bool bRepressOnce = false;
	bool bSuppressOnce = false;
	TArray<FGameplayTag> Presses;
	TArray<FGameplayTag> Releases;
	UFUNCTION() void OnInput(FGameplayTag Tag, bool bPressed)
	{
		if (bPressed) { Presses.Add(Tag); } else { Releases.Add(Tag); }
		if (!bPressed && bSuppressOnce && Tag == RepressTag && RepressController)
		{ bSuppressOnce = false; RepressController->SuppressTestInputs(); }
		if (!bPressed && bRepressOnce && Tag == RepressTag && RepressController)
		{ bRepressOnce = false; RepressController->AbilityInputPressed(Tag); }
	}
};

UCLASS()
class USovTargetingLossProbe : public UObject
{
	GENERATED_BODY()
public:
	int32 LostCount = 0;
	ESovLockLossReason LastLoss = ESovLockLossReason::None;
	UFUNCTION() void OnTargetChanged(AActor* Target, ESovLockLossReason Reason)
	{ if (!Target) { ++LostCount; LastLoss = Reason; } }
};
