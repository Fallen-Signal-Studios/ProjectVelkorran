#include "Campaign/SovResourceSnapshotPolicy.h"
#include "Combat/SovFinisherPolicy.h"
#include <cassert>
#include <limits>
int main()
{
    using namespace SovResourceSnapshotPolicy;
    assert(ValidBase(-10.,100.));
    assert(!ValidBase(std::numeric_limits<double>::infinity(),100.));
    assert(!ValidBase(10.,-1.));
    double Base=50.;
    for (int I=0; I<100; ++I)
    {
        Base=RestoredBase(Base,100.,100.);
        assert(Resolved(Base,-10.,100.)==40.);
    }
    assert(Resolved(Base,0.,100.)==50.);
    assert(RestoredBase(100.,100.,100.)==100.);
    assert(Resolved(RestoredBase(100.,100.,100.),20.,100.)==100.);
    assert(Resolved(RestoredBase(100.,100.,100.),0.,100.)==100.);
    assert(RestoredBase(50.,100.,30.)==30.);
    assert(RestoredBase(50.,100.,200.)==50.);
    assert(CanRestoreLegacy(false)); assert(!CanRestoreLegacy(true));
    assert(!SovFinisher::CanCommitOutcome(false,true,true,false));
    assert(!SovFinisher::CanCommitOutcome(true,false,true,false));
    assert(!SovFinisher::CanCommitOutcome(true,true,false,false));
    assert(!SovFinisher::CanCommitOutcome(true,true,true,true));
    assert(SovFinisher::CanCommitOutcome(true,true,true,false));
}
