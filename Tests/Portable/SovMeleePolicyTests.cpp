#include "Melee/SovMeleePolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main()
{
    assert(SovMelee::ValidWindows(.15,.2,.3,.35,.6));
    assert(!SovMelee::ValidWindows(.15,-.2,.3,.35,.6));
    assert(!SovMelee::ValidWindows(.15,.2,.3,.2,.6));
    assert(!SovMelee::ValidWindows(.15,.2,.3,.35,.8));
    assert(!SovMelee::ValidWindows(1,3,2,4,4));
    assert(SovMelee::ValidFollowUp(0,1,3));
    assert(!SovMelee::ValidFollowUp(1,1,3));
    assert(!SovMelee::ValidFollowUp(2,0,3));
    assert(!SovMelee::ValidFollowUp(0,3,3));
    assert(!SovMelee::ValidFollowUp(0,1,9));
    assert(SovMelee::SpatialSamples(100,8)==14);
    assert(SovMelee::SpatialSamples(500,2)==251);
    assert(SovMelee::SpatialSamples(501,2)==0);
    assert(SovMelee::SpatialSamples(100,0)==0);
    assert(SovMelee::AimCorrection(30,12,.5)==6);
    assert(SovMelee::AimCorrection(-30,12,1)==-12);
    assert(SovMelee::AimCorrection(10,12,0)==0);
    assert(SovMelee::AimCorrection(std::numeric_limits<double>::quiet_NaN(),12,1)==0);
    assert(SovMelee::TemporalSamples(0,0,8)==1);
    assert(SovMelee::TemporalSamples(90,20,8)==12);
    assert(SovMelee::TemporalSamples(0,320,8)==10);
    assert(SovMelee::TemporalSamples(180,1500,2)==24);
    assert(SovMelee::TemporalSamples(0,1501,8)==0);
    assert(SovMelee::TemporalSamples(181,10,8)==0);
    assert(SovMelee::TemporalSamples(0,10,0)==0);
    assert(SovMelee::TemporalSamples(0,std::numeric_limits<double>::infinity(),8)==0);
    std::cout<<"Native melee policies: 26 assertions passed\n";
}
