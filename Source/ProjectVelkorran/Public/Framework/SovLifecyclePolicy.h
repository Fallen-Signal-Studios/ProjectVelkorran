// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>

/** Overlapping platform interruptions latch until the player explicitly resumes. */
namespace SovLifecyclePolicy
{
enum class Reason : std::uint8_t { Background = 1, Inactive = 2, Overlay = 4, Controller = 8, Account = 16 };
class State
{
public:
    bool IsInterrupted() const { return AwaitingResume; }
    bool HasReason(Reason Value) const { return (Reasons & static_cast<std::uint8_t>(Value)) != 0; }
    bool IsApplicationUnavailable() const { return (Reasons & 7u) != 0; }
    bool CanResume() const { return AwaitingResume && Reasons == 0; }
    void SetReason(Reason Value, bool Active, double Now)
    {
        if (!std::isfinite(Now)) { return; }
        if (Active)
        {
            if (!AwaitingResume) { Started = Now; AwaitingResume = true; }
            Reasons |= static_cast<std::uint8_t>(Value);
        }
        else { Reasons &= static_cast<std::uint8_t>(~static_cast<std::uint8_t>(Value)); }
    }
    bool Resume(double Now)
    {
        if (!CanResume() || !std::isfinite(Now) || Now < Started) { return false; }
        Excluded += Now - Started; AwaitingResume = false; return true;
    }
    double ActiveTime(double Now) const
    {
        if (!std::isfinite(Now)) { return 0.; }
        return (AwaitingResume && Now >= Started ? Started : Now) - Excluded;
    }
private:
    std::uint8_t Reasons = 0;
    bool AwaitingResume = false;
    double Started = 0.;
    double Excluded = 0.;
};
}
