// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Diagnostics/SovPerformanceCaptureSubsystem.h"

#include "Diagnostics/SovLogChannels.h"
#include "Diagnostics/SovPerformancePolicy.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SovPerformanceCapture
{
TAutoConsoleVariable<int32> Enabled(TEXT("sov.PerfCapture"), 0,
	TEXT("Opt-in local frame-time capture. 1 arms, 0 stops. Bounded; never exports on its own."),
	ECVF_Default);
TAutoConsoleVariable<float> TargetMilliseconds(TEXT("sov.PerfCapture.TargetMs"), 16.6667f,
	TEXT("Steady-state frame budget in milliseconds. 16.6667 is 60Hz."), ECVF_Default);
TAutoConsoleVariable<float> HardStallMilliseconds(TEXT("sov.PerfCapture.HardStallMs"), 100.f,
	TEXT("Any single frame at or above this fails the capture outright."), ECVF_Default);
TAutoConsoleVariable<float> AllowedOverBudgetFraction(TEXT("sov.PerfCapture.AllowedOverFraction"), 0.05f,
	TEXT("Share of frames permitted above the target, 0..1."), ECVF_Default);
TAutoConsoleVariable<float> JudgedPercentile(TEXT("sov.PerfCapture.JudgedPercentile"), 0.95f,
	TEXT("Percentile the target is judged at, 0..1. 0.95 judges p95."), ECVF_Default);
TAutoConsoleVariable<float> MemoryToleranceMegabytes(TEXT("sov.PerfCapture.MemoryToleranceMb"), 64.f,
	TEXT("Per-load rise in used memory, in megabytes, treated as noise when judging the reload trend."), ECVF_Default);

TAutoConsoleVariable<int32> ExportOnEnd(TEXT("sov.PerfCapture.ExportOnEnd"), 0,
	TEXT("1 writes a report under Saved/Diagnostics when each captured game world ends. Never on by default."), ECVF_Default);
TAutoConsoleVariable<float> ExportIntervalSeconds(TEXT("sov.PerfCapture.ExportIntervalSeconds"), 0.f,
	TEXT("When positive, also writes a report this often during play, so a crash cannot erase a capture."), ECVF_Default);
TAutoConsoleVariable<int32> ReloadCount(TEXT("sov.PerfCapture.ReloadCount"), 0,
	TEXT("Packaged game only: reopen the current map this many times, so load memory is judged across reloads."), ECVF_Default);
TAutoConsoleVariable<float> ReloadAfterSeconds(TEXT("sov.PerfCapture.ReloadAfterSeconds"), 60.f,
	TEXT("Steady-state seconds captured in each world before the next reload or quit."), ECVF_Default);
TAutoConsoleVariable<int32> QuitAfterReloads(TEXT("sov.PerfCapture.QuitAfterReloads"), 0,
	TEXT("Packaged game only: exit after the final reloaded world has been captured."), ECVF_Default);

/** Process lifetime, so a scheduled capture spans the worlds each reload creates. */
int32& ReloadsRequested() { static int32 Count = 0; return Count; }
int32& ExportSequence() { static int32 Sequence = 0; return Sequence; }

constexpr int32 MaximumLoadMemorySamples = 64;
constexpr double BytesPerMegabyte = 1024.0 * 1024.0;

/** Process lifetime: each reload creates a new world subsystem, and the trend spans reloads. */
TArray<double>& LoadMemorySamples()
{
	static TArray<double> Samples;
	return Samples;
}

const TCHAR* MemoryTrendName(ESovMemoryTrend Trend)
{
	switch (Trend)
	{
	case ESovMemoryTrend::Insufficient: return TEXT("Insufficient");
	case ESovMemoryTrend::Stable: return TEXT("Stable");
	case ESovMemoryTrend::Growing: return TEXT("Growing");
	}
	return TEXT("Insufficient");
}

SovPerformancePolicy::FFrameBudget ConfiguredBudget()
{
	SovPerformancePolicy::FFrameBudget Budget;
	Budget.TargetMilliseconds = static_cast<double>(TargetMilliseconds.GetValueOnAnyThread());
	Budget.HardStallMilliseconds = static_cast<double>(HardStallMilliseconds.GetValueOnAnyThread());
	Budget.AllowedOverBudgetFraction = static_cast<double>(AllowedOverBudgetFraction.GetValueOnAnyThread());
	Budget.JudgedPercentile = static_cast<double>(JudgedPercentile.GetValueOnAnyThread());
	return Budget;
}

ESovPerformanceVerdict ToBlueprintVerdict(SovPerformancePolicy::EVerdict Verdict)
{
	switch (Verdict)
	{
	case SovPerformancePolicy::EVerdict::InvalidBudget: return ESovPerformanceVerdict::InvalidBudget;
	case SovPerformancePolicy::EVerdict::Insufficient: return ESovPerformanceVerdict::Insufficient;
	case SovPerformancePolicy::EVerdict::Pass: return ESovPerformanceVerdict::Pass;
	case SovPerformancePolicy::EVerdict::Fail: return ESovPerformanceVerdict::Fail;
	}
	// A verdict this code does not recognise must not read as a pass.
	return ESovPerformanceVerdict::Insufficient;
}

/** Keeps only characters the policy permits verbatim, so a field cannot break the report. */
FString SafeField(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());
	for (const TCHAR Character : Value)
	{
		const bool bAscii = Character >= 0 && Character < 128;
		if (bAscii && SovPerformancePolicy::ReportSafeCharacter(static_cast<char>(Character)))
		{
			Result.AppendChar(Character);
		}
		else
		{
			Result.AppendChar(TEXT('_'));
		}
	}
	return Result.IsEmpty() ? FString(TEXT("unknown")) : Result;
}

