// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Settings/SovGameUserSettings.h"
#include "Settings/SovDisplayPolicy.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/IConsoleManager.h"
#include "Misc/SecureHash.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr const TCHAR* BlackVariable = TEXT("r.HDR.Display.MinLuminanceLog10");
constexpr const TCHAR* GrayVariable = TEXT("r.HDR.Display.MidLuminance");
constexpr const TCHAR* UIVariable = TEXT("r.HDR.UI.Level");
bool MutableVariable(const IConsoleVariable* Var)
{
    return Var && !Var->TestFlags(ECVF_ReadOnly)
        && (Var->GetFlags() & ECVF_SetByMask) <= ECVF_SetByGameSetting;
}
}

IConsoleVariable* USovGameUserSettings::FindDisplayCalibrationVariable(const TCHAR* Name) const
{ return IConsoleManager::Get().FindConsoleVariable(Name); }
float USovGameUserSettings::DisplayUIBaseNits() const
{
    const IConsoleVariable* Base = FindDisplayCalibrationVariable(TEXT("r.HDR.UI.Luminance"));
    return Base ? Base->GetFloat() : 0.f;
}
bool USovGameUserSettings::CanApplyDisplayCalibration() const
{
    const IConsoleVariable* Composite = FindDisplayCalibrationVariable(TEXT("r.HDR.UI.CompositeMode"));
    return !IsDisplayOutputSystemManaged() && CanApplyHDROutput() && Composite && Composite->GetInt() == 1
        && FMath::IsFinite(DisplayUIBaseNits()) && DisplayUIBaseNits() > 0.f
        && MutableVariable(FindDisplayCalibrationVariable(BlackVariable)) && MutableVariable(FindDisplayCalibrationVariable(GrayVariable))
        && MutableVariable(FindDisplayCalibrationVariable(UIVariable));
}
bool USovGameUserSettings::ReadDisplayCalibration(FSovHDRCalibration& Out) const
{
    const IConsoleVariable* Black = FindDisplayCalibrationVariable(BlackVariable);
    const IConsoleVariable* Gray = FindDisplayCalibrationVariable(GrayVariable);
    const IConsoleVariable* UI = FindDisplayCalibrationVariable(UIVariable);
    if (!Black || !Gray || !UI || !FMath::IsFinite(DisplayUIBaseNits()) || DisplayUIBaseNits() <= 0.f) { return false; }
    Out.BlackFloorNits = FMath::Pow(10.f, Black->GetFloat());
    Out.PaperWhiteNits = Gray->GetFloat() / .18f;
    Out.UIWhiteNits = DisplayUIBaseNits() * UI->GetFloat();
    return FMath::IsFinite(Out.BlackFloorNits) && Out.BlackFloorNits > 0.f
        && FMath::IsFinite(Out.PaperWhiteNits) && Out.PaperWhiteNits > 0.f
        && FMath::IsFinite(Out.UIWhiteNits) && Out.UIWhiteNits > 0.f;
}
bool USovGameUserSettings::WriteDisplayCalibration(const FSovHDRCalibration& Value)
{
    if (!CanApplyDisplayCalibration() || !FMath::IsFinite(Value.BlackFloorNits) || Value.BlackFloorNits <= 0.f
        || !FMath::IsFinite(Value.PaperWhiteNits) || Value.PaperWhiteNits <= 0.f
        || !FMath::IsFinite(Value.UIWhiteNits) || Value.UIWhiteNits <= 0.f) { return false; }
    // The public request validates designer bounds; physical rollback may restore a prior renderer value outside them.
    IConsoleVariable* Vars[] = { FindDisplayCalibrationVariable(BlackVariable), FindDisplayCalibrationVariable(GrayVariable), FindDisplayCalibrationVariable(UIVariable) };
    const float Desired[] = { static_cast<float>(SovDisplayPolicy::BlackLog10(Value.BlackFloorNits)),
        static_cast<float>(SovDisplayPolicy::GrayNits(Value.PaperWhiteNits)), Value.UIWhiteNits / DisplayUIBaseNits() };
    const float Before[] = { Vars[0]->GetFloat(), Vars[1]->GetFloat(), Vars[2]->GetFloat() };
    int32 Written = 0;
    const auto Rollback = [&]()
    {
        // A setter can synchronously invoke a sink that changes another field's priority/value.
        for (int32 J = 0; J < Written; ++J)
        {
            if (MutableVariable(Vars[J]) && FMath::IsNearlyEqual(Vars[J]->GetFloat(), Desired[J], .0001f))
            { Vars[J]->Set(Before[J], ECVF_SetByGameSetting); }
        }
    };
    for (int32 I = 0; I < 3; ++I)
    {
        if (!MutableVariable(Vars[I])) { Rollback(); return false; }
        Written = I + 1;
        Vars[I]->Set(Desired[I], ECVF_SetByGameSetting);
        if (!FMath::IsNearlyEqual(Vars[I]->GetFloat(), Desired[I], .0001f))
        {
            // Retire only values this write still owns; console/device overrides remain authoritative.
            Rollback();
            return false;
        }
    }
    FSovHDRCalibration Observed;
    if (ReadDisplayCalibration(Observed) && Observed.Equals(Value)) { return true; }
    Rollback(); return false;
}
void USovGameUserSettings::ApplyConfirmedDisplayCalibration()
{
    if (!IsDisplayOutputSystemManaged() && bHasDisplayCalibration && DisplayCalibration.IsValid() && ReadHDROutput().bEnabled)
    { WriteDisplayCalibration(DisplayCalibration); }
}
void USovGameUserSettings::RestorePreviewDisplayCalibration()
{
    RestoreDisplayCalibration(PreviewDisplayCalibration, BeforeRenderCalibration, PreviewUILevel, BeforeRenderUILevel);
}
void USovGameUserSettings::RestoreDisplayCalibration(const FSovHDRCalibration& Expected, const FSovHDRCalibration& Before, float ExpectedUILevel, float BeforeUILevel)
{
    // Rollback deliberately does not require a usable HDR device/compositor or ALL fields to be writable.
    // Each surviving CVar is independently owned: one console override must not strand the other preview values.
    const auto Restore = [&](const TCHAR* Name, float ExpectedRaw, float BeforeRaw)
    {
        IConsoleVariable* Var = FindDisplayCalibrationVariable(Name);
        if (FMath::IsFinite(ExpectedRaw) && FMath::IsFinite(BeforeRaw) && MutableVariable(Var)
            && FMath::IsNearlyEqual(Var->GetFloat(), ExpectedRaw, .0001f))
        { Var->Set(BeforeRaw, ECVF_SetByGameSetting); }
    };
    if (Expected.BlackFloorNits > 0.f && Before.BlackFloorNits > 0.f)
    { Restore(BlackVariable, static_cast<float>(SovDisplayPolicy::BlackLog10(Expected.BlackFloorNits)), static_cast<float>(SovDisplayPolicy::BlackLog10(Before.BlackFloorNits))); }
    Restore(GrayVariable, static_cast<float>(SovDisplayPolicy::GrayNits(Expected.PaperWhiteNits)), static_cast<float>(SovDisplayPolicy::GrayNits(Before.PaperWhiteNits)));
    // The base luminance is an external dependency; changing it must not alter this receipt's raw gain ownership.
    Restore(UIVariable, ExpectedUILevel, BeforeUILevel);
}

