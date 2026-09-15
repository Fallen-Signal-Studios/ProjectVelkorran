// Compiles the same bounded frame-time admission and budget rules used by the production capture.
#include "Diagnostics/SovPerformancePolicy.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using namespace SovPerformancePolicy;

FFrameBudget SixtyHertz()
{
	FFrameBudget Budget;
	Budget.TargetMilliseconds = 16.6667;
	Budget.AllowedOverBudgetFraction = 0.05;
	Budget.HardStallMilliseconds = 100.0;
	Budget.JudgedPercentile = 0.95;
	return Budget;
}

std::vector<double> SortedAscending(std::vector<double> Values)
{
	std::sort(Values.begin(), Values.end());
	return Values;
}
}

int main()
{
	const double NaNValue = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	int Checks = 0;
	auto Check = [&Checks](bool Condition) { assert(Condition); ++Checks; };

	// Admission: only finite, positive, physically plausible frame durations.
	Check(ValidFrameMilliseconds(16.6667));
	Check(ValidFrameMilliseconds(0.001));
	Check(ValidFrameMilliseconds(9999.999));
	Check(!ValidFrameMilliseconds(0.0));
	Check(!ValidFrameMilliseconds(-0.001));
	Check(!ValidFrameMilliseconds(-16.6667));
	Check(!ValidFrameMilliseconds(NaNValue));
	Check(!ValidFrameMilliseconds(Infinity));
	Check(!ValidFrameMilliseconds(-Infinity));
	Check(!ValidFrameMilliseconds(ImplausibleFrameMilliseconds));
	Check(!ValidFrameMilliseconds(ImplausibleFrameMilliseconds + 1.0));

	// Budget validation.
	Check(ValidBudget(SixtyHertz()));
	{
		FFrameBudget Budget = SixtyHertz();
		Budget.TargetMilliseconds = 0.0;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.TargetMilliseconds = NaNValue;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.AllowedOverBudgetFraction = -0.01;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.AllowedOverBudgetFraction = 1.01;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.AllowedOverBudgetFraction = NaNValue;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.JudgedPercentile = 0.0;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.JudgedPercentile = 1.5;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.JudgedPercentile = NaNValue;
		Check(!ValidBudget(Budget));
		Budget = SixtyHertz();
		Budget.HardStallMilliseconds = NaNValue;
		Check(!ValidBudget(Budget));
		// A hard stall at or below target would fail frames that merely meet the target.
		Budget = SixtyHertz();
		Budget.HardStallMilliseconds = Budget.TargetMilliseconds;
		Check(!ValidBudget(Budget));
		Budget.HardStallMilliseconds = Budget.TargetMilliseconds - 1.0;
		Check(!ValidBudget(Budget));
		Budget.AllowedOverBudgetFraction = 0.0;
		Budget.HardStallMilliseconds = Budget.TargetMilliseconds + 0.0001;
		Check(ValidBudget(Budget));
		Budget.AllowedOverBudgetFraction = 1.0;
		Check(ValidBudget(Budget));
	}

	// Ring admission and warm-up gating.
	Check(!DropOldestBeforeAppend(0));
	Check(!DropOldestBeforeAppend(MaximumSamples - 1));
	Check(DropOldestBeforeAppend(MaximumSamples));
	Check(DropOldestBeforeAppend(MaximumSamples + 1));
	Check(!PastWarmup(0));
	Check(!PastWarmup(WarmupSamplesDiscarded - 1));
	Check(PastWarmup(WarmupSamplesDiscarded));
	Check(PastWarmup(WarmupSamplesDiscarded + 1));

	// Nearest-rank percentile: the result is always an observed sample, never a blend.
	{
		std::vector<double> Samples;
		for (int Value = 1; Value <= 100; ++Value) { Samples.push_back(static_cast<double>(Value)); }
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 0.01) == 1.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 0.50) == 50.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 0.95) == 95.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 0.99) == 99.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 1.00) == 100.0);
		// Out-of-range and non-finite fractions clamp rather than read out of bounds.
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 0.0) == 1.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), -1.0) == 1.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), 2.0) == 100.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), NaNValue) == 1.0);
		Check(PercentileOfSorted(Samples.data(), Samples.size(), Infinity) == 100.0);
		// Degenerate inputs.
		Check(PercentileOfSorted(nullptr, 0, 0.95) == 0.0);
		Check(PercentileOfSorted(nullptr, 10, 0.95) == 0.0);
		Check(PercentileOfSorted(Samples.data(), 0, 0.95) == 0.0);
		Check(PercentileOfSorted(Samples.data(), 1, 0.95) == 1.0);
	}
	// Every percentile of every size lands inside the sample set and never decreases.
	for (std::size_t Count = 1; Count <= 200; ++Count)
	{
		std::vector<double> Samples;
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			Samples.push_back(static_cast<double>(Index) + 1.0);
		}
		double Previous = 0.0;
		for (int Step = 1; Step <= 100; ++Step)
		{
			const double Fraction = static_cast<double>(Step) / 100.0;
			const double Value = PercentileOfSorted(Samples.data(), Count, Fraction);
			Check(Value >= 1.0 && Value <= static_cast<double>(Count));
			Check(Value >= Previous);
			Previous = Value;
		}
		Check(PercentileOfSorted(Samples.data(), Count, 1.0) == static_cast<double>(Count));
	}

	// Nearest-rank semantics pinned against an independent oracle. Ranks where Fraction*Count is
	// a whole number cannot distinguish ceiling from floor, so exact-multiple sizes alone leave the
	// rounding rule untested; these cases are chosen so the two disagree.
	{
		auto SampleAt = [](std::size_t Count, double Fraction)
		{
			std::vector<double> Samples;
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Samples.push_back(static_cast<double>(Index) + 1.0);
			}
			return PercentileOfSorted(Samples.data(), Count, Fraction);
		};
		Check(SampleAt(10, 0.95) == 10.0);   // 9.5  -> rank 10, not 9
		Check(SampleAt(10, 0.55) == 6.0);    // 5.5  -> rank 6, not 5
		Check(SampleAt(7, 0.50) == 4.0);     // 3.5  -> rank 4, not 3
		Check(SampleAt(3, 0.50) == 2.0);     // 1.5  -> rank 2, not 1
		Check(SampleAt(9, 0.95) == 9.0);     // 8.55 -> rank 9
		Check(SampleAt(4, 0.30) == 2.0);     // 1.2  -> rank 2, not 1
		Check(SampleAt(150, 0.95) == 143.0); // 142.5 -> rank 143, not 142
		Check(SampleAt(4, 0.25) == 1.0);     // 1.0 exactly -> rank 1
		Check(SampleAt(200, 0.99) == 198.0); // 198.0 exactly -> rank 198

		// The rule, asserted for every size and fraction against std::ceil.
		for (std::size_t Count = 1; Count <= 200; ++Count)
		{
			for (int Step = 1; Step <= 100; ++Step)
			{
				const double Fraction = static_cast<double>(Step) / 100.0;
				double ExpectedRank = std::ceil(Fraction * static_cast<double>(Count));
				if (ExpectedRank < 1.0) { ExpectedRank = 1.0; }
				if (ExpectedRank > static_cast<double>(Count)) { ExpectedRank = static_cast<double>(Count); }
				Check(SampleAt(Count, Fraction) == ExpectedRank);
			}
		}
	}

	// Over-budget counting and streaks.
	{
		const std::vector<double> Mixed = { 10.0, 20.0, 10.0, 20.0, 20.0, 20.0, 10.0 };
		Check(CountOverBudget(Mixed.data(), Mixed.size(), 16.6667) == 4);
		Check(CountOverBudget(Mixed.data(), Mixed.size(), 100.0) == 0);
		Check(CountOverBudget(Mixed.data(), Mixed.size(), 0.0) == Mixed.size());
		Check(CountOverBudget(nullptr, 5, 16.0) == 0);
		Check(LongestOverBudgetStreak(Mixed.data(), Mixed.size(), 16.6667) == 3);
		Check(LongestOverBudgetStreak(Mixed.data(), Mixed.size(), 100.0) == 0);
		Check(LongestOverBudgetStreak(Mixed.data(), Mixed.size(), 0.0) == Mixed.size());
		Check(LongestOverBudgetStreak(nullptr, 5, 16.0) == 0);
		// A streak that runs to the final sample must still be counted.
		const std::vector<double> Trailing = { 10.0, 10.0, 20.0, 20.0 };
		Check(LongestOverBudgetStreak(Trailing.data(), Trailing.size(), 16.6667) == 2);
		// Exactly meeting the target is not over budget.
		const std::vector<double> AtTarget(8, 16.6667);
		Check(CountOverBudget(AtTarget.data(), AtTarget.size(), 16.6667) == 0);
		Check(LongestOverBudgetStreak(AtTarget.data(), AtTarget.size(), 16.6667) == 0);
	}

	// Hard stall detection is inclusive of the threshold.
	{
		const FFrameBudget Budget = SixtyHertz();
		const std::vector<double> Clean(200, 16.0);
		Check(!HasHardStall(Clean.data(), Clean.size(), Budget));
		std::vector<double> Exact = Clean;
		Exact[100] = Budget.HardStallMilliseconds;
		Check(HasHardStall(Exact.data(), Exact.size(), Budget));
		std::vector<double> Below = Clean;
		Below[100] = Budget.HardStallMilliseconds - 0.0001;
		Check(!HasHardStall(Below.data(), Below.size(), Budget));
		Check(!HasHardStall(nullptr, 5, Budget));
	}

	// Verdicts. Absence of data is never a pass.
	{
		const FFrameBudget Budget = SixtyHertz();
		Check(Evaluate(nullptr, 0, Budget) == EVerdict::Insufficient);
		Check(Evaluate(nullptr, MinimumSamplesForVerdict, Budget) == EVerdict::Insufficient);
		const std::vector<double> JustShort(MinimumSamplesForVerdict - 1, 16.0);
		Check(Evaluate(JustShort.data(), JustShort.size(), Budget) == EVerdict::Insufficient);
		const std::vector<double> JustEnough(MinimumSamplesForVerdict, 16.0);
		Check(Evaluate(JustEnough.data(), JustEnough.size(), Budget) == EVerdict::Pass);

		// An unusable budget outranks sample count, and is never a pass or a fail.
		FFrameBudget Broken = Budget;
		Broken.TargetMilliseconds = -1.0;
		Check(Evaluate(JustEnough.data(), JustEnough.size(), Broken) == EVerdict::InvalidBudget);
		Check(Evaluate(nullptr, 0, Broken) == EVerdict::InvalidBudget);
		Check(!QualifiesCapture(EVerdict::InvalidBudget));
		Check(!QualifiesCapture(EVerdict::Insufficient));
		Check(QualifiesCapture(EVerdict::Pass));
		Check(QualifiesCapture(EVerdict::Fail));

		// One hard stall fails a capture whose percentiles are otherwise perfect.
		std::vector<double> OneStall(400, 8.0);
		OneStall[200] = Budget.HardStallMilliseconds + 1.0;
		const std::vector<double> StallSorted = SortedAscending(OneStall);
		Check(Evaluate(StallSorted.data(), StallSorted.size(), Budget) == EVerdict::Fail);

		// p95 over target fails even when most frames are comfortable.
		std::vector<double> HeavyTail(400, 8.0);
		for (std::size_t Index = 0; Index < 40; ++Index) { HeavyTail[Index] = 30.0; }
		const std::vector<double> TailSorted = SortedAscending(HeavyTail);
		Check(Evaluate(TailSorted.data(), TailSorted.size(), Budget) == EVerdict::Fail);

		// Over-budget share is judged at the boundary, not approximately.
		{
			FFrameBudget Share = SixtyHertz();
			Share.JudgedPercentile = 1.0;
			Share.TargetMilliseconds = 16.6667;
			Share.HardStallMilliseconds = 1000.0;
			Share.AllowedOverBudgetFraction = 0.10;
			std::vector<double> Samples(200, 8.0);
			for (std::size_t Index = 0; Index < 20; ++Index) { Samples[Index] = 20.0; }
			std::vector<double> Sorted = SortedAscending(Samples);
			// p100 exceeds the target, so judging at 1.0 must fail regardless of share.
			Check(Evaluate(Sorted.data(), Sorted.size(), Share) == EVerdict::Fail);
			// Judged at p50 the target is met, and 20/200 = exactly the allowed share.
			Share.JudgedPercentile = 0.50;
			Check(Evaluate(Sorted.data(), Sorted.size(), Share) == EVerdict::Pass);
			// One more over-budget frame crosses the allowance.
			Samples[20] = 20.0;
			Sorted = SortedAscending(Samples);
			Check(Evaluate(Sorted.data(), Sorted.size(), Share) == EVerdict::Fail);
		}
	}

	// Property sweep: no sample count below the minimum may ever qualify as a pass, for any budget.
	{
		const std::vector<double> Budgets = { 16.6667, 33.3333, 8.3333 };
		for (double Target : Budgets)
		{
			FFrameBudget Budget;
			Budget.TargetMilliseconds = Target;
			Budget.HardStallMilliseconds = Target * 6.0;
			Budget.AllowedOverBudgetFraction = 1.0;
			Budget.JudgedPercentile = 1.0;
			for (std::size_t Count = 0; Count < MinimumSamplesForVerdict; ++Count)
			{
				const std::vector<double> Samples(Count, Target * 0.25);
				Check(Evaluate(Samples.empty() ? nullptr : Samples.data(), Count, Budget) == EVerdict::Insufficient);
			}
			const std::vector<double> Enough(MinimumSamplesForVerdict, Target * 0.25);
			Check(Evaluate(Enough.data(), Enough.size(), Budget) == EVerdict::Pass);
		}
	}

	// Report field safety: the export writes JSON without a library, so anything that could
	// introduce a quote, a backslash or a control character must be rejected here.
	{
		for (char Character = 'a'; Character <= 'z'; ++Character) { Check(ReportSafeCharacter(Character)); }
		for (char Character = 'A'; Character <= 'Z'; ++Character) { Check(ReportSafeCharacter(Character)); }
		for (char Character = '0'; Character <= '9'; ++Character) { Check(ReportSafeCharacter(Character)); }
		Check(ReportSafeCharacter('.'));
		Check(ReportSafeCharacter('_'));
		Check(ReportSafeCharacter('-'));
		Check(!ReportSafeCharacter('"'));
		Check(!ReportSafeCharacter('\\'));
		Check(!ReportSafeCharacter('\n'));
		Check(!ReportSafeCharacter('\r'));
		Check(!ReportSafeCharacter('\t'));
		Check(!ReportSafeCharacter('\0'));
		Check(!ReportSafeCharacter(' '));
		Check(!ReportSafeCharacter('{'));
		Check(!ReportSafeCharacter('}'));
		Check(!ReportSafeCharacter(':'));
		Check(!ReportSafeCharacter(','));
		Check(!ReportSafeCharacter('/'));
		// Every printable ASCII character is either permitted or rejected, never undefined, and no
		// permitted character is one JSON would need escaped.
		for (int Code = 32; Code < 127; ++Code)
		{
			const char Character = static_cast<char>(Code);
			const bool bSafe = ReportSafeCharacter(Character);
			if (bSafe)
			{
				Check(Character != '"' && Character != '\\');
				Check(Code >= 32 && Code < 127);
			}
		}
	}

	std::cout << Checks << " performance capture policy checks passed\n";
	// Load memory trend: three consecutive rises beyond tolerance grow; noise and interruptions do not.
	{
		const double Megabyte = 1024.0 * 1024.0;
		const double Steady[] = {3000 * Megabyte, 3010 * Megabyte, 2995 * Megabyte, 3020 * Megabyte};
		Check(EvaluateLoadMemory(Steady, 4, 64 * Megabyte) == EMemoryTrend::Stable);
		const double Rising[] = {3000 * Megabyte, 3100 * Megabyte, 3200 * Megabyte};
		Check(EvaluateLoadMemory(Rising, 3, 64 * Megabyte) == EMemoryTrend::Growing);
		const double Interrupted[] = {3000 * Megabyte, 3100 * Megabyte, 3050 * Megabyte, 3150 * Megabyte};
		Check(EvaluateLoadMemory(Interrupted, 4, 64 * Megabyte) == EMemoryTrend::Stable);
		Check(EvaluateLoadMemory(Rising, 2, 64 * Megabyte) == EMemoryTrend::Insufficient);
		const double Corrupt[] = {3000 * Megabyte, NaNValue, 3200 * Megabyte};
		Check(EvaluateLoadMemory(Corrupt, 3, 64 * Megabyte) == EMemoryTrend::Insufficient);
		const double Zero[] = {3000 * Megabyte, 0.0, 3200 * Megabyte};
		Check(EvaluateLoadMemory(Zero, 3, 64 * Megabyte) == EMemoryTrend::Insufficient);
		Check(EvaluateLoadMemory(Rising, 3, -1.0) == EMemoryTrend::Insufficient);
		Check(EvaluateLoadMemory(Rising, 3, NaNValue) == EMemoryTrend::Insufficient);
		Check(EvaluateLoadMemory(nullptr, 3, 64 * Megabyte) == EMemoryTrend::Insufficient);
	}

	return 0;
}