const TCHAR* VerdictName(ESovPerformanceVerdict Verdict)
{
	switch (Verdict)
	{
	case ESovPerformanceVerdict::InvalidBudget: return TEXT("InvalidBudget");
	case ESovPerformanceVerdict::Insufficient: return TEXT("Insufficient");
	case ESovPerformanceVerdict::Pass: return TEXT("Pass");
	case ESovPerformanceVerdict::Fail: return TEXT("Fail");
	}
	return TEXT("Insufficient");
}
}

void USovPerformanceCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ClearSamples();
}

void USovPerformanceCaptureSubsystem::Deinitialize()
{
	// Opt-in unattended export: a packaged capture has no Blueprint caller for ExportLocalReport.
	if (IsCapturing() && SovPerformanceCapture::ExportOnEnd.GetValueOnAnyThread() != 0 && Samples.Num() > 0)
	{
		FString Relative, Error;
		if (!ExportLocalReport(Relative, Error))
		{
			UE_LOG(LogSovPerformance, Warning, TEXT("Performance capture export on world end failed: %s"), *Error);
		}
	}
	ClearSamples();
	Super::Deinitialize();
}

bool USovPerformanceCaptureSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USovPerformanceCaptureSubsystem::IsCapturing() const
{
	return SovPerformanceCapture::Enabled.GetValueOnAnyThread() != 0;
}

void USovPerformanceCaptureSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsCapturing())
	{
		return;
	}
	RecordSampleMilliseconds(DeltaTime * 1000.f);
	const float Interval = SovPerformanceCapture::ExportIntervalSeconds.GetValueOnAnyThread();
	if (FMath::IsFinite(Interval) && Interval > 0.f && GetWorld()
		&& Samples.Num() >= static_cast<int32>(SovPerformancePolicy::MinimumSamplesForVerdict)
		&& static_cast<double>(GetWorld()->GetTimeSeconds()) - LastPeriodicExportSeconds >= static_cast<double>(FMath::Max(Interval, 5.f)))
	{
		LastPeriodicExportSeconds = static_cast<double>(GetWorld()->GetTimeSeconds());
		FString Relative, Error;
		if (!ExportLocalReport(Relative, Error))
		{
			UE_LOG(LogSovPerformance, Warning, TEXT("Periodic performance capture export failed: %s"), *Error);
		}
	}
	UpdateScheduledCapture();
}

