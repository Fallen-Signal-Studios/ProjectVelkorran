// Copyright Fallen Signal Studios. All Rights Reserved.
// Covers the native plumbing of the local frame-time capture: admission, warm-up discard,
// bounds, verdicts and the platform identity recorded in every summary.
//
// The judging arithmetic itself is covered exhaustively by Tests/Portable/SovPerformancePolicyTests.cpp
// without an Unreal build; these tests exist for what that cannot reach - the subsystem, the cvars,
// the tick path and the export.
//
// One assertion here is deliberately about honesty rather than behaviour: automation runs under
// -NullRHI, where no GPU work occurs, so a summary produced in this environment must mark itself as
// rendering-disabled. A frame time measured with rendering off cannot qualify a frame budget, and
// the artefact has to say so itself rather than relying on whoever reads it to remember.
#include "Diagnostics/SovPerformanceCaptureSubsystem.h"
#include "Diagnostics/SovPerformancePolicy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
namespace SovPerformanceCaptureTests
{
/** Sets the capture cvars to known values and restores them, so verdicts are deterministic. */
struct FBudgetScope
{
	FBudgetScope(float Target, float HardStall, float AllowedFraction, float Percentile)
	{
		Apply(TEXT("sov.PerfCapture.TargetMs"), Target);
		Apply(TEXT("sov.PerfCapture.HardStallMs"), HardStall);
		Apply(TEXT("sov.PerfCapture.AllowedOverFraction"), AllowedFraction);
		Apply(TEXT("sov.PerfCapture.JudgedPercentile"), Percentile);
	}
	~FBudgetScope()
	{
		for (int32 Index = Names.Num() - 1; Index >= 0; --Index)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Names[Index]))
			{
				Variable->Set(Previous[Index], ECVF_SetByCode);
			}
		}
	}
	void Apply(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Names.Add(Name);
			Previous.Add(Variable->GetFloat());
			Variable->Set(Value, ECVF_SetByCode);
		}
	}
	TArray<FString> Names;
	TArray<float> Previous;
};

struct FArmScope
{
	explicit FArmScope(int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("sov.PerfCapture")))
		{
			Previous = Variable->GetInt();
			Variable->Set(Value, ECVF_SetByCode);
			bApplied = true;
		}
	}
	~FArmScope()
	{
		if (bApplied)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("sov.PerfCapture")))
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}
	}
	int32 Previous = 0;
	bool bApplied = false;
};

struct FWorld
{
	FWorld()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		if (World && GEngine)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
	}
	~FWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	}
	USovPerformanceCaptureSubsystem* Capture() const
	{
		return World ? World->GetSubsystem<USovPerformanceCaptureSubsystem>() : nullptr;
	}
	UWorld* World = nullptr;
};

