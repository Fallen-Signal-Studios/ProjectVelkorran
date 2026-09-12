// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>

/**
 * Bounded frame-time admission and budget evaluation for the local performance capture.
 *
 * Pure C++ so the production rules are exercised by Tests/Portable without an Unreal build.
 * Deliberate properties, each covered by a portable test:
 *
 *  - Absence of data is never a pass. Too few samples yields Insufficient, not Pass. A capture
 *    that collected nothing must not read as a clean frame budget; that distinction is the whole
 *    point of the verdict enum.
 *  - Percentiles use nearest-rank with no interpolation, so a verdict is reproducible across
 *    platforms and compilers rather than depending on floating-point blending.
 *  - Percentile input is sorted ascending; streak input is chronological. The two orderings are
 *    named in the parameters because passing one for the other is silently wrong.
 *  - A verdict describes only the samples handed in. It carries no platform claim: the capture
 *    layer records host and target, because a desktop capture is not a console capture.
 */
namespace SovPerformancePolicy
{
constexpr std::size_t MaximumSamples = 4096;
/** Frames discarded at capture start; shader warm-up and level streaming are not steady state. */
constexpr std::size_t WarmupSamplesDiscarded = 60;
/** Below this, a capture is too short to qualify anything. Two seconds at 60Hz. */
constexpr std::size_t MinimumSamplesForVerdict = 120;
/** Beyond this a reading is a stopped clock, a breakpoint or a suspended process, not a frame. */
constexpr double ImplausibleFrameMilliseconds = 10000.0;

enum class EVerdict
{
	/** The budget itself is not usable. Never reported as a pass or a failure of the build. */
	InvalidBudget,
	/** Not enough steady-state samples to qualify anything. Not a pass. */
	Insufficient,
	Pass,
	Fail,
};

struct FFrameBudget
{
	/** Steady-state target, e.g. 16.6667 for 60Hz. */
	double TargetMilliseconds = 0.0;
	/** Share of frames permitted to exceed the target, in [0, 1]. */
	double AllowedOverBudgetFraction = 0.0;
	/** Any single frame at or above this fails outright, however good the percentiles are. */
	double HardStallMilliseconds = 0.0;
	/** Percentile the target is judged at, in (0, 1]. 0.95 judges p95. */
	double JudgedPercentile = 0.95;
};

/** True when Value is finite, positive and physically plausible as one frame. */
inline bool ValidFrameMilliseconds(double Value)
{
	// Comparisons rather than <cmath> so this header stays dependency-free; NaN fails both.
	const bool bIsNaN = !(Value == Value);
	if (bIsNaN) { return false; }
	if (!(Value > 0.0)) { return false; }
	if (!(Value < ImplausibleFrameMilliseconds)) { return false; }
	return true;
}

inline bool ValidBudget(const FFrameBudget& Budget)
{
	if (!ValidFrameMilliseconds(Budget.TargetMilliseconds)) { return false; }
	const bool bStallIsNaN = !(Budget.HardStallMilliseconds == Budget.HardStallMilliseconds);
	const bool bFractionIsNaN = !(Budget.AllowedOverBudgetFraction == Budget.AllowedOverBudgetFraction);
	const bool bPercentileIsNaN = !(Budget.JudgedPercentile == Budget.JudgedPercentile);
	if (bStallIsNaN || bFractionIsNaN || bPercentileIsNaN) { return false; }
	if (!(Budget.AllowedOverBudgetFraction >= 0.0) || !(Budget.AllowedOverBudgetFraction <= 1.0)) { return false; }
	if (!(Budget.JudgedPercentile > 0.0) || !(Budget.JudgedPercentile <= 1.0)) { return false; }
	// A hard stall at or below the target would fail every frame that merely meets the target.
	if (!(Budget.HardStallMilliseconds > Budget.TargetMilliseconds)) { return false; }
	return true;
}

/** Ring-buffer rule: the oldest sample is dropped once the bound is reached. */
inline bool DropOldestBeforeAppend(std::size_t Count) { return Count >= MaximumSamples; }

/** True once enough frames have elapsed that samples represent steady state. */
inline bool PastWarmup(std::size_t FramesObserved) { return FramesObserved >= WarmupSamplesDiscarded; }

/**
 * Nearest-rank percentile over samples sorted ascending. No interpolation, so the result is one
 * of the observed samples and is bit-identical on every platform.
 */
inline double PercentileOfSorted(const double* SortedAscending, std::size_t Count, double Fraction)
{
	if (!SortedAscending || Count == 0) { return 0.0; }
	const bool bFractionIsNaN = !(Fraction == Fraction);
	if (bFractionIsNaN || !(Fraction > 0.0)) { return SortedAscending[0]; }
	if (!(Fraction < 1.0)) { return SortedAscending[Count - 1]; }
	// ceil(Fraction * Count) - 1 without <cmath>.
	const double Scaled = Fraction * static_cast<double>(Count);
	std::size_t Rank = static_cast<std::size_t>(Scaled);
	if (static_cast<double>(Rank) < Scaled) { ++Rank; }
	if (Rank == 0) { Rank = 1; }
	if (Rank > Count) { Rank = Count; }
	return SortedAscending[Rank - 1];
}

/** Count of samples strictly exceeding the target, over samples in any order. */
inline std::size_t CountOverBudget(const double* Samples, std::size_t Count, double TargetMilliseconds)
{
	if (!Samples) { return 0; }
	std::size_t Over = 0;
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		if (Samples[Index] > TargetMilliseconds) { ++Over; }
	}
	return Over;
}