void USovGameUserSettings::BeginDisplayObservation()
{
    EndDisplayObservation();
    if (!FSlateApplication::IsInitialized()) { return; }
    const TSharedPtr<GenericApplication> Application = FSlateApplication::Get().GetPlatformApplication();
    if (!Application.IsValid()) { return; }
    ObservedDisplayApplication = Application;
    DisplayMetricsHandle = Application->OnDisplayMetricsChanged().AddUObject(this, &ThisClass::HandleDisplayMetricsChanged);
}
void USovGameUserSettings::EndDisplayObservation()
{
    if (const TSharedPtr<GenericApplication> Application = ObservedDisplayApplication.Pin())
    { Application->OnDisplayMetricsChanged().Remove(DisplayMetricsHandle); }
    DisplayMetricsHandle.Reset(); ObservedDisplayApplication.Reset(); bDisplayMetricsInvalidated = false;
}
void USovGameUserSettings::HandleDisplayMetricsChanged(const FDisplayMetrics& Metrics)
{
    // Even unplug/replug with an identical monitor descriptor invalidates a pending confirmation.
    bDisplayMetricsInvalidated = true;
}

// Separate from saved settings: identities are short-lived preview evidence, never sent to cloud or telemetry.
FString SovCurrentDisplayIdentity()
{
    if (!FSlateApplication::IsInitialized() || !GEngine || !GEngine->GameViewport) { return FString(); }
    const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
    if (!Window.IsValid()) { return FString(); }
    const TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
    if (!NativeWindow.IsValid() || NativeWindow->IsMinimized()) { return FString(); }
    const FVector2D Position = Window->GetPositionInScreen(), Size = Window->GetSizeInScreen();
    const SovDisplayPolicy::Rect View{Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y};
    if (!SovDisplayPolicy::ValidRect(View)) { return FString(); }
    FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics);
    double GreatestArea = 0.; FString Selected; bool bAmbiguous = false;
    TArray<FString> Topology;
    for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
    {
        const auto& R = Monitor.DisplayRect;
        const double Area = SovDisplayPolicy::IntersectionArea(View,
            {static_cast<double>(R.Left), static_cast<double>(R.Top), static_cast<double>(R.Right), static_cast<double>(R.Bottom)});
        Topology.Add(FString::Printf(TEXT("%s:%d,%d,%d,%d"), *Monitor.ID, R.Left, R.Top, R.Right, R.Bottom));
        if (Area > GreatestArea) { GreatestArea = Area; Selected = Monitor.ID; bAmbiguous = false; }
        else if (Area > 0. && Area == GreatestArea) { bAmbiguous = true; }
    }
    if (GreatestArea <= 0. || bAmbiguous || Selected.IsEmpty()) { return FString(); }
    int32 Matches = 0;
    for (const FMonitorInfo& Monitor : Metrics.MonitorInfo) { if (Monitor.ID == Selected) { ++Matches; } }
    if (Matches != 1) { return FString(); }
    Topology.Sort();
    return FMD5::HashAnsiString(*(Selected + TEXT("|") + FString::Join(Topology, TEXT("|"))));
}
