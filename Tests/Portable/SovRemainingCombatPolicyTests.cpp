#include "Combat/SovFinisherPolicy.h"
#include "GAS/SovDamageChannelPolicy.h"
#include "Projectiles/SovProjectileDefensePolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main()
{
    using SovDamageChannels::FPortion;
    FPortion Mixed[]={{1.,true,1.},{1.,false,1.}};
    assert(SovDamageChannels::Resolve(Mixed,2)==.5);
    Mixed[0].Weight=3.; assert(SovDamageChannels::Resolve(Mixed,2)==.25);
    Mixed[1].Multiplier=.4; assert(std::abs(SovDamageChannels::Resolve(Mixed,2)-.1)<1e-9);
    Mixed[1].Immune=true; assert(SovDamageChannels::Resolve(Mixed,2)==0.);
    FPortion Single{1.,false,.75}; assert(SovDamageChannels::Resolve(&Single,1)==.75);
    Single.Weight=0.; assert(SovDamageChannels::Resolve(&Single,1)==0.);
    Single.Weight=-1.; assert(SovDamageChannels::Resolve(&Single,1)==0.);
    Single.Weight=std::numeric_limits<double>::quiet_NaN(); assert(SovDamageChannels::Resolve(&Single,1)==0.);
    assert(SovDamageChannels::Resolve(nullptr,0)==1.);
    assert(SovFinisher::Vulnerable(true,true,100,100,0));
    assert(SovFinisher::Vulnerable(true,false,20,100,.2));
    assert(!SovFinisher::Vulnerable(true,false,21,100,.2));
    assert(!SovFinisher::Vulnerable(false,true,100,100,1));
    assert(!SovFinisher::Vulnerable(true,true,0,100,1));
    assert(!SovFinisher::Vulnerable(true,false,100,100,-1));
    assert(SovFinisher::StrikeDamage(70,25,true,true)==70);
    assert(SovFinisher::StrikeDamage(70,25,true,false)==25);
    assert(SovFinisher::StrikeDamage(10,25,false,true)==9);
    assert(SovFinisher::StrikeDamage(1,25,false,false)==0);
    assert(SovFinisher::StrikeDamage(10,-25,true,false)==0);
    assert(SovFinisher::Duration(9,true)==1.8);
    assert(SovFinisher::Duration(.1,true)==.8);
    assert(SovFinisher::Duration(1.8,false)==.35);
    using namespace SovProjectileDefense;
    assert(Resolve(false,true,true,false,0,1)==EOutcome::Explode);
    assert(Resolve(true,false,true,false,0,1)==EOutcome::Explode);
    assert(Resolve(true,true,false,true,0,1)==EOutcome::Absorb);
    assert(Resolve(true,true,true,false,0,1)==EOutcome::Reflect);
    assert(Resolve(true,true,true,false,1,1)==EOutcome::Absorb);
    assert(Resolve(true,true,false,false,0,1)==EOutcome::Explode);
    std::cout << "Remaining combat policies: 29 assertions passed\n";
}
