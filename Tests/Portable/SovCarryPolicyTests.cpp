#include "World/SovCarryPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    unsigned Checks=0;
    for (unsigned Bits=0;Bits<32;++Bits)
    { assert(SovCarryPolicy::CanRescue(Bits&1,Bits&2,Bits&4,Bits&8,Bits&16)==(Bits==31)); ++Checks; }
    for (int Radius=0;Radius<=151;++Radius)
    { for (int Height=0;Height<=221;++Height)
      { assert(SovCarryPolicy::ValidBody(Radius,Height)==(Radius>0 && Radius<=150 && Height>=Radius && Height<=220)); ++Checks; } }
    assert(!SovCarryPolicy::ValidBody(std::numeric_limits<double>::quiet_NaN(),80)); ++Checks;
    assert(!SovCarryPolicy::ValidBody(20,std::numeric_limits<double>::infinity())); ++Checks;
    std::cout << "PASS: " << Checks << " carry geometry and rescue admission checks\n";
}
