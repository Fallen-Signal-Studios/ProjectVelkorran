// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Settings/SovGameUserSettings.h"
#include "Feedback/SovHapticFeedbackComponent.h"
#include "SovPlatformOutputTestFixtures.generated.h"

UCLASS()
class USovPlatformOutputTestSettings : public USovGameUserSettings
{
	GENERATED_BODY()
public:
	bool bAvailable = true;
	bool bSupported = true;
	bool bRejectEnable = false;
	bool bLoseOutputAfterWrite = false;
	bool bOutputEnabled = false;
	int32 OutputNits = 0;
	double Now = 100.;
	int32 Writes = 0;
	int32 Saves = 0;
	bool bSavedEnabled = false;
	int32 SavedNits = 0;
	bool bTryReentryOnPersist = false;
	bool bReentryAccepted = false;
	FGuid ReentryReceipt;
	void InitializeOutput(bool bEnabled, int32 Nits)
	{ bOutputEnabled = bEnabled; OutputNits = bEnabled ? Nits : 0; bUseHDRDisplayOutput = bEnabled; HDRDisplayOutputNits = Nits; }
protected:
	virtual bool CanApplyHDROutput() const override { return bAvailable; }
	virtual double HDRTime() const override { return Now; }
	virtual FSovHDROutputStatus ReadHDROutput() override
	{
		FSovHDROutputStatus Result;
		Result.bSupported = bAvailable && bSupported;
		Result.bEnabled = Result.bSupported && bOutputEnabled;
		Result.PeakNits = Result.bEnabled ? OutputNits : 0;
		return Result;
	}
	virtual void WriteHDROutput(bool bEnable, int32 Nits) override
	{
		++Writes;
		if (!bAvailable) { return; }
		bOutputEnabled = bEnable && bSupported && !bRejectEnable;
		OutputNits = bOutputEnabled ? (Nits > 1500 ? 2000 : 1000) : 0;
		bUseHDRDisplayOutput = bOutputEnabled;
		HDRDisplayOutputNits = bOutputEnabled ? OutputNits : Nits;
		if (bLoseOutputAfterWrite) { bLoseOutputAfterWrite = false; bAvailable = false; }
	}
	virtual void PersistSettings() override
	{
		++Saves; bSavedEnabled = bUseHDRDisplayOutput; SavedNits = HDRDisplayOutputNits;
		if (bTryReentryOnPersist)
		{ FString Error; bReentryAccepted = ConfirmHDRCalibration(ReentryReceipt, Error); }
	}
};

UCLASS()
class USovHapticOutputTestComponent : public USovHapticFeedbackComponent
{
	GENERATED_BODY()
public:
	bool bCanOutput = true;
	double Now = 10.;
	FSovHapticSettings Value;
	float Outputs[SovHapticPolicy::ChannelCount] = {};
protected:
	virtual bool CanOutput(ESovHapticChannel Channel) const override { return bCanOutput; }
	virtual double FeedbackTime() const override { return Now; }
	virtual FSovHapticSettings ReadFeedbackSettings() const override { return Value; }
	virtual void SubmitChannel(ESovHapticChannel Channel, float Intensity) override
	{ if (static_cast<unsigned>(Channel) < SovHapticPolicy::ChannelCount) { Outputs[static_cast<unsigned>(Channel)] = Intensity; } }
};
