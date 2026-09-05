// Copyright Fallen Signal Studios. All Rights Reserved.
#include "../../Source/ProjectVelkorran/Private/Platform/SovPlatformServicesPolicy.h"
#include <cassert>
int main()
{
    using namespace SovPlatformServicesPolicy;
    for (int Bits = 0; Bits < 8; ++Bits)
        for (int User = -1; User <= 4; ++User)
            assert(CanUseGenericCloud(Bits & 1, Bits & 2, Bits & 4, User) == (Bits == 7 && User >= 0));
    for (int Bits = 0; Bits < 16; ++Bits)
        for (int User = -1; User <= 4; ++User)
        {
            const bool Known = Bits & 1, ActiveProfile = Bits & 2, MappedOwner = Bits & 4, RequiresOwner = Bits & 8;
            assert(CanAuthorizeStorage(RequiresOwner, Known, ActiveProfile, MappedOwner, User)
                == (!RequiresOwner || (Known && ActiveProfile && MappedOwner && User >= 0)));
        }
    for (int Bits = 0; Bits < 64; ++Bits)
    {
        const bool OptIn = Bits & 1, SignedIn = Bits & 2, Available = Bits & 4;
        const bool SameAccount = Bits & 8, Frontend = Bits & 16, Busy = Bits & 32;
        assert(CanAdmit(OptIn, SignedIn, Available, SameAccount, Frontend, Busy) == (Bits == 31));
    }
    for (int Bits = 0; Bits < 16; ++Bits)
        assert(CanConsume(Bits & 1, Bits & 2, Bits & 4, Bits & 8) == (Bits == 15));
    for (int Kind = -1; Kind < 4; ++Kind)
        for (int Slot = -1; Slot < 12; ++Slot)
            assert(ValidSlot(Kind, Slot) == (Slot >= 0 && ((Kind == 0 && Slot < 10) || (Kind == 1 && Slot < 3) || (Kind == 2 && Slot == 0))));
    assert(CanPublishRevision(0, 1)); assert(CanPublishRevision(31, MaximumEnvelopeBytes));
    assert(!CanPublishRevision(-1, 1)); assert(!CanPublishRevision(32, 1));
    assert(!CanPublishRevision(0, 0)); assert(!CanPublishRevision(0, MaximumEnvelopeBytes + 1));
    return 0;
}