void USovPerformanceCaptureSubsystem::UpdateScheduledCapture()
{
	UWorld* World = GetWorld();
	// Reload and quit are packaged-game conveniences only; editor and PIE worlds are never driven.
	if (!World || World->WorldType != EWorldType::Game || GIsEditor || bScheduledActionRequested) { return; }
	const int32 Reloads = FMath::Max(0, SovPerformanceCapture::ReloadCount.GetValueOnAnyThread());
	const bool bQuit = SovPerformanceCapture::QuitAfterReloads.GetValueOnAnyThread() != 0;
	if (Reloads <= 0 && !bQuit) { return; }
	const float After = SovPerformanceCapture::ReloadAfterSeconds.GetValueOnAnyThread();
	const float Steady = FMath::IsFinite(After) ? FMath::Clamp(After, 5.f, 3600.f) : 60.f;
	if (Samples.Num() < static_cast<int32>(SovPerformancePolicy::MinimumSamplesForVerdict)
		|| static_cast<double>(World->GetTimeSeconds()) - FirstAdmittedWorldSeconds < static_cast<double>(Steady)) { return; }
	bScheduledActionRequested = true;
	int32& Requested = SovPerformanceCapture::ReloadsRequested();
	if (Requested < Reloads)
	{
		++Requested;
		const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		UE_LOG(LogSovPerformance, Log, TEXT("Performance capture reload %d of %d: %s"), Requested, Reloads, *Map);
		UGameplayStatics::OpenLevel(World, FName(*Map));
	}
	else if (bQuit)
	{
		UE_LOG(LogSovPerformance, Log, TEXT("Performance capture schedule complete after %d reloads; exiting."), Requested);
		FPlatformMisc::RequestExit(false, TEXT("SovPerformanceCapture"));
	}
}

void USovPerformanceCaptureSubsystem::RecordSampleMilliseconds(float Milliseconds)
{
	const double Value = static_cast<double>(Milliseconds);
	if (!SovPerformancePolicy::ValidFrameMilliseconds(Value))
	{
		++RejectedSamples;
		return;
	}
	// Warm-up frames are counted and discarded: shader compilation and streaming are not steady state.
	if (!SovPerformancePolicy::PastWarmup(static_cast<std::size_t>(FramesObserved)))
	{
		++FramesObserved;
		++WarmupDiscarded;
		return;
	}
	++FramesObserved;
	if (Samples.Num() == 0 && GetWorld()) { FirstAdmittedWorldSeconds = static_cast<double>(GetWorld()->GetTimeSeconds()); }
	if (!bLoadMemoryRecorded)
	{
		// One steady-state reading per captured world; loading and warm-up spikes are excluded.
		bLoadMemoryRecorded = true;
		RecordLoadMemoryBytes(static_cast<double>(FPlatformMemory::GetStats().UsedPhysical));
	}
	if (SovPerformancePolicy::DropOldestBeforeAppend(static_cast<std::size_t>(Samples.Num())))
	{
		Samples.RemoveAt(0, 1, EAllowShrinking::No);
		++DroppedOldest;
	}
	Samples.Add(Value);
}

void USovPerformanceCaptureSubsystem::ClearSamples()
{
	Samples.Reset();
	FramesObserved = 0;
	RejectedSamples = 0;
	DroppedOldest = 0;
	WarmupDiscarded = 0;
	bLoadMemoryRecorded = false;
	bScheduledActionRequested = false;
	FirstAdmittedWorldSeconds = 0.;
	LastPeriodicExportSeconds = 0.;
}

void USovPerformanceCaptureSubsystem::RecordLoadMemoryBytes(double UsedBytes)
{
	if (!SovPerformancePolicy::ValidMemoryBytes(UsedBytes))
	{
		++RejectedSamples;
		return;
	}
	TArray<double>& Loads = SovPerformanceCapture::LoadMemorySamples();
	if (Loads.Num() >= SovPerformanceCapture::MaximumLoadMemorySamples)
	{
		Loads.RemoveAt(0, 1, EAllowShrinking::No);
	}
	Loads.Add(UsedBytes);
}

void USovPerformanceCaptureSubsystem::ClearLoadMemory()
{
	SovPerformanceCapture::LoadMemorySamples().Reset();
}

