#include "World/SovWorldMotionPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    using namespace SovWorldMotionPolicy;
    unsigned Checks = 0;
    for (int Rate : {30, 60, 120})
    {
        for (int Start=0; Start<100; ++Start)
        {
            double Time = 0.; const double Duration = .1 + Start*.03;
            int Frames = 0;
            while (Time < Duration && Frames < 10000)
            { const double Before=Time; Time=Advance(Time,1./Rate,Duration); assert(Time>Before && Time<=Duration); ++Frames; ++Checks; }
            assert(Time==Duration && Frames <= static_cast<int>(std::ceil(Duration*Rate))+1); ++Checks;
        }
    }
    assert(Advance(.2,2.,1.)==1.); ++Checks;
    assert(Advance(.2,-1.,1.)==.2); ++Checks;
    assert(Advance(.2,std::numeric_limits<double>::quiet_NaN(),1.)==.2); ++Checks;
    for (unsigned Flags=0; Flags<16; ++Flags)
    {
        const bool Mutating=Flags&1, Moving=Flags&2, Waiting=Flags&4, Stable=Flags&8;
        assert(CanSaveEndpoint(Mutating,Moving,Waiting,Stable)==(Flags==8)); ++Checks;
        assert(CanEnableLink(Mutating,Moving,Waiting,Stable)==(Flags==15)); ++Checks;
    }
    for (int Rate : {30,60,120})
    {
        double Now=10.; int Frames=0;
        while(!TimedOut(Now,10.,20.)) { Now+=1./Rate; ++Frames; }
        assert(Frames<=20*Rate+1); ++Checks;
    }
    assert(TimedOut(9.,10.,20.)); ++Checks;
    assert(TimedOut(std::numeric_limits<double>::quiet_NaN(),10.,20.)); ++Checks;
    assert(!TimedOut(29.99,10.,20.)); ++Checks;
    std::cout << "PASS: " << Checks << " production world-motion timing/admission checks\n";
}