/** Pushes Count samples through the warm-up so later samples are admitted. */
void CompleteWarmup(USovPerformanceCaptureSubsystem& Subsystem)
{
	for (std::size_t Index = 0; Index < SovPerformancePolicy::WarmupSamplesDiscarded; ++Index)
	{
		Subsystem.RecordSampleMilliseconds(8.f);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerformanceCaptureAdmission, "ProjectVelkorran.Diagnostics.Performance.Admission", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovPerformanceCaptureAdmission::RunTest(const FString& Parameters)
{
	using namespace SovPerformanceCaptureTests;
	FWorld Scope;
	USovPerformanceCaptureSubsystem* Subsystem = Scope.Capture();
	if (!TestNotNull(TEXT("The capture subsystem exists on a game world"), Subsystem))
	{
		return false;
	}

	// Warm-up frames are observed but never admitted.
	CompleteWarmup(*Subsystem);
	TestEqual(TEXT("Warm-up frames were observed but not admitted"), Subsystem->GetFramesObserved(),
		static_cast<int32>(SovPerformancePolicy::WarmupSamplesDiscarded));
	TestEqual(TEXT("No warm-up frame was admitted"), Subsystem->BuildSummary().SampleCount, 0);
	TestEqual(TEXT("Warm-up discards are reported"), Subsystem->BuildSummary().WarmupDiscarded,
		static_cast<int32>(SovPerformancePolicy::WarmupSamplesDiscarded));

	// Implausible readings are rejected and counted rather than skewing a percentile.
	Subsystem->RecordSampleMilliseconds(0.f);
	Subsystem->RecordSampleMilliseconds(-16.f);
	Subsystem->RecordSampleMilliseconds(std::numeric_limits<float>::quiet_NaN());
	Subsystem->RecordSampleMilliseconds(std::numeric_limits<float>::infinity());
	Subsystem->RecordSampleMilliseconds(
		static_cast<float>(SovPerformancePolicy::ImplausibleFrameMilliseconds) + 1.f);
	FSovPerformanceCaptureSummary Summary = Subsystem->BuildSummary();
	TestEqual(TEXT("Five implausible readings were rejected"), Summary.RejectedSamples, 5);
	TestEqual(TEXT("No implausible reading was admitted"), Summary.SampleCount, 0);

	Subsystem->RecordSampleMilliseconds(12.f);
	TestEqual(TEXT("A plausible reading is admitted"), Subsystem->BuildSummary().SampleCount, 1);

	Subsystem->ClearSamples();
	Summary = Subsystem->BuildSummary();
	TestEqual(TEXT("Clearing resets samples"), Summary.SampleCount, 0);
	TestEqual(TEXT("Clearing resets rejections"), Summary.RejectedSamples, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerformanceCaptureBounded, "ProjectVelkorran.Diagnostics.Performance.Bounded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovPerformanceCaptureBounded::RunTest(const FString& Parameters)
{
	using namespace SovPerformanceCaptureTests;
	FWorld Scope;
	USovPerformanceCaptureSubsystem* Subsystem = Scope.Capture();
	if (!TestNotNull(TEXT("The capture subsystem exists"), Subsystem)) { return false; }

	CompleteWarmup(*Subsystem);
	const int32 Bound = static_cast<int32>(SovPerformancePolicy::MaximumSamples);
	for (int32 Index = 0; Index < Bound + 250; ++Index)
	{
		Subsystem->RecordSampleMilliseconds(10.f);
	}
	const FSovPerformanceCaptureSummary Summary = Subsystem->BuildSummary();
	TestEqual(TEXT("The buffer never exceeds its bound"), Summary.SampleCount, Bound);
	TestEqual(TEXT("Overflow is reported as dropped oldest"), Summary.DroppedOldest, 250);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerformanceCaptureVerdicts, "ProjectVelkorran.Diagnostics.Performance.Verdicts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovPerformanceCaptureVerdicts::RunTest(const FString& Parameters)
{
	using namespace SovPerformanceCaptureTests;
	const FBudgetScope Budget(16.6667f, 100.f, 0.05f, 0.95f);
	FWorld Scope;
	USovPerformanceCaptureSubsystem* Subsystem = Scope.Capture();
	if (!TestNotNull(TEXT("The capture subsystem exists"), Subsystem)) { return false; }

	// A capture too short to judge is Insufficient, never Pass. Absence of data is not a pass.
	CompleteWarmup(*Subsystem);
	TestEqual(TEXT("An empty capture is Insufficient"),
		Subsystem->BuildSummary().Verdict, ESovPerformanceVerdict::Insufficient);
	for (std::size_t Index = 0; Index + 1 < SovPerformancePolicy::MinimumSamplesForVerdict; ++Index)
	{
		Subsystem->RecordSampleMilliseconds(8.f);
	}
	TestEqual(TEXT("One sample short of the minimum is still Insufficient"),
		Subsystem->BuildSummary().Verdict, ESovPerformanceVerdict::Insufficient);
	Subsystem->RecordSampleMilliseconds(8.f);
	TestEqual(TEXT("Reaching the minimum with clean frames passes"),
		Subsystem->BuildSummary().Verdict, ESovPerformanceVerdict::Pass);

	// A single hard stall fails a capture whose percentiles are otherwise comfortable.
	Subsystem->RecordSampleMilliseconds(250.f);
	const FSovPerformanceCaptureSummary Stalled = Subsystem->BuildSummary();
	TestEqual(TEXT("One hard stall fails the capture"), Stalled.Verdict, ESovPerformanceVerdict::Fail);
	TestTrue(TEXT("The stall is visible as the maximum"), Stalled.MaximumMilliseconds >= 250.f);
	TestEqual(TEXT("The stall is counted as over budget"), Stalled.OverBudgetCount, 1);

	// An unusable budget is reported as such, and is neither a pass nor a failure of the build.
	{
		const FBudgetScope Broken(16.6667f, 8.f, 0.05f, 0.95f);
		TestEqual(TEXT("A hard stall below the target is an invalid budget"),
			Subsystem->BuildSummary().Verdict, ESovPerformanceVerdict::InvalidBudget);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerformanceCaptureTickPath, "ProjectVelkorran.Diagnostics.Performance.TickPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovPerformanceCaptureTickPath::RunTest(const FString& Parameters)
{
	using namespace SovPerformanceCaptureTests;
	FWorld Scope;
	USovPerformanceCaptureSubsystem* Subsystem = Scope.Capture();
	if (!TestNotNull(TEXT("The capture subsystem exists"), Subsystem)) { return false; }

	// Disarmed, ticking must record nothing at all.
	{
		const FArmScope Disarmed(0);
		TestFalse(TEXT("The capture is not armed by default"), Subsystem->IsCapturing());
		for (int32 Index = 0; Index < 10; ++Index) { Subsystem->Tick(0.016f); }
		TestEqual(TEXT("A disarmed capture observes no frames"), Subsystem->GetFramesObserved(), 0);
	}
	// Armed, the tick feeds the same admission path production uses.
	{
		const FArmScope Armed(1);
		TestTrue(TEXT("The cvar arms the capture"), Subsystem->IsCapturing());
		const int32 Ticks = static_cast<int32>(SovPerformancePolicy::WarmupSamplesDiscarded) + 5;
		for (int32 Index = 0; Index < Ticks; ++Index) { Subsystem->Tick(0.016f); }
		const FSovPerformanceCaptureSummary Summary = Subsystem->BuildSummary();
		TestEqual(TEXT("Ticks past warm-up are admitted"), Summary.SampleCount, 5);
		TestTrue(TEXT("A 16ms tick is recorded in milliseconds, not seconds"),
			Summary.MedianMilliseconds > 15.f && Summary.MedianMilliseconds < 17.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerformanceCaptureRecordsPlatform, "ProjectVelkorran.Diagnostics.Performance.RecordsPlatform", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovPerformanceCaptureRecordsPlatform::RunTest(const FString& Parameters)
{
	using namespace SovPerformanceCaptureTests;
	FWorld Scope;
	USovPerformanceCaptureSubsystem* Subsystem = Scope.Capture();
	if (!TestNotNull(TEXT("The capture subsystem exists"), Subsystem)) { return false; }

	CompleteWarmup(*Subsystem);
	Subsystem->RecordSampleMilliseconds(9.f);
	const FSovPerformanceCaptureSummary Summary = Subsystem->BuildSummary();

	// A capture must identify its own host. A figure quoted without one is not interpretable,
	// and a desktop number must never be mistakable for a console number.
	TestFalse(TEXT("The summary records a platform"), Summary.Platform.IsEmpty());
	TestFalse(TEXT("The summary records a build configuration"), Summary.BuildConfiguration.IsEmpty());
	// Automation runs under -NullRHI, so the capture must declare that rendering was off. A frame
	// time measured with no GPU work cannot qualify a frame budget.
	TestEqual(TEXT("Rendering-disabled matches the running environment"),
		Summary.bRenderingDisabled, !FApp::CanEverRender());
	if (!FApp::CanEverRender())
	{
		TestTrue(TEXT("A capture with rendering off says so"), Summary.bRenderingDisabled);
	}

	// Export refuses to write anything when there is nothing to describe.
	Subsystem->ClearSamples();
	FString Path;
	FString Error;
	TestFalse(TEXT("Export refuses an empty capture"), Subsystem->ExportLocalReport(Path, Error));
	TestFalse(TEXT("Refusal explains itself"), Error.IsEmpty());
	TestTrue(TEXT("Refusal writes no path"), Path.IsEmpty());

	// A real export lands under Saved/Diagnostics and names its platform in the file itself.
	CompleteWarmup(*Subsystem);
	Subsystem->RecordSampleMilliseconds(9.f);
	if (TestTrue(TEXT("Export writes a report"), Subsystem->ExportLocalReport(Path, Error)))
	{
		TestTrue(TEXT("The report is under Saved/Diagnostics"), Path.StartsWith(TEXT("Diagnostics/")));
		TestTrue(TEXT("The report names its platform"), Path.Contains(Summary.Platform));
		const FString Absolute = FPaths::Combine(FPaths::ProjectSavedDir(), Path);
		TestTrue(TEXT("The report file exists"), IFileManager::Get().FileExists(*Absolute));
		IFileManager::Get().Delete(*Absolute);
	}
	return true;
}
#endif