FSovPerformanceCaptureSummary USovPerformanceCaptureSubsystem::BuildSummary() const
{
	using namespace SovPerformancePolicy;
	const FFrameBudget Budget = SovPerformanceCapture::ConfiguredBudget();

	FSovPerformanceCaptureSummary Summary;
	Summary.SampleCount = Samples.Num();
	Summary.WarmupDiscarded = WarmupDiscarded;
	Summary.RejectedSamples = RejectedSamples;
	Summary.DroppedOldest = DroppedOldest;
	Summary.TargetMilliseconds = static_cast<float>(Budget.TargetMilliseconds);
	Summary.HardStallMilliseconds = static_cast<float>(Budget.HardStallMilliseconds);
	Summary.AllowedOverBudgetFraction = static_cast<float>(Budget.AllowedOverBudgetFraction);
	Summary.JudgedPercentile = static_cast<float>(Budget.JudgedPercentile);
	const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
	Summary.UsedPhysicalMegabytes = static_cast<float>(static_cast<double>(Memory.UsedPhysical) / SovPerformanceCapture::BytesPerMegabyte);
	Summary.PeakUsedPhysicalMegabytes = static_cast<float>(static_cast<double>(Memory.PeakUsedPhysical) / SovPerformanceCapture::BytesPerMegabyte);
	const TArray<double>& Loads = SovPerformanceCapture::LoadMemorySamples();
	for (const double Load : Loads) { Summary.LoadMemoryMegabytes.Add(static_cast<float>(Load / SovPerformanceCapture::BytesPerMegabyte)); }
	const double ToleranceMegabytes = static_cast<double>(SovPerformanceCapture::MemoryToleranceMegabytes.GetValueOnAnyThread());
	Summary.MemoryToleranceMegabytes = static_cast<float>(ToleranceMegabytes);
	switch (SovPerformancePolicy::EvaluateLoadMemory(Loads.GetData(), static_cast<std::size_t>(Loads.Num()),
		ToleranceMegabytes * SovPerformanceCapture::BytesPerMegabyte))
	{
	case SovPerformancePolicy::EMemoryTrend::Stable: Summary.MemoryTrend = ESovMemoryTrend::Stable; break;
	case SovPerformancePolicy::EMemoryTrend::Growing: Summary.MemoryTrend = ESovMemoryTrend::Growing; break;
	default: Summary.MemoryTrend = ESovMemoryTrend::Insufficient; break;
	}
	Summary.Platform = FString(FPlatformProperties::PlatformName());
	Summary.BuildConfiguration = LexToString(FApp::GetBuildConfiguration());
	Summary.bEditorBuild = GIsEditor;
	// With rendering off there is no GPU cost in the frame, so the reading cannot qualify a budget.
	Summary.bRenderingDisabled = !FApp::CanEverRender();

	// Percentiles need ascending order; the streak needs chronological order. Keep both.
	TArray<double> Sorted = Samples;
	Sorted.Sort();

	const std::size_t Count = static_cast<std::size_t>(Sorted.Num());
	Summary.Verdict = SovPerformanceCapture::ToBlueprintVerdict(
		Evaluate(Sorted.GetData(), Count, Budget));
	if (Count > 0)
	{
		Summary.MinimumMilliseconds = static_cast<float>(Sorted[0]);
		Summary.MedianMilliseconds = static_cast<float>(PercentileOfSorted(Sorted.GetData(), Count, 0.50));
		Summary.Percentile95Milliseconds = static_cast<float>(PercentileOfSorted(Sorted.GetData(), Count, 0.95));
		Summary.Percentile99Milliseconds = static_cast<float>(PercentileOfSorted(Sorted.GetData(), Count, 0.99));
		Summary.MaximumMilliseconds = static_cast<float>(Sorted[Sorted.Num() - 1]);
		Summary.OverBudgetCount = static_cast<int32>(
			CountOverBudget(Sorted.GetData(), Count, Budget.TargetMilliseconds));
		Summary.LongestOverBudgetStreak = static_cast<int32>(
			LongestOverBudgetStreak(Samples.GetData(), Count, Budget.TargetMilliseconds));
	}
	return Summary;
}

