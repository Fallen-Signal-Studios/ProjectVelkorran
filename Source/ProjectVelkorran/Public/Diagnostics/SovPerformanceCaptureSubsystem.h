// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SovPerformanceCaptureSubsystem.generated.h"

/** Mirrors SovPerformancePolicy::EVerdict for Blueprint and report consumers. */
UENUM(BlueprintType)
enum class ESovPerformanceVerdict : uint8
{
	/** The configured budget is unusable. Never reported as a pass or a failure. */
	InvalidBudget,
	/** Too few steady-state samples to qualify anything. Deliberately not a pass. */
	Insufficient,
	Pass,
	Fail,
};

/** Mirrors SovPerformancePolicy::EMemoryTrend. */
UENUM(BlueprintType)
enum class ESovMemoryTrend : uint8
{
	Insufficient,
	Stable,
	Growing,
};

/**
 * One capture's outcome, including the identity of the machine that produced it.
 *
 * The platform fields are not decoration. A frame-time figure means nothing without the host it
 * came from, and a desktop capture must never be readable as a console capture. Every export
 * carries them so a number cannot be quoted free of its platform.
 */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovPerformanceCaptureSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) ESovPerformanceVerdict Verdict = ESovPerformanceVerdict::Insufficient;
	/** Samples retained after warm-up discard, which is what the verdict is based on. */
	UPROPERTY(BlueprintReadOnly) int32 SampleCount = 0;
	/** Frames observed before warm-up completed and therefore never admitted. */
	UPROPERTY(BlueprintReadOnly) int32 WarmupDiscarded = 0;
	/** Readings rejected as non-finite, non-positive or implausibly long. */
	UPROPERTY(BlueprintReadOnly) int32 RejectedSamples = 0;
	/** Samples dropped from the front because the bounded buffer was full. */
	UPROPERTY(BlueprintReadOnly) int32 DroppedOldest = 0;
	UPROPERTY(BlueprintReadOnly) float MinimumMilliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float MedianMilliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float Percentile95Milliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float Percentile99Milliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float MaximumMilliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 OverBudgetCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 LongestOverBudgetStreak = 0;
	UPROPERTY(BlueprintReadOnly) float TargetMilliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float HardStallMilliseconds = 0.f;
	UPROPERTY(BlueprintReadOnly) float AllowedOverBudgetFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) float JudgedPercentile = 0.f;
	/** Process used physical memory when the summary was built, and the operating system's peak. */
	UPROPERTY(BlueprintReadOnly) float UsedPhysicalMegabytes = 0.f;
	UPROPERTY(BlueprintReadOnly) float PeakUsedPhysicalMegabytes = 0.f;
	/** One reading per mission world, taken as its warm-up completes, across this process. */
	UPROPERTY(BlueprintReadOnly) TArray<float> LoadMemoryMegabytes;
	UPROPERTY(BlueprintReadOnly) ESovMemoryTrend MemoryTrend = ESovMemoryTrend::Insufficient;
	UPROPERTY(BlueprintReadOnly) float MemoryToleranceMegabytes = 0.f;

	/** "Mac", "Windows", and so on. Never inferred by a reader; always recorded. */
	UPROPERTY(BlueprintReadOnly) FString Platform;
	UPROPERTY(BlueprintReadOnly) FString BuildConfiguration;
	/** Editor captures include editor-only cost and do not represent a shipping build. */
	UPROPERTY(BlueprintReadOnly) bool bEditorBuild = false;
	/** A capture with rendering disabled measures no GPU cost and cannot qualify a frame budget. */
	UPROPERTY(BlueprintReadOnly) bool bRenderingDisabled = false;
};

/**
 * Bounded, opt-in local frame-time capture. Armed by sov.PerfCapture; never on by default, never
 * networked, and never exported without an explicit call.
 *
 * The judging rules live in SovPerformancePolicy so they are exercised by Tests/Portable without an
 * Unreal build. This class owns only the plumbing: admission, bounds, and identity of the host.
 */
UCLASS()
class PROJECTVELKORRAN_API USovPerformanceCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(USovPerformanceCaptureSubsystem, STATGROUP_Tickables);
	}
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** True while sov.PerfCapture arms this world. */
	UFUNCTION(BlueprintPure, Category="Sovereign|Performance") bool IsCapturing() const;

	/**
	 * Admit one frame duration. This is the production path the tick uses, and the same path tests
	 * drive, so a test cannot pass through plumbing that production does not use.
	 */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Performance")
	void RecordSampleMilliseconds(float Milliseconds);

	UFUNCTION(BlueprintPure, Category="Sovereign|Performance")
	FSovPerformanceCaptureSummary BuildSummary() const;

	/** Explicit action writes one JSON report under Saved/Diagnostics. No arbitrary destination. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Performance")
	bool ExportLocalReport(FString& OutRelativePath, FString& Error) const;

	UFUNCTION(BlueprintCallable, Category="Sovereign|Performance") void ClearSamples();

	/**
	 * Admit one mission-load memory reading. Production calls this once per captured world when warm-up
	 * completes; tests drive the same path. Readings persist across worlds for the process lifetime,
	 * because the trend is judged across reloads that each create a new subsystem.
	 */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Performance")
	void RecordLoadMemoryBytes(double UsedBytes);

	UFUNCTION(BlueprintCallable, Category="Sovereign|Performance") void ClearLoadMemory();

	/**
	 * Every valid frame this capture has seen, warm-up and admitted alike. Exposed so a capture that
	 * produced no verdict can be told apart from one that never ran.
	 */
	UFUNCTION(BlueprintPure, Category="Sovereign|Performance")
	int32 GetFramesObserved() const { return FramesObserved; }

private:
	/** Chronological, bounded by SovPerformancePolicy::MaximumSamples. */
	TArray<double> Samples;
	int32 FramesObserved = 0;
	int32 RejectedSamples = 0;
	int32 DroppedOldest = 0;
	int32 WarmupDiscarded = 0;
	bool bLoadMemoryRecorded = false;
	/** One reload or quit request per world; the request itself retires this world. */
	bool bScheduledActionRequested = false;
	double FirstAdmittedWorldSeconds = 0.;
	/** Opt-in packaged-game reload/quit schedule driven by sov.PerfCapture.ReloadCount and QuitAfterReloads. */
	void UpdateScheduledCapture();
};
