// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Settings/SovGameUserSettings.h"
#include "Feedback/SovHapticFeedbackComponent.h"
#include "HAL/IConsoleManager.h"
#include "SovPlatformOutputTestFixtures.generated.h"

UCLASS()
class USovPlatformOutputTestSettings : public USovGameUserSettings
{
	GENERATED_BODY()
public:
	bool bAvailable = true;
	bool bSystemManaged = false;
	bool bSupported = true;
	bool bRejectEnable = false;
	bool bLoseOutputAfterWrite = false;
	bool bOutputEnabled = false;
	bool bCalibrationAvailable = true;
	bool bRejectCalibration = false;
	FString DisplayIdentity = TEXT("display-a");
	FSovHDRCalibration PhysicalCalibration;
	FSovHDRCalibration SavedCalibration;
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
	virtual bool IsDisplayOutputSystemManaged() const override { return bSystemManaged; }
protected:
	virtual bool CanApplyHDROutput() const override { return bAvailable; }
	virtual double HDRTime() const override { return Now; }
	virtual bool CanApplyDisplayCalibration() const override { return bAvailable && bCalibrationAvailable; }
	virtual bool ReadDisplayCalibration(FSovHDRCalibration& Out) const override
	{ Out = PhysicalCalibration; return bAvailable && bCalibrationAvailable; }
	virtual bool WriteDisplayCalibration(const FSovHDRCalibration& Value) override
	{ if (!CanApplyDisplayCalibration() || bRejectCalibration) { return false; } PhysicalCalibration = Value; return true; }
	virtual float DisplayUIBaseNits() const override { return 300.f; }
	virtual void RestoreDisplayCalibration(const FSovHDRCalibration& Expected, const FSovHDRCalibration& Before, float ExpectedUILevel, float BeforeUILevel) override
	{
		if (!bAvailable || !bCalibrationAvailable) { return; }
		if (FMath::IsNearlyEqual(PhysicalCalibration.BlackFloorNits, Expected.BlackFloorNits, 1.e-7f)) { PhysicalCalibration.BlackFloorNits = Before.BlackFloorNits; }
		if (FMath::IsNearlyEqual(PhysicalCalibration.PaperWhiteNits, Expected.PaperWhiteNits, .01f)) { PhysicalCalibration.PaperWhiteNits = Before.PaperWhiteNits; }
		if (FMath::IsNearlyEqual(PhysicalCalibration.UIWhiteNits, Expected.UIWhiteNits, .01f)) { PhysicalCalibration.UIWhiteNits = Before.UIWhiteNits; }
	}
	virtual FSovHDROutputStatus ReadHDROutput() override
	{
		FSovHDROutputStatus Result;
		Result.bSupported = bAvailable && bSupported;
		Result.bEnabled = Result.bSupported && bOutputEnabled;
		Result.PeakNits = Result.bEnabled ? OutputNits : 0;
		Result.DisplayIdentity = bAvailable ? DisplayIdentity : FString();
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
		SavedCalibration = GetHDRCalibration();
		if (bTryReentryOnPersist)
		{ FString Error; bReentryAccepted = ConfirmHDRCalibration(ReentryReceipt, Error); }
	}
};

/** Uses the real production CVar adapter with isolated variables, never the workstation's renderer CVars. */
UCLASS()
class USovDisplayCVarTestSettings : public USovPlatformOutputTestSettings
{
	GENERATED_BODY()
public:
	void InitializeVariables()
	{
		const FString Prefix = TEXT("Sov.Test.Calibration.") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".");
		const TCHAR* Names[] = { TEXT("r.HDR.Display.MinLuminanceLog10"), TEXT("r.HDR.Display.MidLuminance"), TEXT("r.HDR.UI.Level"), TEXT("r.HDR.UI.Luminance"), TEXT("r.HDR.UI.CompositeMode") };
		const float Defaults[] = { -4.f, 15.f, 1.f, 300.f, 1.f };
		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			const FString Registered = Prefix + FString::FromInt(I); RegisteredNames.Add(Registered);
			Variables.Add(Names[I], IConsoleManager::Get().RegisterConsoleVariable(*Registered, Defaults[I], TEXT("Isolated calibration regression variable."), ECVF_Default));
		}
	}
	IConsoleVariable* GetVariable(const TCHAR* Name) const { return Variables.FindRef(Name); }
	bool WriteProductionCalibration(const FSovHDRCalibration& Value) { return WriteDisplayCalibration(Value); }
	void ResetVariables()
	{
		for (auto& Entry : Variables) { if (Entry.Value) { Entry.Value->SetOnChangedCallback(FConsoleVariableDelegate()); } }
		Variables.Reset();
		for (const FString& Name : RegisteredNames) { IConsoleManager::Get().UnregisterConsoleObject(*Name, false); }
		RegisteredNames.Reset();
	}
	virtual void BeginDestroy() override { Super::BeginDestroy(); ResetVariables(); }
protected:
	virtual IConsoleVariable* FindDisplayCalibrationVariable(const TCHAR* Name) const override { return GetVariable(Name); }
	virtual bool CanApplyDisplayCalibration() const override { return USovGameUserSettings::CanApplyDisplayCalibration(); }
	virtual bool ReadDisplayCalibration(FSovHDRCalibration& Value) const override { return USovGameUserSettings::ReadDisplayCalibration(Value); }
	virtual bool WriteDisplayCalibration(const FSovHDRCalibration& Value) override { return USovGameUserSettings::WriteDisplayCalibration(Value); }
	virtual float DisplayUIBaseNits() const override { return USovGameUserSettings::DisplayUIBaseNits(); }
	virtual void RestoreDisplayCalibration(const FSovHDRCalibration& Expected, const FSovHDRCalibration& Before, float ExpectedUILevel, float BeforeUILevel) override
	{ USovGameUserSettings::RestoreDisplayCalibration(Expected, Before, ExpectedUILevel, BeforeUILevel); }
private:
	TMap<FString, IConsoleVariable*> Variables;
	TArray<FString> RegisteredNames;
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
