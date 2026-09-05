// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovLifecyclePolicy.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    using namespace SovLifecyclePolicy;
    State S;
    assert(!S.IsInterrupted() && !S.CanResume() && !S.Resume(5.));
    S.SetReason(Reason::Background, true, 10.);
    S.SetReason(Reason::Background, true, 11.); // Duplicate platform events cannot reset the clock.
    S.SetReason(Reason::Overlay, true, 15.);
    S.SetReason(Reason::Background, false, 100000.);
    assert(S.IsInterrupted() && S.IsApplicationUnavailable() && !S.Resume(100001.));
    assert(S.ActiveTime(100001.) == 10.);
    S.SetReason(Reason::Overlay, false, 100002.);
    assert(S.CanResume() && S.IsInterrupted()); // Foreground does not resume combat.
    S.SetReason(Reason::Controller, true, 100003.);
    S.SetReason(Reason::Account, true, 100004.);
    S.SetReason(Reason::Controller, false, 100005.);
    assert(!S.Resume(100006.)); // A different connected player cannot bypass account ownership.
    S.SetReason(Reason::Account, false, 100007.);
    assert(S.Resume(100008.) && !S.Resume(100008.));
    assert(S.ActiveTime(100009.) == 11.);
    S.SetReason(Reason::Inactive, true, 100010.);
    S.SetReason(Reason::Inactive, false, 100020.);
    assert(S.Resume(100030.) && S.ActiveTime(100031.) == 13.);
    const double Bad = std::numeric_limits<double>::quiet_NaN();
    S.SetReason(Reason::Background, true, Bad); assert(!S.IsInterrupted());
    S.SetReason(Reason::Background, true, 100040.); S.SetReason(Reason::Background, false, 100041.);
    assert(!S.Resume(Bad) && !S.Resume(100039.) && S.IsInterrupted());

    // Exhaust every overlapping reason subset and each possible last cleared reason.
    constexpr std::array<Reason, 5> Reasons{Reason::Background, Reason::Inactive, Reason::Overlay, Reason::Controller, Reason::Account};
    unsigned Checks = 0;
    for (unsigned Mask = 1; Mask < 32; ++Mask)
    {
        for (unsigned Last = 0; Last < Reasons.size(); ++Last)
        {
            if ((Mask & (1u << Last)) == 0) { continue; }
            State Combined;
            for (unsigned I = 0; I < Reasons.size(); ++I)
            { if (Mask & (1u << I)) { Combined.SetReason(Reasons[I], true, 20.); } }
            for (unsigned I = 0; I < Reasons.size(); ++I)
            { if (I != Last) { Combined.SetReason(Reasons[I], false, 40.); } }
            assert(!Combined.Resume(50.) && Combined.ActiveTime(50.) == 20.);
            Combined.SetReason(Reasons[Last], false, 60.);
            assert(Combined.IsInterrupted() && Combined.CanResume() && Combined.ActiveTime(1000000.) == 20.);
            assert(Combined.Resume(1000000.) && Combined.ActiveTime(1000005.) == 25.);
            ++Checks;
        }
    }
    std::cout << "SovLifecyclePolicy: overlapping interruptions, duplicate events, explicit resume, invalid clocks and " << Checks << " reason-order cases passed\n";
}
