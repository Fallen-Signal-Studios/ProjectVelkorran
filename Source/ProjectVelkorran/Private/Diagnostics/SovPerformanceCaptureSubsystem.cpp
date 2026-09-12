// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Diagnostics/SovPerformanceCaptureSubsystem.h"

#include "Diagnostics/SovPerformancePolicy.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovPerformance, Log, All);

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
	// Stated in the artefact itself so a desktop capture cannot be quoted as a console result.
	Json += TEXT("  \"scope\": \"Local capture on the recorded platform only. ");
	Json += TEXT("Not a console or certification capture.\"\n");
	Json += TEXT("}\n");

	const FString Relative = FString::Printf(TEXT("Diagnostics/PerfCapture-%s-%s.json"),
		*Platform, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
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
