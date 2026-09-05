#include "Save/SovSavePolicy.h"
#include <cassert>
#include <iostream>
using namespace SovSavePolicy;
int main()
{
    for (int Index = -2; Index < 14; ++Index)
    {
        assert(ValidSlot(Kind::Manual, Index) == (Index >= 0 && Index < 10));
        assert(ValidSlot(Kind::Auto, Index) == (Index >= 0 && Index < 3));
        assert(ValidSlot(Kind::Checkpoint, Index) == (Index == 0));
    }
    assert(!ValidSlot(static_cast<Kind>(255), 0));
    assert(CheckVersion(1, 0, true, true) == Compatibility::Compatible);
    assert(CheckVersion(0, 0, true, true) == Compatibility::Legacy);
    assert(CheckVersion(1, 1, true, true) == Compatibility::NewerSchema);
    assert(CheckVersion(2, 0, true, true) == Compatibility::NewerSchema);
    assert(CheckVersion(1, 0, false, true) == Compatibility::DifferentProduct);
    assert(CheckVersion(1, 0, true, false) == Compatibility::WrongAccount);
    int Target = -1; std::int64_t Generation = 0;
    assert(NextWrite({}, {}, Target, Generation) && Target == 0 && Generation == 1);
    assert(NextWrite({true, 4}, {true, 3}, Target, Generation) && Target == 1 && Generation == 5);
    assert(NextWrite({true, 4}, {false, 99}, Target, Generation) && Target == 1 && Generation == 5);
    assert(LatestBank({false, 99}, {true, 3}) == 1);
    assert(LatestBank({true, 0}, {true, -8}) == -1);
    assert(!NextWrite({true, std::numeric_limits<std::int64_t>::max()}, {}, Target, Generation));
    Bank Slots[AutoSlots] {};
    for (int Save = 0; Save < 100; ++Save)
    {
        const int Index = OldestAuto(Slots);
        assert(Index == Save % AutoSlots);
        Slots[Index] = {true, Save + 1};
    }
    // Exhaustive admission: seven positive requirements, eight blockers.
    int Admitted = 0;
    for (unsigned Mask = 0; Mask < (1u << 15); ++Mask)
    {
        auto Bit = [Mask](int Index) { return (Mask & (1u << Index)) != 0; };
        Admission A;
        A.Authority=Bit(0); A.Standalone=Bit(1); A.Ready=Bit(2); A.Alive=Bit(3);
        A.StableIdentity=Bit(4); A.Grounded=Bit(5); A.StateValid=Bit(6);
        A.InCombat=Bit(7); A.InCinematic=Bit(8); A.InTraversal=Bit(9); A.UnresolvedChoice=Bit(10);
        A.Mutating=Bit(11); A.SavingDisabled=Bit(12); A.NearbyThreat=Bit(13);
        // Bit 14 is intentionally irrelevant presentation state.
        if (CanCapture(A)) { ++Admitted; assert((Mask & 0x3fffu) == 0x7fu); }
    }
    assert(Admitted == 2);
    std::cout << "Save policy: 32768 admission combinations, 100 rotations, bank/schema/slot boundaries passed\n";
}
