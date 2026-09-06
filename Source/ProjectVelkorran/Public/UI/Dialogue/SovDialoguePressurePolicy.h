// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SovDialoguePressure
{
	enum class Mode : uint8_t { Standard, Extended, Disabled };
	/** Pure elapsed-active-time policy. No wall clocks, engine timers, or fabricated speech durations. */
	struct State
	{
		uint64_t Revision = 0;
		double Duration = 0.;
		double MinimumRead = 5.;
		double ReadElapsed = 0.;
		double PressureElapsed = 0.;
		bool Active = false;
		bool TextReady = false;
		bool SpeechRequired = false;
		bool SpeechComplete = false;
		bool Committed = false;
		void Begin(uint64_t InRevision, double AuthoredDuration, bool ValidSilence, Mode Timing,
			double Extension, double ReadSeconds, bool RequireSpeech)
		{
			*this = State();
			Revision = InRevision;
			Active = true;
			SpeechRequired = RequireSpeech;
			MinimumRead = std::isfinite(ReadSeconds) ? std::clamp(ReadSeconds, 2., 30.) : 5.;
			if (ValidSilence && Timing != Mode::Disabled && std::isfinite(AuthoredDuration)
				&& AuthoredDuration > 0. && AuthoredDuration <= 300.)
			{
				Duration = AuthoredDuration;
				if (Timing == Mode::Extended)
				{
					Duration *= std::isfinite(Extension) ? std::clamp(Extension, 1., 5.) : 2.;
				}
			}
		}
		bool Ready() const { return TextReady && (!SpeechRequired || SpeechComplete) && ReadElapsed >= MinimumRead; }
		void SetSpeechComplete(uint64_t InRevision, bool Complete)
		{ if (Active && Revision == InRevision) { SpeechComplete = Complete; } }
		void SuspendReading() { TextReady = false; SpeechComplete = false; }
		void Cancel() { Active = false; TextReady = false; SpeechComplete = false; }
		bool TryCommit(uint64_t InRevision)
		{
			if (!Active || Committed || Revision != InRevision) { return false; }
			Committed = true; Active = false; return true;
		}
		bool Advance(uint64_t InRevision, double Delta, bool Paused)
		{
			if (!Active || Committed || Revision != InRevision || Paused || !TextReady
				|| !std::isfinite(Delta) || Delta <= 0.) { return false; }
			const bool WasReady = Ready();
			ReadElapsed += Delta;
			if (!WasReady || Duration <= 0.) { return false; }
			PressureElapsed += Delta;
			return PressureElapsed >= Duration;
		}
		double Remaining() const { return std::max(0., Duration - PressureElapsed); }
	};
}