/** Longest run of consecutive over-budget frames. Requires chronological order to mean anything. */
inline std::size_t LongestOverBudgetStreak(const double* Chronological, std::size_t Count, double TargetMilliseconds)
{
	if (!Chronological) { return 0; }
	std::size_t Longest = 0;
	std::size_t Current = 0;
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		if (Chronological[Index] > TargetMilliseconds)
		{
			++Current;
			if (Current > Longest) { Longest = Current; }
		}
		else
		{
			Current = 0;
		}
	}
	return Longest;
}

/** True when any sample is at or above the budget's hard stall threshold. */
inline bool HasHardStall(const double* Samples, std::size_t Count, const FFrameBudget& Budget)
{
	if (!Samples) { return false; }
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		if (Samples[Index] >= Budget.HardStallMilliseconds) { return true; }
	}
	return false;
}

/**
 * Judge a capture. SortedAscending must hold Count post-warmup samples sorted ascending.
 *
 * Order of decision is deliberate: an unusable budget is reported before sample count, and both
 * are reported before any pass. Nothing here can return Pass for an empty capture.
 */
inline EVerdict Evaluate(const double* SortedAscending, std::size_t Count, const FFrameBudget& Budget)
{
	if (!ValidBudget(Budget)) { return EVerdict::InvalidBudget; }
	if (!SortedAscending || Count < MinimumSamplesForVerdict) { return EVerdict::Insufficient; }
	if (HasHardStall(SortedAscending, Count, Budget)) { return EVerdict::Fail; }
	if (PercentileOfSorted(SortedAscending, Count, Budget.JudgedPercentile) > Budget.TargetMilliseconds)
	{
		return EVerdict::Fail;
	}
	const std::size_t Over = CountOverBudget(SortedAscending, Count, Budget.TargetMilliseconds);
	const double Fraction = static_cast<double>(Over) / static_cast<double>(Count);
	if (Fraction > Budget.AllowedOverBudgetFraction) { return EVerdict::Fail; }
	return EVerdict::Pass;
}

/**
 * Characters permitted verbatim in a report field.
 *
 * The report is written without a JSON library, to keep the runtime module's dependency surface
 * as it was, so any text placed in it must be provably free of quotes and escapes. Engine-supplied
 * identifiers satisfy this today; filtering rather than trusting them means a future platform name
 * cannot silently produce a malformed report.
 */
inline bool ReportSafeCharacter(char Character)
{
	const bool bLower = Character >= 'a' && Character <= 'z';
	const bool bUpper = Character >= 'A' && Character <= 'Z';
	const bool bDigit = Character >= '0' && Character <= '9';
	return bLower || bUpper || bDigit || Character == '.' || Character == '_' || Character == '-';
}

/** A verdict that qualifies the samples. Insufficient and InvalidBudget deliberately do not. */
inline bool QualifiesCapture(EVerdict Verdict)
{
	return Verdict == EVerdict::Pass || Verdict == EVerdict::Fail;
}
}