bool USovPerformanceCaptureSubsystem::ExportLocalReport(FString& OutRelativePath, FString& Error) const
{
	OutRelativePath.Reset();
	Error.Reset();
	const FSovPerformanceCaptureSummary Summary = BuildSummary();
	if (Summary.SampleCount <= 0)
	{
		Error = TEXT("No admitted samples; nothing to export.");
		return false;
	}

	const FString Platform = SovPerformanceCapture::SafeField(Summary.Platform);
	const FString Configuration = SovPerformanceCapture::SafeField(Summary.BuildConfiguration);

	// Written by hand rather than through the Json module, so the runtime module's dependencies are
	// unchanged. Every value below is a number, a bool, or a field passed through SafeField.
	FString Json;
	Json += TEXT("{\n");
	Json += TEXT("  \"schema\": 1,\n");
	Json += FString::Printf(TEXT("  \"verdict\": \"%s\",\n"), SovPerformanceCapture::VerdictName(Summary.Verdict));
	Json += FString::Printf(TEXT("  \"platform\": \"%s\",\n"), *Platform);
	Json += FString::Printf(TEXT("  \"build_configuration\": \"%s\",\n"), *Configuration);
	Json += FString::Printf(TEXT("  \"editor_build\": %s,\n"), Summary.bEditorBuild ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("  \"rendering_disabled\": %s,\n"), Summary.bRenderingDisabled ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("  \"sample_count\": %d,\n"), Summary.SampleCount);
	Json += FString::Printf(TEXT("  \"warmup_discarded\": %d,\n"), Summary.WarmupDiscarded);
	Json += FString::Printf(TEXT("  \"rejected_samples\": %d,\n"), Summary.RejectedSamples);
	Json += FString::Printf(TEXT("  \"dropped_oldest\": %d,\n"), Summary.DroppedOldest);
	Json += FString::Printf(TEXT("  \"min_ms\": %.4f,\n"), Summary.MinimumMilliseconds);
	Json += FString::Printf(TEXT("  \"p50_ms\": %.4f,\n"), Summary.MedianMilliseconds);
	Json += FString::Printf(TEXT("  \"p95_ms\": %.4f,\n"), Summary.Percentile95Milliseconds);
	Json += FString::Printf(TEXT("  \"p99_ms\": %.4f,\n"), Summary.Percentile99Milliseconds);
	Json += FString::Printf(TEXT("  \"max_ms\": %.4f,\n"), Summary.MaximumMilliseconds);
	Json += FString::Printf(TEXT("  \"over_budget_count\": %d,\n"), Summary.OverBudgetCount);
	Json += FString::Printf(TEXT("  \"longest_over_budget_streak\": %d,\n"), Summary.LongestOverBudgetStreak);
	Json += FString::Printf(TEXT("  \"target_ms\": %.4f,\n"), Summary.TargetMilliseconds);
	Json += FString::Printf(TEXT("  \"hard_stall_ms\": %.4f,\n"), Summary.HardStallMilliseconds);
	Json += FString::Printf(TEXT("  \"allowed_over_fraction\": %.4f,\n"), Summary.AllowedOverBudgetFraction);
	Json += FString::Printf(TEXT("  \"judged_percentile\": %.4f,\n"), Summary.JudgedPercentile);
	Json += FString::Printf(TEXT("  \"used_physical_mb\": %.1f,\n"), Summary.UsedPhysicalMegabytes);
	Json += FString::Printf(TEXT("  \"peak_used_physical_mb\": %.1f,\n"), Summary.PeakUsedPhysicalMegabytes);
	Json += FString::Printf(TEXT("  \"memory_trend\": \"%s\",\n"), SovPerformanceCapture::MemoryTrendName(Summary.MemoryTrend));
	Json += FString::Printf(TEXT("  \"memory_tolerance_mb\": %.1f,\n"), Summary.MemoryToleranceMegabytes);
	Json += TEXT("  \"load_memory_mb\": [");
	for (int32 Index = 0; Index < Summary.LoadMemoryMegabytes.Num(); ++Index)
	{
		Json += FString::Printf(TEXT("%s%.1f"), Index == 0 ? TEXT("") : TEXT(", "), Summary.LoadMemoryMegabytes[Index]);
	}
	Json += TEXT("],\n");
	// Stated in the artefact itself so a desktop capture cannot be quoted as a console result.
	Json += TEXT("  \"scope\": \"Local capture on the recorded platform only. ");
	Json += TEXT("Not a console or certification capture.\"\n");
	Json += TEXT("}\n");

	// A per-process sequence keeps several world reports written within one second distinct.
	const FString Relative = FString::Printf(TEXT("Diagnostics/PerfCapture-%s-%s-%03d.json"),
		*Platform, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")), ++SovPerformanceCapture::ExportSequence());
	const FString Absolute = FPaths::Combine(FPaths::ProjectSavedDir(), Relative);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Absolute), true);
	if (!FFileHelper::SaveStringToFile(Json, *Absolute, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Error = FString::Printf(TEXT("Could not write %s"), *Relative);
		return false;
	}
	OutRelativePath = Relative;
	UE_LOG(LogSovPerformance, Log, TEXT("Performance capture exported: %s (verdict %s, %d samples, %s)"),
		*Relative, SovPerformanceCapture::VerdictName(Summary.Verdict), Summary.SampleCount, *Platform);
	return true;
}
